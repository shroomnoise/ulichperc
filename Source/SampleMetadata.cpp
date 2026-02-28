#include "SampleMetadata.h"

std::unique_ptr<SampleMetadata> loadMetadataForResource(const juce::String& wavResourceName)
{
    // Example:
    //   wavResourceName     = "_33_wav"
    //   we want JSON name = "_33_transients_json"

    // Strip the trailing "_wav" -> "_33"
    juce::String baseName = wavResourceName.upToLastOccurrenceOf("_wav", false, false);

    // Build the expected JSON resource name
    juce::String jsonResName = baseName + "_transients_json";

    DBG("Trying to load metadata: wav=" << wavResourceName
        << " -> jsonResName=" << jsonResName);

    int size = 0;
    const void* data = BinaryData::getNamedResource(jsonResName.toRawUTF8(), size);
    if (data == nullptr || size <= 0)
    {
        DBG("  No BinaryData resource found for " << jsonResName);
        return nullptr;
    }

    juce::String jsonText = juce::String::fromUTF8(static_cast<const char*>(data), size);

    auto jsonVar = juce::JSON::parse(jsonText);
    if (jsonVar.isVoid() || !jsonVar.isObject())
    {
        DBG("  JSON parse failed for " << jsonResName);
        return nullptr;
    }

    auto& obj = *jsonVar.getDynamicObject();
    auto meta = std::make_unique<SampleMetadata>();

    // JUCE 7 style: no default in getProperty
    meta->sampleRate = obj.hasProperty("sampleRate")
                         ? (double) obj.getProperty("sampleRate")
                         : 48000.0;

    meta->lengthSec = obj.hasProperty("lengthSec")
                        ? (double) obj.getProperty("lengthSec")
                        : 0.0;

    meta->warp = obj.hasProperty("warp")
                   ? (bool) obj.getProperty("warp")
                   : false;

    auto tv = obj.getProperty("transients");
    if (tv.isArray())
    {
        for (auto& v : *tv.getArray())
            meta->transients.push_back((double)v);

        std::sort(meta->transients.begin(), meta->transients.end());
    }

    if (!meta->hasTransients())
    {
        DBG("  Parsed JSON but no valid transients in " << jsonResName);
        return nullptr;
    }

    DBG("  Loaded metadata OK for " << jsonResName
        << " (zones=" << meta->transients.size() << ")");
    return meta;
}
