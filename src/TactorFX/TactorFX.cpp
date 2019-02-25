#include <TactorFX/TactorFX.hpp>
#include "Helpers.hpp"
#include "portaudio.h"
#include <atomic>
#include <mutex>
#include <cassert>

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 256

namespace tfx {

// private namespace
namespace {    

std::size_t g_num_ch;         ///< number of channels specified
std::vector<Ptr<Cue>> g_cues; ///< cues
std::mutex g_mutex;           ///< mutex
PaStream* g_stream;           ///< portaudio stream
bool g_initialized = false;   ///< tfx initialized?

// portaudio callback method
int pa_callback(const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo *timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userData)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    /* Cast data passed through stream to our structure. */    
    float *out = (float *)outputBuffer;
    (void)inputBuffer; /* Prevent unused variable warning. */   
    // lock mutex using RA-II lockgaurd (unlocked when we exit scope b/c lock dies)
    for (unsigned long f = 0; f < framesPerBuffer; f++) {
        for (std::size_t c = 0; c < g_num_ch; ++c) {
            out[g_num_ch * f + c] = g_cues[c]->nextSample();
        }
    }
    return 0;
}    

} // private namespace

int initialize(std::size_t channelCount, std::size_t device) {
    assert(!g_initialized);
    // init g_cues with empty cues
    g_cues.resize(channelCount);
    for (auto& cue : g_cues)
        cue = make<Cue>();  
    Pa_Initialize();
    PaStreamParameters hostApiOutputParameters;
    PaStreamParameters* hostApiOutputParametersPtr;        
    if (channelCount > 0) {
        hostApiOutputParameters.device = (PaDeviceIndex)device;
		if (hostApiOutputParameters.device == paNoDevice)
			return paDeviceUnavailable;
        hostApiOutputParameters.channelCount = (int)channelCount;
        hostApiOutputParameters.sampleFormat = paFloat32;
        hostApiOutputParameters.suggestedLatency = Pa_GetDeviceInfo( hostApiOutputParameters.device )->defaultHighOutputLatency;
        hostApiOutputParameters.hostApiSpecificStreamInfo = NULL;
        hostApiOutputParametersPtr = &hostApiOutputParameters;
    }
    else {
        hostApiOutputParametersPtr = NULL;
    }
    return Pa_OpenStream(&g_stream, nullptr, hostApiOutputParametersPtr, SAMPLE_RATE, BUFFER_SIZE, paNoFlag, pa_callback, nullptr );
}

void finalize() {
    assert(g_initialized);
    Pa_StopStream(g_stream); 
    Pa_CloseStream(g_stream); 
    Pa_Terminate();
}

void playCue(std::size_t channel, Ptr<Cue> cue) {
    assert(g_initialized);
    assert(channel < g_num_ch);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_cues[channel] = cue;
} 

} // namespace tfx