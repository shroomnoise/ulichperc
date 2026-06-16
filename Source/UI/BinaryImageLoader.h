#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace PluginUI
{
class BinaryImageLoader
{
public:
    static juce::Image load(const char* resourceName);

private:
    static juce::String normaliseResourceName(juce::String resourceName);
    static const void* findBinaryResourceData(const juce::String& preferredName, int& dataSize);
};
}
