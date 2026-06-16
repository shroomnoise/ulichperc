#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace PluginUI
{
class TempoSyncButtonRenderer
{
public:
    TempoSyncButtonRenderer();

    bool draw(juce::Graphics& g, juce::ToggleButton& button) const;

private:
    juce::Image onImage;
    juce::Image offImage;
};
}
