#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CustomLookAndFeel.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    setSize (900, 600);

    bgImage = juce::ImageCache::getFromMemory(BinaryData::bg_png, BinaryData::bg_pngSize);

    static CustomLookAndFeel customLNF; // static so image is shared

    addAndMakeVisible(rzhavSlider);
    rzhavSlider.setComponentID("rzhavSlider");
    rzhavSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    rzhavSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    rzhavSlider.setLookAndFeel(&customLNF);
    rzhavSlider.setDoubleClickReturnValue(true, 0.0);
    rzhavSlider.setMouseDragSensitivity(150);
    rzhavAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    processorRef.parameters, "rzhavchina", rzhavSlider);

    addAndMakeVisible(sustainSlider);
    sustainSlider.setComponentID("sustainSlider");
    sustainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sustainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sustainSlider.setLookAndFeel(&customLNF);
    sustainSlider.setDoubleClickReturnValue(true, 0.0);
    sustainSlider.setMouseDragSensitivity(150);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    processorRef.parameters, "sustainShorten", sustainSlider);

    addAndMakeVisible(warpButton);
    warpButton.setButtonText({});
    warpButton.setLookAndFeel(&customLNF);
    warpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
    processorRef.parameters, "warpEnabled", warpButton);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (bgImage.isValid())
        g.drawImage(bgImage, getLocalBounds().toFloat());
    else
        g.fillAll(juce::Colours::black);
}

void AudioPluginAudioProcessorEditor::resized()
{
    constexpr int controlWidth = 200;
    constexpr int controlHeight = 200;

    rzhavSlider.setBounds(68, -140, 380, 900);
    sustainSlider.setBounds(549, 183, controlWidth, controlHeight);
    warpButton.setBounds(23, 18, 170, 110);
}
