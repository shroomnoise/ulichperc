#pragma once

#include "PluginProcessor.h"

class CustomLookAndFeel;

class ImageKnobSlider final : public juce::Slider
{
public:
    bool hitTest(int x, int y) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const auto radius = 0.5f * juce::jmin(bounds.getWidth(), bounds.getHeight());
        return centre.getDistanceFrom({ static_cast<float>(x), static_cast<float>(y) }) <= radius;
    }
};

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    // juce::Slider bitDepthSlider;
    // juce::Slider sampleRateSlider;
    ImageKnobSlider rzhavSlider;
    ImageKnobSlider sustainSlider;
    juce::Label rzhavLabel;
    juce::Label sustainLabel;
    juce::ToggleButton warpButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rzhavAttachment;

    // std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bitDepthAttachment;
    // std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sampleRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> warpAttachment;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;
    std::unique_ptr<CustomLookAndFeel> customLNF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
