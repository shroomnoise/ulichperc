#include "RotarySliderRenderer.h"

#include <utility>

#include "BinaryImageLoader.h"

namespace PluginUI
{
RotarySliderRenderer::RotarySliderRenderer(RotarySliderRendererConfig rendererConfig)
    : config(std::move(rendererConfig))
{
    if (config.imageResourceName.isNotEmpty())
        image = BinaryImageLoader::load(config.imageResourceName.toRawUTF8());
}

bool RotarySliderRenderer::draw(juce::Graphics& g,
                                int x,
                                int y,
                                int width,
                                int height,
                                float sliderPosProportional) const
{
    if (!image.isValid())
        return false;

    auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                         static_cast<float>(y),
                                         static_cast<float>(width),
                                         static_cast<float>(height));
    auto knobBounds = bounds;
    juce::Rectangle<float> labelBounds;

    const bool shouldDrawLabel = config.labelText.isNotEmpty()
                                 && config.labelHeight > 0.0f
                                 && config.labelFontHeight > 0.0f;

    if (shouldDrawLabel)
    {
        labelBounds = knobBounds.removeFromBottom(juce::jmin(config.labelHeight, knobBounds.getHeight()));
        knobBounds.removeFromBottom(juce::jmin(config.labelGap, knobBounds.getHeight()));
    }

    if (knobBounds.isEmpty())
        return false;

    const float rotation = juce::jmap(juce::jlimit(0.0f, 1.0f, sliderPosProportional),
                                      0.0f,
                                      1.0f,
                                      config.minAngleRadians,
                                      config.maxAngleRadians);
    const auto centre = knobBounds.getCentre() + config.centreOffset;

    const juce::AffineTransform transform =
        juce::AffineTransform::translation(-image.getWidth() * 0.5f, -image.getHeight() * 0.5f)
            .scaled(config.imageScale)
            .rotated(rotation)
            .translated(centre.x, centre.y);

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImageTransformed(image, transform, false);

    if (shouldDrawLabel)
    {
        g.setColour(config.labelColour);
        g.setFont(config.labelFontHeight);
        g.drawFittedText(config.labelText,
                         labelBounds.toNearestInt(),
                         juce::Justification::centred,
                         1);
    }

    return true;
}
}
