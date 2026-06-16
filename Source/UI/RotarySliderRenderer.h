#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace PluginUI
{
struct RotarySliderRendererConfig
{
    juce::String imageResourceName;
    juce::String labelText;
    float imageScale = 0.25f;
    float minAngleRadians = juce::degreesToRadians(-100.0f);
    float maxAngleRadians = juce::degreesToRadians(0.0f);
    juce::Point<float> centreOffset;
    float labelHeight = 22.0f;
    float labelGap = 1.0f;
    float labelFontHeight = 15.0f;
    juce::Colour labelColour = juce::Colours::black;
};

class RotarySliderRenderer
{
public:
    explicit RotarySliderRenderer(RotarySliderRendererConfig rendererConfig);

    bool draw(juce::Graphics& g,
              int x,
              int y,
              int width,
              int height,
              float sliderPosProportional) const;

private:
    RotarySliderRendererConfig config;
    juce::Image image;
};
}
