#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "SampleLibrary/PercussionSampleLibrary.h"

class SampleSpecificParameterState final
{
public:
    SampleSpecificParameterState();

    float getValue(const juce::String& parameterId,
                   const PercussionSampleLibrary::SampleGroupInfo& sampleGroup,
                   int fallbackGroupIndex,
                   float fallbackValue) const;

    void setValue(const juce::String& parameterId,
                  const PercussionSampleLibrary::SampleGroupInfo& sampleGroup,
                  int fallbackGroupIndex,
                  float value);

    void writeToPluginState(juce::ValueTree& pluginState) const;
    void restoreFromPluginState(const juce::ValueTree& pluginState);

private:
    juce::ValueTree state;
    mutable juce::CriticalSection lock;
};
