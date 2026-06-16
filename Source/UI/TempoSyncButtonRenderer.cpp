#include "TempoSyncButtonRenderer.h"

#include "BinaryImageLoader.h"

namespace PluginUI
{
TempoSyncButtonRenderer::TempoSyncButtonRenderer()
    : onImage(BinaryImageLoader::load("tempo_on_png")),
      offImage(BinaryImageLoader::load("tempo_off_png"))
{
}

bool TempoSyncButtonRenderer::draw(juce::Graphics& g, juce::ToggleButton& button) const
{
    const auto& image = button.getToggleState() ? onImage : offImage;

    if (image.isValid())
    {
        const int scaledWidth = juce::roundToInt(image.getWidth() * 0.45);
        const int scaledHeight = juce::roundToInt(image.getHeight() * 0.45);
        const int x = (button.getWidth() - scaledWidth) / 2;
        const int y = (button.getHeight() - scaledHeight) / 2;

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(image,
                    x,
                    y,
                    scaledWidth,
                    scaledHeight,
                    0,
                    0,
                    image.getWidth(),
                    image.getHeight());
        return true;
    }

    const auto bounds = button.getLocalBounds().toFloat();
    g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colours::darkgrey);
    g.fillRoundedRectangle(bounds, 4.0f);

    return true;
}
}
