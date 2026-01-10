#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CustomLookAndFeel.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    setSize (400, 300);
    
    bgImage = juce::ImageCache::getFromMemory(BinaryData::bg_png, BinaryData::bg_pngSize);

    static CustomLookAndFeel customLNF; // static so image is shared

    addAndMakeVisible(rzhavSlider);
    rzhavSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    rzhavSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    rzhavSlider.setLookAndFeel(&customLNF);
    rzhavSlider.setDoubleClickReturnValue(true, 0.0);
    rzhavSlider.setMouseDragSensitivity(150);
    rzhavAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    processorRef.parameters, "rzhavchina", rzhavSlider);

    addAndMakeVisible(sustainSlider);
    sustainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sustainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sustainSlider.setLookAndFeel(&customLNF);
    sustainSlider.setDoubleClickReturnValue(true, 0.0);
    sustainSlider.setMouseDragSensitivity(150);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    processorRef.parameters, "sustainShorten", sustainSlider);
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
    rzhavSlider.setBounds(300, 100, 55, 55);
    sustainSlider.setBounds(220, 100, 55, 55);
}
