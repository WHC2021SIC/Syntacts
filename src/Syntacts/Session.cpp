#ifndef PA_USE_ASIO
    #define PA_USE_ASIO 1
#endif

#include "Util/Util.hpp"
#include "Util/SPSCQueue.hpp"
#include <Syntacts/Session.hpp>
#include <cassert>
#include "portaudio.h"
#include "pa_asio.h"
#include "pa_win_wasapi.h"
#include "pa_win_ds.h"
#include <algorithm>
#include <functional>
#include <thread>
#include <iostream>
#include <fstream>
#include "rubberband/RubberBandStretcher.h"
#include <set>
#include <numeric>
#include "Util/Util.hpp"

namespace tact {

//=============================================================================
// IMPLEMENTATION
//=============================================================================

namespace {

constexpr int    QUEUE_SIZE        = 256;
constexpr int    FRAMES_PER_BUFFER = 0;

static std::vector<double> STANDARD_SAMPLE_RATES = {
    8000.0, 9600.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0, 32000.0,
    44100.0, 48000.0, 88200.0, 96000.0, 192000.0
};

using namespace rigtorp;

/// Channel structure
class Channel {
public:
    //Channel() : m_stretcher(SAMPLE_RATE, 1, RubberBand::RubberBandStretcher::OptionProcessRealTime) {}
    Ptr<Cue> cue;
    double   time   = 0.0; 
    double   sampleLength = 0.0;
    float    volume = 1.0f;
    float    lastVolume = 1.0f;
    float    pitch  = 1.0f;
    bool     paused = true;
    float nextSample() {
        float sample = 0;
        if (!paused) {
            if (time >= 0 && time <= cue->getEnvelope()->getDuration())
                sample = volume * cue->sample(static_cast<float>(time));
            time += sampleLength;
        }
        return sample;
    }
    void fillBuffer(float* buffer, unsigned long frames) {
        float nextVolume = volume;
        float volumeIncr = (nextVolume - lastVolume) / frames;
        volume = lastVolume;        
        for (unsigned long f = 0; f < frames; f++) {
            volume += volumeIncr;
            buffer[f] = nextSample();
        }
        volume     = nextVolume;
        lastVolume = nextVolume;
    }
private:
    //RubberBand::RubberBandStretcher m_stretcher;
};

/// Interface for commands sent through command queue
struct Command {
    int channel;
    virtual void perform(Channel& channel) = 0;
};

/// Commands to play a new cue
struct Play : public Command {
    virtual void perform(Channel& channel) override {
        channel.paused = false;
        channel.time   = -inSeconds;
        channel.cue    = cue;
    }
    Ptr<Cue> cue;
    double inSeconds;
};

/// Command to stop a channel
struct Stop : public Command {
    virtual void perform(Channel& channel) override {
        channel.paused = true;
        channel.time   = 0;
        channel.cue    = create<Cue>();
    }
};

/// Command to pause a channel
struct Pause : public Command {
    virtual void perform(Channel& channel) override {
        channel.paused = paused;
    }
    bool paused;
};

struct Volume : public Command {
    virtual void perform(Channel& channel) override {
        channel.volume = volume;
    }
    float volume;
};

} // private namespace

/// Sesssion Implementation
class Session::Impl {
public:

    Impl() :
        m_stream(nullptr),
        m_commands(QUEUE_SIZE),
        m_device({-1,"N/A",false,-1,"N/A",false,0})
    {
        // initialize PortAudio
        int result = Pa_Initialize();
        assert(result == paNoError);
        // build list of available devices
        for (int i = 0; i < Pa_GetDeviceCount(); ++i) {
            if (Pa_GetDeviceInfo(i)->maxOutputChannels > 0) {   
                m_devices.emplace(i, makeDevice(i));
            }   
        }
        // clean up devices
        correctMMENames();
        removeDigitalDevices();
        tidyNames();
        s_count++;
    }

    ~Impl() {
        if (isOpen())
            close();
        int result = Pa_Terminate();
        assert(result == paNoError);
        s_count--;
    }

    int open(const Device& device, int channels, double sampleRate) {

        // return if already open
        if (isOpen())
            return SyntactsError_AlreadyOpen;

        // set device
        m_device = device;

        // generat list of channel numbers
        std::vector<int> channelNumbers(channels);
        std::iota(channelNumbers.begin(), channelNumbers.end(), 0);

        // TODO
        PaWasapiStreamInfo wasapiInfo ;
        wasapiInfo.size = sizeof(PaWasapiStreamInfo);
        wasapiInfo.hostApiType = paWASAPI;
        wasapiInfo.version = 1;
        wasapiInfo.flags = paWinWasapiExclusive;   
        wasapiInfo.channelMask = NULL;
        wasapiInfo.hostProcessorOutput = NULL;
        wasapiInfo.hostProcessorInput = NULL;
        wasapiInfo.threadPriority = eThreadPriorityProAudio;

        // TODO
        PaAsioStreamInfo asioInfo;
        asioInfo.channelSelectors = &channelNumbers[0];
        asioInfo.size = sizeof(PaAsioStreamInfo);
        asioInfo.version = 1;
        asioInfo.hostApiType = paASIO;
        asioInfo.flags = 0;

        PaStreamParameters params;
        params.device = device.index;
        params.channelCount = channels;
        params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;
        params.hostApiSpecificStreamInfo = nullptr;
        params.sampleFormat = paFloat32 | paNonInterleaved;

        m_sampleRate = sampleRate == 0 ? Pa_GetDeviceInfo(params.device)->defaultSampleRate : sampleRate;

        if (Pa_IsFormatSupported(nullptr, &params, m_sampleRate) != paFormatIsSupported)
            return SyntactsError_InvalidSampleRate;

        // resize vector of channels
        m_channels.resize(channels);
        for (auto& c : m_channels) {
            c.cue = create<Cue>();
            c.sampleLength = 1.0 / m_sampleRate;
        }
        // open stream
        int result;
        result = Pa_OpenStream(&m_stream, nullptr, &params, m_sampleRate, FRAMES_PER_BUFFER, paNoFlag, callback, this);
        if (result != paNoError)
            return result;  
        result = Pa_StartStream(m_stream);
        if (result != paNoError)
            return result;
        return SyntactsError_NoError;
    }

    int close() {
        if (!isOpen())
            return SyntactsError_NotOpen;
        Pa_CloseStream(m_stream);
        m_device = {-1,"N/A",false,-1,"N/A",false,0};
        m_channels.clear();
        m_sampleRate = 0;
        return SyntactsError_NoError;
    }

    bool isOpen() const {
        return Pa_IsStreamActive(m_stream) == 1;
    }

    int play(int channel, Ptr<Cue> cue, double inSeconds) {
        if (!isOpen())
            return SyntactsError_NotOpen;
        if (!(channel < m_channels.size()))
            return SyntactsError_InvalidChannel;
        auto command = create<Play>();
        command->cue = std::move(cue);
        command->channel = channel;
        command->inSeconds = inSeconds;
        bool success = m_commands.try_push(std::move(command));
        assert(success);
        return SyntactsError_NoError;
    }

    int stop(int channel) {
        if (!isOpen())
            return SyntactsError_NotOpen;
        if (!(channel < m_channels.size()))
            return SyntactsError_InvalidChannel;
        auto command = create<Stop>();
        command->channel = channel;   
        bool success = m_commands.try_push(std::move(command));
        assert(success);
        return SyntactsError_NoError;     
    }

    int pause(int channel, bool paused) {
        if (!isOpen())
            return SyntactsError_NotOpen;
        if (!(channel < m_channels.size()))
            return SyntactsError_InvalidChannel;
        auto command = create<Pause>();
        command->channel = channel;   
        command->paused  = paused;
        bool success = m_commands.try_push(std::move(command));
        assert(success);
        return SyntactsError_NoError;       
    }

    int setVolume(int channel, float volume) {
        if (!isOpen())
            return SyntactsError_NotOpen;
        if (!(channel < m_channels.size()))
            return SyntactsError_InvalidChannel;
        auto command = create<Volume>();
        command->channel = channel;
        command->volume  = clamp01(volume);
        bool success = m_commands.try_push(std::move(command));
        assert(success);
        return SyntactsError_NoError; 
    }

    const Device& getCurrentDevcice() const {
        return m_device;
    }

    const Device& getDefaultDevice() const {
        int default = Pa_GetDefaultOutputDevice();
        assert(default != paNoDevice);
        return m_devices.at(default);
    }

    const std::map<int, Device>& getAvailableDevices() const {
        return m_devices;
    }

    int getChannelCount() const {
        if (isOpen())
            return (int)m_channels.size();
        else
            return 0;
    }

    double getSampleRate() const {
        return m_sampleRate;
    }

    double getCpuLoad() const {
        if (isOpen())
            return Pa_GetStreamCpuLoad(m_stream);
        return 0;
    }

    static int count() {
        return s_count;
    }

    void performCommands() {
        while (m_commands.front()) {
            auto command = *m_commands.front();
            command->perform(m_channels[command->channel]);
            m_commands.pop();
        }
    }

    static int callback(const void *inputBuffer, void *outputBuffer,
                 unsigned long framesPerBuffer,
                 const PaStreamCallbackTimeInfo *timeInfo,
                 PaStreamCallbackFlags statusFlags,
                 void *userData)
    {
        Session::Impl* session = (Session::Impl*)userData;
        auto& channels = session->m_channels;
        session->performCommands();
        (void)inputBuffer;     
        float** out = (float**)outputBuffer;
        for (std::size_t c = 0; c < channels.size(); ++c) {
            channels[c].fillBuffer(out[c], framesPerBuffer);
        }
        return paContinue;
    }

    void openControlPanel(int index) {
        PaAsio_ShowControlPanel(index, nullptr);
    }

    Device makeDevice(int index) {
    auto pa_dev_info = Pa_GetDeviceInfo(index);
    auto pa_api_info = Pa_GetHostApiInfo(pa_dev_info->hostApi);

    std::vector<double> sampleRates;
    sampleRates.reserve(STANDARD_SAMPLE_RATES.size());

    PaStreamParameters params;
    params.device = index;
    params.channelCount = pa_dev_info->maxOutputChannels;
    params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;
    params.hostApiSpecificStreamInfo = nullptr;
    params.sampleFormat = paFloat32 | paNonInterleaved;


    for (auto& s : STANDARD_SAMPLE_RATES) {
        if (Pa_IsFormatSupported(nullptr, &params, s) == paFormatIsSupported)
            sampleRates.push_back(s);
    }

    return Device{index, 
                pa_dev_info->name, 
                index == Pa_GetDefaultOutputDevice(),
                pa_api_info->type,
                pa_api_info->name, 
                index == Pa_GetHostApiInfo( pa_dev_info->hostApi )->defaultOutputDevice,
                pa_dev_info->maxOutputChannels,
                std::move(sampleRates)};
    }




    void tidyNames() {
        static std::vector<std::string> apiRemoves = {"Windows "};
        for (auto& d : m_devices) {
            for (auto& r : apiRemoves) {
                auto found = d.second.apiName.find(r);
                if (found != std::string::npos)
                    d.second.apiName.erase(found, r.length());
            }
        }
    }

    void correctMMENames() {
        // correct MME names
        std::vector<std::string*> mme;
        std::set<std::string> notMME;
        for (auto& d : m_devices) {
            if (d.second.apiIndex == paMME)
                mme.push_back(&d.second.name);
            else
                notMME.insert(d.second.name);
        }
        for (auto& cur : mme) {
            for (auto& alt : notMME) {
                if (alt.find(*cur) == 0) {
                    *cur = alt;
                }
            }
        } 
    }

    void removeDigitalDevices() {
        static std::vector<std::string> digitalStrings = {"SPDIF","S/PDIF","Optic","optic"};
        for (auto dev = m_devices.begin(); dev != m_devices.end();) {
            bool remove = false;
            for (auto& digi : digitalStrings) {
                if (dev->second.name.find(digi) != std::string::npos) {
                    remove = true;
                    break;
                }
            }
            if (remove)
                m_devices.erase(dev++);
            else
                ++dev;
        }
    }

    Device m_device;
    std::map<int, Device> m_devices;

    std::vector<Channel> m_channels;

    SPSCQueue<Ptr<Command>> m_commands;
    PaStream* m_stream;

    double m_sampleRate = 0;

    static int s_count;
};

int Session::Impl::s_count = 0;

//=============================================================================
// PUBLIC INTERFACE
//=============================================================================

Session::Session() : 
    m_impl((std::make_unique<Session::Impl>())) 
{ }

Session::~Session() {
    
}

int Session::open() {
    return open(getDefaultDevice());
}

int Session::open(const Device& device) {
    return m_impl->open(device, device.maxChannels, 0);
}

int Session::open(const Device& device, int channelCount, double sampleRate) {
    return m_impl->open(device, std::min(channelCount, device.maxChannels), sampleRate);
}

int Session::open(int index) {
    return open(getAvailableDevices().at(index));
}

int Session::open(int index, int channelCount, double sampleRate) {
    return open(getAvailableDevices().at(index),channelCount, sampleRate);
}

int Session::close() {
    return m_impl->close();
}

bool Session::isOpen() const {
    return m_impl->isOpen();
}

int Session::play(int channel, Ptr<Cue> cue) {
    return m_impl->play(channel, std::move(cue), 0);
}

int Session::play(int channel, Ptr<Cue> cue, double inSeconds) {
    return m_impl->play(channel, std::move(cue), inSeconds);
}

int Session::playAll(Ptr<Cue> cue) {
    for (int i = 0; i < getChannelCount(); ++i) {
        if (int ret = play(i, cue) != SyntactsError_NoError)
            return ret;
    }
    return SyntactsError_NoError;
}

int Session::stop(int channel) {
    return m_impl->stop(channel);
}

int Session::stopAll() {
    for (int i = 0; i < getChannelCount(); ++i) {
        if (int ret = stop(i) != SyntactsError_NoError)
            return ret;
    }
    return SyntactsError_NoError;
}

int Session::pause(int channel) {
    return m_impl->pause(channel, true);
}

int Session::pauseAll() {
    for (int i = 0; i < getChannelCount(); ++i) {
        if (int ret = pause(i) != SyntactsError_NoError)
            return ret;
    }
    return SyntactsError_NoError;
}

int Session::resume(int channel) {
    return m_impl->pause(channel, false);
}

int Session::resumeAll() {
    for (int i = 0; i < getChannelCount(); ++i) {
        if (int ret = resume(i) != SyntactsError_NoError)
            return ret;
    }
    return SyntactsError_NoError;
}

int Session::setVolume(int channel, float volume) {
    return m_impl->setVolume(channel, volume);
}

const Device& Session::getCurrentDevice() const {
    return m_impl->getCurrentDevcice();
}

const Device& Session::getDefaultDevice() const {
    return m_impl->getDefaultDevice();
}

const std::map<int, Device>& Session::getAvailableDevices() const {
    return m_impl->getAvailableDevices();
}

int Session::getChannelCount() const {
    return m_impl->getChannelCount();
}

double Session::getSampleRate() const {
    return m_impl->getSampleRate();
}

double Session::getCpuLoad() const {
    return m_impl->getCpuLoad();
}

int Session::count() {
    return Impl::count();
}

void Session::openControlPanel(int index) {
    m_impl->openControlPanel(index);
}

}; // namespace tact