#pragma once

#include <juce_core/juce_core.h>

struct ParsedSampleName
{
    int noteIndex = 0;
    int velocityGroupIndex = 1;
    int variationIndex = 1;
    int pitchIndex = 1;
};

namespace SampleNameParser
{
    juce::String getFileNameStem(juce::String pathOrName);
    bool parseSampleName(const juce::String& sampleIdentifier, ParsedSampleName& out);
}
