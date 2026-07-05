#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace PluginParameters
{
    inline constexpr const char* stateType = "params";

    inline constexpr const char* rzhavchinaId = "rzhavchina";
    inline constexpr const char* sustainShortenId = "sustainShorten";
    inline constexpr const char* warpEnabledId = "warpEnabled";
    inline constexpr const char* samplePunchId = "samplePunch";
    inline constexpr const char* samplePitchSemitonesId = "samplePitchSemitones";

    inline constexpr float samplePunchMinimum = 0.0f;
    inline constexpr float samplePunchMaximum = 1.0f;
    inline constexpr float samplePunchDefault = 0.0f;

    inline constexpr float samplePitchSemitonesMinimum = -8.0f;
    inline constexpr float samplePitchSemitonesMaximum = 8.0f;
    inline constexpr float samplePitchSemitonesDefault = 0.0f;

    bool isSampleSpecificParameterId(const juce::String& parameterId);

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
