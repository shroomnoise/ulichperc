#include "RzhavProcessor.h"

#include <algorithm>
#include <cmath>

void RzhavProcessor::prepare(double sampleRate) noexcept
{
    currentSampleRate = sampleRate;
    reset();
}

void RzhavProcessor::reset() noexcept
{
    heldPerChannel.fill(0.0f);
    phasePerChannel.fill(0);
}

void RzhavProcessor::process(juce::AudioBuffer<float>& buffer, float amount) noexcept
{
    const float k = juce::jlimit(0.0f, 1.0f, amount);
    if (k <= 0.0f)
        return;

    if (currentSampleRate <= 0.0)
        return;

    const int numChannels = juce::jmin(buffer.getNumChannels(), maxSupportedChannels);
    const int numSamples = buffer.getNumSamples();

    const float bitsF = juce::jmap(k, 12.0f, 8.0f);
    const int bits = juce::jlimit(8, 12, (int) std::round(bitsF));

    const double srMax = std::min(21000.0, currentSampleRate);
    const double srMin = 9000.0;
    const double targetFs = juce::jlimit(srMin, srMax, juce::jmap((double) k, srMax, srMin));

    int rateDivide = (int) std::round(currentSampleRate / targetFs);
    rateDivide = juce::jlimit(1, 4096, rateDivide);

    const float levels = (float) ((1 << bits) - 1);
    const float invLevels = 1.0f / levels;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);

        float held = heldPerChannel[(size_t) ch];
        int phase = phasePerChannel[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            if (rateDivide == 1)
            {
                held = data[i];
            }
            else
            {
                if (phase == 0)
                    held = data[i];
                else
                    data[i] = held;

                ++phase;
                if (phase >= rateDivide)
                    phase = 0;
            }

            float x = juce::jlimit(-1.0f, 1.0f, data[i]);
            x = std::round(x * levels) * invLevels;
            data[i] = x;
        }

        heldPerChannel[(size_t) ch] = held;
        phasePerChannel[(size_t) ch] = phase;
    }
}
