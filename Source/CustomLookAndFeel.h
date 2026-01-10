#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        knobImage = juce::ImageCache::getFromMemory(BinaryData::knob_png, BinaryData::knob_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        if (! knobImage.isValid())
            return;

        const float rotation = juce::jmap(sliderPosProportional, 0.0f, 1.0f,
                                          juce::degreesToRadians(-110.0f),
                                          juce::degreesToRadians(110.0f));

        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float scale = juce::jmin(width, height) / (float) knobImage.getWidth();

        juce::AffineTransform transform =
            juce::AffineTransform::translation(-knobImage.getWidth() * 0.5f, -knobImage.getHeight() * 0.5f)
                .rotated(rotation)
                .scaled(scale)
                .translated(cx, cy);

        g.drawImageTransformed(knobImage, transform, false);
    }

private:
    juce::Image knobImage;
};
