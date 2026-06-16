#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace PluginParameters
{
    inline constexpr const char* stateType = "params";

    inline constexpr const char* rzhavchinaId = "rzhavchina";
    inline constexpr const char* sustainShortenId = "sustainShorten";
    inline constexpr const char* warpEnabledId = "warpEnabled";

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
