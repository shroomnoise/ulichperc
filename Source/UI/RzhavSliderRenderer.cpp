#include "RzhavSliderRenderer.h"

#include "BinaryImageLoader.h"

namespace PluginUI
{
namespace
{
constexpr float controlImageScale = 0.5f;
}

RzhavSliderRenderer::RzhavSliderRenderer()
    : image(BinaryImageLoader::load("rzhav_png"))
{
}

bool RzhavSliderRenderer::drawVerticalTravelSlider(juce::Graphics& g,
                                                   int x,
                                                   int y,
                                                   int width,
                                                   int height,
                                                   float sliderPosProportional) const
{
    juce::ignoreUnused(height);

    if (!image.isValid())
        return false;

    constexpr float travelUpPx = 130.0f;
    const float norm = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
    const float offsetY = -norm * travelUpPx;
    const int scaledWidth = juce::roundToInt(image.getWidth() * controlImageScale);
    const int scaledHeight = juce::roundToInt(image.getHeight() * controlImageScale);
    const int imageX = x + (width - scaledWidth) / 2;
    const float imageY = y + travelUpPx + offsetY;

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(image,
                imageX,
                juce::roundToInt(imageY),
                scaledWidth,
                scaledHeight,
                0,
                0,
                image.getWidth(),
                image.getHeight());

    return true;
}
}
