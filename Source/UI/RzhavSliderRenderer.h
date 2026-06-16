#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace PluginUI
{
class RzhavSliderRenderer
{
public:
    RzhavSliderRenderer();

    bool drawVerticalTravelSlider(juce::Graphics& g,
                                  int x,
                                  int y,
                                  int width,
                                  int height,
                                  float sliderPosProportional) const;

private:
    juce::Image image;
};
}
