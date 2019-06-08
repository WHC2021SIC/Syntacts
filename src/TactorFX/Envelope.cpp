#include <TactorFX/Envelope.hpp>
#include "Helpers.hpp"

namespace tfx {

Envelope::Envelope(float duration, float amplitude) :
    Generator(),
    m_duration(std::abs(duration)),
    m_amplitude(clamp01(amplitude))
{

}

void Envelope::setDuration(float duration) {
    m_duration = std::abs(duration);
}

void Envelope::setAmplitude(float amplitude) {
    m_amplitude = clamp01(amplitude);
}

bool Envelope::playing() const {
    return getTime() < m_duration;
}

float Envelope::onSample(float t) {
    return playing() ? m_amplitude : 0.0f;
}

ASR::ASR(float attackTime, float sustainTime, float releaseTime, float amplitude) :
    Envelope(attackTime + sustainTime + releaseTime, amplitude),
    m_attackTime(attackTime), 
    m_sustainTime(sustainTime), 
    m_releaseTime(releaseTime)
{

}

float ASR::onSample(float t) {
    if (playing()) {
        if (t < m_attackTime)
            return interp(t, 0.0f, m_attackTime, 0.0f, m_amplitude);
        else if (t < (m_attackTime + m_sustainTime) && t > m_attackTime)
            return m_amplitude;
        else if (t > (m_attackTime + m_sustainTime))
            return interp(t, (m_attackTime + m_sustainTime), (m_attackTime + m_sustainTime + m_releaseTime), m_amplitude, 0.0f);
        else if (t > m_duration)
            return 0.0f;
    }
    else 
        return 0.0f;        
}

} // namespae tfx