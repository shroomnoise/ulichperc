#include "PluginParameters.h"

namespace PluginParameters
{
    bool isSampleSpecificParameterId(const juce::String& parameterId)
    {
        return parameterId == samplePunchId
            || parameterId == samplePitchSemitonesId;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add(std::make_unique<juce::AudioParameterFloat>(
                       rzhavchinaId,
                       "Rzhavchina",
                       juce::NormalisableRange<float>(0.0f, 1.0f),
                       0.0f),
                   std::make_unique<juce::AudioParameterFloat>(
                       sustainShortenId,
                       "Pomyatost",
                       juce::NormalisableRange<float>(0.0f, 1.0f),
                       0.0f),
                   std::make_unique<juce::AudioParameterBool>(
                       warpEnabledId,
                       "Tempo sync",
                       true),
                   std::make_unique<juce::AudioParameterFloat>(
                       samplePunchId,
                       "Punch",
                       juce::NormalisableRange<float>(samplePunchMinimum,
                                                       samplePunchMaximum),
                       samplePunchDefault),
                   std::make_unique<juce::AudioParameterFloat>(
                       samplePitchSemitonesId,
                       "Pitch",
                       juce::NormalisableRange<float>(samplePitchSemitonesMinimum,
                                                       samplePitchSemitonesMaximum),
                       samplePitchSemitonesDefault));

        return layout;
    }
}
