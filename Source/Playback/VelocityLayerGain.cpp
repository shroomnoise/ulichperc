#include "VelocityLayerGain.h"

#include <cmath>

float VelocityLayerGain::calculate(float noteVelocity, const PercussionSound* sound) noexcept
{
    const float velocity = juce::jlimit(0.0f, 1.0f, noteVelocity);
    const int midiVelocity = juce::jlimit(1, 127, (int) std::lround(velocity * 127.0f));

    const int groupCount = (sound != nullptr) ? juce::jmax(1, sound->getVelocityGroupCount()) : 1;
    int groupMin = (sound != nullptr) ? sound->getVelocityMin() : 1;
    int groupMax = (sound != nullptr) ? sound->getVelocityMax() : 127;
    if (groupMax < groupMin)
        groupMax = groupMin;

    const float t = (groupMax > groupMin)
                        ? juce::jlimit(0.0f, 1.0f,
                                       (float) (midiVelocity - groupMin) / (float) (groupMax - groupMin))
                        : 0.0f;

    const float groupSpanDb = 20.0f / (float) groupCount;
    const float gainDb = (-0.5f * groupSpanDb) + (t * groupSpanDb);
    return juce::Decibels::decibelsToGain(gainDb + 4.0f);
}
