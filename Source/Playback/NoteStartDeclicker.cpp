#include "NoteStartDeclicker.h"

#include <juce_core/juce_core.h>

void NoteStartDeclicker::reset() noexcept
{
    remainingSamples = 0;
}

void NoteStartDeclicker::trigger() noexcept
{
    remainingSamples = rampSamples;
}

float NoteStartDeclicker::getNextGain() noexcept
{
    if (remainingSamples <= 0)
        return 1.0f;

    const int progressed = rampSamples - remainingSamples + 1;
    const float gain = (float) progressed / (float) rampSamples;
    --remainingSamples;
    return juce::jlimit(0.0f, 1.0f, gain);
}
