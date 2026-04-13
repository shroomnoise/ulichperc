#include "SampleMetadata.h"

std::unique_ptr<SampleMetadata> loadMetadataForResource(const juce::String& wavResourceName)
{
    auto makeFallbackMetadata = [&wavResourceName](bool hasJsonFile, const juce::String& reason)
    {
        auto fallback = std::make_unique<SampleMetadata>();
        fallback->sampleRate = 48000.0;
        fallback->lengthSec = 0.0;
        fallback->hasTransientJson = hasJsonFile;
        fallback->warp = false;
        fallback->loop = false;
        fallback->ignoreTransientShaper = false;
        fallback->transients.push_back(0.0);
        DBG("  Using fallback metadata for " << wavResourceName
            << ": " << reason
            << " (sampleRate=48000, transient[0]=0.0)");
        return fallback;
    };

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
        return makeFallbackMetadata(false, "no JSON resource " + jsonResName);
    }

    juce::String jsonText = juce::String::fromUTF8(static_cast<const char*>(data), size);

    auto jsonVar = juce::JSON::parse(jsonText);
    if (jsonVar.isVoid() || !jsonVar.isObject())
    {
        return makeFallbackMetadata(true, "JSON parse failed for " + jsonResName);
    }

    auto& obj = *jsonVar.getDynamicObject();
    auto meta = std::make_unique<SampleMetadata>();
    meta->hasTransientJson = true;

    // JUCE 7 style: no default in getProperty
    if (obj.hasProperty("sampleRate"))
        meta->sampleRate = (double) obj.getProperty("sampleRate");
    if (meta->sampleRate <= 0.0)
        meta->sampleRate = 48000.0;

    meta->warp = obj.hasProperty("warp")
                   ? (bool) obj.getProperty("warp")
                   : false;

    meta->loop = obj.hasProperty("loop")
                   ? (bool) obj.getProperty("loop")
                   : false;

    meta->ignoreTransientShaper = obj.hasProperty("ignoreTransientShaper")
                                      ? (bool) obj.getProperty("ignoreTransientShaper")
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
        meta->transients.push_back(0.0);
        DBG("  JSON had no valid transients, inserted default transient 0.0 for " << jsonResName);
    }

    DBG("  Loaded metadata OK for " << jsonResName
        << " (transients=" << meta->transients.size()
        << ", loop=" << (meta->loop ? "true" : "false")
        << ", ignoreTransientShaper=" << (meta->ignoreTransientShaper ? "true" : "false") << ")");
    return meta;
}
