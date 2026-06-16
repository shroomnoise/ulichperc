#include "BinaryImageLoader.h"

#include "BinaryData.h"

namespace PluginUI
{
juce::Image BinaryImageLoader::load(const char* resourceName)
{
    int dataSize = 0;
    if (const void* data = findBinaryResourceData(resourceName, dataSize))
        return juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));

    return {};
}

juce::String BinaryImageLoader::normaliseResourceName(juce::String resourceName)
{
    resourceName = resourceName.trim()
                               .toLowerCase()
                               .replaceCharacter('\\', '_')
                               .replaceCharacter('/', '_');

    while (resourceName.startsWithChar('_'))
        resourceName = resourceName.substring(1);

    return resourceName;
}

const void* BinaryImageLoader::findBinaryResourceData(const juce::String& preferredName, int& dataSize)
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
}
