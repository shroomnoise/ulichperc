#include "MetaSamplerVoice.h"
#include <cmath>

MetaSamplerVoice::MetaSamplerVoice()
{
    adsr.setParameters({ 0.00f, 0.0f, 1.0f, 0.06f });
}

void MetaSamplerVoice::setSustainParam(std::atomic<float>* p)
{
    sustainAmountParam = p;
}

bool MetaSamplerVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<MetaSamplerSound*>(sound) != nullptr;
}

void MetaSamplerVoice::startNote(int midiNoteNumber, float velocity,
                                 juce::SynthesiserSound* s,
                                 int /*currentPitchWheelPosition*/)
{
    currentSound = dynamic_cast<MetaSamplerSound*>(s);
    sourceSamplePosition = 0.0;
    currentTransientIndex = 0;
    metadata = nullptr;

    juce::ignoreUnused(midiNoteNumber);

    {
        const float v = juce::jlimit(0.0f, 1.0f, velocity);
        velocityGain = std::sqrt(v);
    }

    if (currentSound != nullptr)
    {
        metadata = currentSound->metadata.get();

        double sourceSR   = currentSound->getSourceSampleRate();
        double playbackSR = getSampleRate();

        if (sourceSR <= 0.0)   sourceSR = 44100.0;
        if (playbackSR <= 0.0) playbackSR = 44100.0;

        pitchRatio = (sourceSR / playbackSR);

        adsr.setSampleRate(playbackSR);
        adsr.noteOn();
    }
    else
    {
        clearCurrentNote();
    }
}

void MetaSamplerVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
        currentSound = nullptr;
        metadata = nullptr;
        currentTransientIndex = 0;
    }
}

void MetaSamplerVoice::pitchWheelMoved(int) {}
void MetaSamplerVoice::controllerMoved(int, int) {}

void MetaSamplerVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                       int startSample,
                                       int numSamples)
{
    if (currentSound == nullptr)
        return;

    const auto& data = currentSound->getAudioData();
    const int sourceNumSamples = data.getNumSamples();
    const int sourceNumChans   = data.getNumChannels();
    const int outNumChans      = outputBuffer.getNumChannels();

    if (!adsr.isActive())
    {
        clearCurrentNote();
        currentSound = nullptr;
        metadata = nullptr;
        return;
    }

    float sustainAmount = sustainAmountParam != nullptr ? sustainAmountParam->load() : 0.0f;
    sustainAmount = juce::jlimit(0.0f, 1.0f, sustainAmount);

    const float* srcL = data.getReadPointer(0);
    const float* srcR = (sourceNumChans > 1) ? data.getReadPointer(1) : nullptr;

    float* out0 = (outNumChans > 0) ? outputBuffer.getWritePointer(0, startSample) : nullptr;
    float* out1 = (outNumChans > 1) ? outputBuffer.getWritePointer(1, startSample) : nullptr;

    const bool doSustainShorten = (metadata != nullptr && sustainAmount > 0.0f);

    if (outNumChans >= 2 && out0 != nullptr && out1 != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (sourceSamplePosition >= (double)sourceNumSamples)
            {
                adsr.noteOff();
                if (!adsr.isActive())
                {
                    clearCurrentNote();
                    currentSound = nullptr;
                    metadata = nullptr;
                    break;
                }
            }

            const int pos = (int)sourceSamplePosition;
            if (pos + 1 >= sourceNumSamples)
            {
                sourceSamplePosition += pitchRatio;
                continue;
            }

            const float frac = (float)(sourceSamplePosition - (double)pos);

            const float s1L = srcL[pos];
            const float s2L = srcL[pos + 1];
            float inL = s1L + frac * (s2L - s1L);

            float inR = inL;
            if (srcR != nullptr)
            {
                const float s1R = srcR[pos];
                const float s2R = srcR[pos + 1];
                inR = s1R + frac * (s2R - s1R);
            }

            const float env = adsr.getNextSample();
            const float g = env * velocityGain;

            float sampleL = inL * g;
            float sampleR = inR * g;

            if (doSustainShorten)
            {
                const double timeSec = sourceSamplePosition / metadata->sampleRate;
                const float sustainGain = computeSustainGain(timeSec, sustainAmount);
                sampleL *= sustainGain;
                sampleR *= sustainGain;
            }

            out0[i] += sampleL;
            out1[i] += sampleR;

            for (int ch = 2; ch < outNumChans; ++ch)
            {
                float* out = outputBuffer.getWritePointer(ch, startSample);
                out[i] += 0.5f * (sampleL + sampleR);
            }

            sourceSamplePosition += pitchRatio;
        }

        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        if (sourceSamplePosition >= (double)sourceNumSamples)
        {
            adsr.noteOff();
            if (!adsr.isActive())
            {
                clearCurrentNote();
                currentSound = nullptr;
                metadata = nullptr;
                break;
            }
        }

        const int pos = (int)sourceSamplePosition;
        if (pos + 1 >= sourceNumSamples)
        {
            sourceSamplePosition += pitchRatio;
            continue;
        }

        const float frac = (float)(sourceSamplePosition - (double)pos);

        const float s1L = srcL[pos];
        const float s2L = srcL[pos + 1];
        float inL = s1L + frac * (s2L - s1L);

        float inR = inL;
        if (srcR != nullptr)
        {
            const float s1R = srcR[pos];
            const float s2R = srcR[pos + 1];
            inR = s1R + frac * (s2R - s1R);
        }

        const float env = adsr.getNextSample();
        const float g = env * velocityGain;

        float sampleL = inL * g;
        float sampleR = inR * g;

        if (doSustainShorten)
        {
            const double timeSec = sourceSamplePosition / metadata->sampleRate;
            const float sustainGain = computeSustainGain(timeSec, sustainAmount);
            sampleL *= sustainGain;
            sampleR *= sustainGain;
        }

        for (int ch = 0; ch < outNumChans; ++ch)
        {
            const float v = (ch == 0 ? sampleL
                                     : (ch == 1 ? sampleR
                                                : 0.5f * (sampleL + sampleR)));
            outputBuffer.addSample(ch, startSample + i, v);
        }

        sourceSamplePosition += pitchRatio;
    }
}

float MetaSamplerVoice::computeSustainGain(double timeSec, float amount)
{
    if (metadata == nullptr || metadata->transients.empty() || amount <= 0.0f)
        return 1.0f;

    const auto& ts = metadata->transients;

    while (currentTransientIndex + 1 < (int)ts.size()
           && timeSec >= ts[(size_t)currentTransientIndex + 1])
        ++currentTransientIndex;

    const double t0 = ts[(size_t)currentTransientIndex];
    const double t1 = (currentTransientIndex + 1 < (int)ts.size())
                        ? ts[(size_t)currentTransientIndex + 1]
                        : metadata->lengthSec;

    if (timeSec < t0) return 1.0f;

    const double segLen = t1 - t0;
    if (segLen <= 0.0) return 1.0f;

    const double a = juce::jlimit(0.0, 1.0, (double)amount);

    constexpr double holdCurve = 15.0;
    const double holdFrac  = std::pow(1.0 - a, holdCurve);
    const double fadeStart = t0 + holdFrac * segLen;

    if (timeSec <= fadeStart) return 1.0f;
    if (timeSec >= t1)        return 0.0f;

    const double x = (timeSec - fadeStart) / juce::jmax(1e-9, (t1 - fadeStart));

    constexpr double kMin = 0.2;
    constexpr double kMax = 9.0;
    const double k = kMin + (kMax - kMin) * a;

    const double e0 = std::exp(-k * 0.0);
    const double e1 = std::exp(-k * 1.0);
    const double ex = std::exp(-k * x);

    const double g = (ex - e1) / (e0 - e1);

    return (float)juce::jlimit(0.0, 1.0, g);
}
