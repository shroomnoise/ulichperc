#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr float imageScale = 0.5f;

    CustomLookAndFeel()
    {
        rzhavImage = loadImageFromBinaryData("rzhav_png");
        pomyatosImage = loadImageFromBinaryData("pomyatos_png");
        tempoOnImage = loadImageFromBinaryData("tempo_on_png");
        tempoOffImage = loadImageFromBinaryData("tempo_off_png");
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        if (slider.getComponentID() == "rzhavSlider" && rzhavImage.isValid())
        {
            constexpr float travelUpPx = 130.0f;
            const float norm = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
            const float offsetY = -norm * travelUpPx; // 0 -> -240 as value goes 0 -> 1
            const int scaledWidth = juce::roundToInt(rzhavImage.getWidth() * imageScale);
            const int scaledHeight = juce::roundToInt(rzhavImage.getHeight() * imageScale);
            const int imageX = x + (width - scaledWidth) / 2;
            const float imageY = y + travelUpPx + offsetY;

            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(rzhavImage,
                        imageX, juce::roundToInt(imageY),
                        scaledWidth, scaledHeight,
                        0, 0,
                        rzhavImage.getWidth(), rzhavImage.getHeight());
            return;
        }

        if (slider.getComponentID() != "sustainSlider" || !pomyatosImage.isValid())
        {
            juce::LookAndFeel_V4::drawRotarySlider(g, x, y, width, height,
                                                   sliderPosProportional,
                                                   rotaryStartAngle, rotaryEndAngle,
                                                   slider);
            return;
        }

        const float rotation = juce::jmap(sliderPosProportional, 0.0f, 1.0f,
                                          juce::degreesToRadians(-100.0f),
                                          juce::degreesToRadians(0.0f));

        const auto& image = pomyatosImage;
        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f;

        juce::AffineTransform transform =
            juce::AffineTransform::translation(-image.getWidth() * 0.5f, -image.getHeight() * 0.5f)
                .scaled(imageScale)
                .rotated(rotation)
                .translated(cx, cy);

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImageTransformed(image, transform, false);
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool /*shouldDrawButtonAsHighlighted*/,
                          bool /*shouldDrawButtonAsDown*/) override
    {
        const auto& image = button.getToggleState() ? tempoOnImage : tempoOffImage;

        if (image.isValid())
        {
            const int scaledWidth = juce::roundToInt(image.getWidth() * 0.45);
            const int scaledHeight = juce::roundToInt(image.getHeight() * 0.45);
            const int x = (button.getWidth() - scaledWidth) / 2;
            const int y = (button.getHeight() - scaledHeight) / 2;

            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(image,
                        x, y,
                        scaledWidth, scaledHeight,
                        0, 0,
                        image.getWidth(), image.getHeight());
            return;
        }

        const auto bounds = button.getLocalBounds().toFloat();
        g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colours::darkgrey);
        g.fillRoundedRectangle(bounds, 4.0f);
    }

private:
    static juce::String normaliseResourceName(juce::String resourceName)
    {
        resourceName = resourceName.trim()
                                   .toLowerCase()
                                   .replaceCharacter('\\', '_')
                                   .replaceCharacter('/', '_');

        while (resourceName.startsWithChar('_'))
            resourceName = resourceName.substring(1);

        return resourceName;
    }

    static const void* findBinaryResourceData(const juce::String& preferredName, int& dataSize)
    {
        dataSize = 0;

        if (const void* data = BinaryData::getNamedResource(preferredName.toRawUTF8(), dataSize))
            return data;

        const auto wanted = normaliseResourceName(preferredName);

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String candidate(BinaryData::namedResourceList[i]);
            const auto normalisedCandidate = normaliseResourceName(candidate);

            if (normalisedCandidate == wanted
                || normalisedCandidate.endsWith("_" + wanted)
                || normalisedCandidate.endsWith(wanted))
            {
                if (const void* data = BinaryData::getNamedResource(candidate.toRawUTF8(), dataSize))
                    return data;
            }
        }

        return nullptr;
    }

    static juce::Image loadImageFromBinaryData(const char* resourceName)
    {
        int dataSize = 0;
        if (const void* data = findBinaryResourceData(resourceName, dataSize))
            return juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));

        return {};
    }

    juce::Image rzhavImage;
    juce::Image pomyatosImage;
    juce::Image tempoOnImage;
    juce::Image tempoOffImage;
};
