#include "MetaSamplerSound.h"
#include <rubberband/RubberBandStretcher.h>
#include <array>
#include <chrono>
#include <cmath>
#include <map>

namespace
{
    constexpr double warpBpmQuantum = 0.01;
    constexpr double warpBpmMatchEpsilon = 0.005;

    bool bpmMatches(double a, double b) noexcept
    {
        return std::abs(a - b) <= warpBpmMatchEpsilon;
    }
}

MetaSamplerSound::MetaSamplerSound(const juce::String& soundName,
                                   juce::AudioFormatReader& source,
                                   const juce::BigInteger& notes,
                                   int midiNoteForNormalPitch,
                                   double attackTimeSeconds,
                                   double releaseTimeSeconds,
                                   double maxSampleLengthSeconds,
                                   const juce::String& wavResourceNameForMetadata,
                                   double originalBpmIn)
    : name(soundName),
      sourceSampleRate(source.sampleRate),
      midiRootNote(midiNoteForNormalPitch),
      midiNotes(notes),
      attackTime(attackTimeSeconds),
      releaseTime(releaseTimeSeconds),
      originalBpm(originalBpmIn)
{
    if (sourceSampleRate <= 0.0)
        sourceSampleRate = 48000.0;

    auto numSamples = static_cast<int>(juce::jmin((juce::int64) (sourceSampleRate * maxSampleLengthSeconds),
                                                  source.lengthInSamples));

    data.setSize((int) source.numChannels, numSamples);
    source.read(&data, 0, numSamples, 0, true, true);

    lengthInSeconds = (double) numSamples / sourceSampleRate;
    attack = attackTimeSeconds;
    release = releaseTimeSeconds;

    // load transient metadata from BinaryData (if present)
    metadata = loadMetadataForResource(wavResourceNameForMetadata);
    if (metadata == nullptr)
        metadata = std::make_unique<SampleMetadata>();

    if (metadata->sampleRate <= 0.0)
        metadata->sampleRate = 48000.0;

    if (std::abs(metadata->sampleRate - sourceSampleRate) > 1.0)
    {
        DBG("MetaSamplerSound: metadata sampleRate (" << metadata->sampleRate
            << ") differs from audio sampleRate (" << sourceSampleRate
            << ") for " << wavResourceNameForMetadata
            << ". Warp processing will use audio sample rate.");
    }

    if (!metadata->hasTransients())
        metadata->transients.push_back(0.0);

    // Always trust measured audio duration over JSON.
    metadata->lengthSec = lengthInSeconds;

    warpEnabled = metadata->warp;

    DBG("MetaSamplerSound: metadata prepared for " << wavResourceNameForMetadata
        << " (hasTransientJson=" << (metadata->hasTransientJson ? "true" : "false")
        << ", sampleRate=" << metadata->sampleRate
        << ", lengthSec=" << metadata->lengthSec
        << ", transients=" << metadata->transients.size()
        << ", loop=" << (metadata->loop ? "true" : "false")
        << ", ignoreTransientShaper=" << (metadata->ignoreTransientShaper ? "true" : "false")
        << ", warp=" << (metadata->warp ? "true" : "false") << ")");
}

double MetaSamplerSound::quantizeWarpBpm(double hostBpm) noexcept
{
    const double safeBpm = juce::jmax(1.0, hostBpm);
    return std::round(safeBpm / warpBpmQuantum) * warpBpmQuantum;
}

double MetaSamplerSound::warpBaseBpmForHost(double originalBpm, double hostBpm) noexcept
{
    const double safeOriginalBpm = juce::jmax(1.0, originalBpm);
    const double safeHostBpm = juce::jmax(1.0, hostBpm);

    // For very slow host tempos, switch to half-time anchor (153 -> 76.5)
    // so loops speed up instead of stretching too far.
    return (safeHostBpm <= 90.0) ? (safeOriginalBpm * 0.5) : safeOriginalBpm;
}

double MetaSamplerSound::warpTimeRatioForHost(double originalBpm, double hostBpm) noexcept
{
    const double safeHostBpm = juce::jmax(1.0, hostBpm);
    return warpBaseBpmForHost(originalBpm, safeHostBpm) / safeHostBpm;
}

void MetaSamplerSound::setVelocityLayerInfo(int groupIndex,
                                            int groupCount,
                                            int minVelocity,
                                            int maxVelocity) noexcept
{
    velocityGroupCount = juce::jmax(1, groupCount);
    velocityGroupIndex = juce::jlimit(1, velocityGroupCount, groupIndex);
    velocityMin = juce::jlimit(1, 127, minVelocity);
    velocityMax = juce::jlimit(1, 127, maxVelocity);
    if (velocityMax < velocityMin)
        velocityMax = velocityMin;
}

void MetaSamplerSound::collectReadyWarpCache() const
{
    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (!warpCacheFuture.valid())
        return;

    if (warpCacheFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    auto readyCache = warpCacheFuture.get();
    const bool shouldPublish = (pendingWarpCacheBpm > 0.0);
    pendingWarpCacheBpm = 0.0;

    if (shouldPublish && readyCache)
        warpCache = std::move(readyCache);
}

std::shared_ptr<MetaSamplerSound::WarpedCache> MetaSamplerSound::getWarpedCache(double hostBpm) const
{
    if (!warpEnabled || metadata == nullptr)
        return nullptr;

    const double bpm = quantizeWarpBpm(hostBpm);
    collectReadyWarpCache();

    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (warpCache && bpmMatches(warpCache->bpm, bpm))
        return warpCache;

    return nullptr;
}

void MetaSamplerSound::requestWarpedCacheBuild(double hostBpm) const
{
    if (!warpEnabled || metadata == nullptr)
        return;

    const double bpm = quantizeWarpBpm(hostBpm);
    collectReadyWarpCache();

    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (warpCache && bpmMatches(warpCache->bpm, bpm))
        return;

    if (warpCacheFuture.valid())
    {
        if (bpmMatches(pendingWarpCacheBpm, bpm))
            return;

        // Keep background work bounded to one build per sound.
        if (warpCacheFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            if (warpCache && !bpmMatches(warpCache->bpm, bpm))
                warpCache.reset();
            return;
        }

        auto readyCache = warpCacheFuture.get();
        const bool shouldPublish = (pendingWarpCacheBpm > 0.0);
        pendingWarpCacheBpm = 0.0;
        if (shouldPublish && readyCache)
            warpCache = std::move(readyCache);

        if (warpCache && bpmMatches(warpCache->bpm, bpm))
            return;
    }

    // Drop stale cache proactively; voices keep their own shared_ptr copy.
    warpCache.reset();
    pendingWarpCacheBpm = bpm;

    warpCacheFuture = std::async(std::launch::async, [this, bpm]() -> std::shared_ptr<WarpedCache>
    {
        auto builtCache = renderWarpedCache(bpm);
        if (!builtCache)
            return {};

        builtCache->bpm = bpm;
        return std::shared_ptr<WarpedCache>(std::move(builtCache));
    });
}

bool MetaSamplerSound::isWarpCacheBuildInFlight() const
{
    if (!warpEnabled || metadata == nullptr)
        return false;

    collectReadyWarpCache();
    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (!warpCacheFuture.valid())
        return false;

    return warpCacheFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

void MetaSamplerSound::clearWarpedCache() const
{
    collectReadyWarpCache();
    std::lock_guard<std::mutex> lock(warpCacheMutex);
    pendingWarpCacheBpm = 0.0;
    warpCache.reset();
}

std::unique_ptr<MetaSamplerSound::WarpedCache> MetaSamplerSound::renderWarpedCache(double hostBpm) const
{
    if (metadata == nullptr)
        return nullptr;

    const int srcChannels = juce::jlimit(1, 2, data.getNumChannels());
    const int srcSamples  = data.getNumSamples();

    if (srcSamples <= 0)
        return nullptr;

    auto cache = std::make_unique<WarpedCache>();
    cache->bpm = hostBpm;
    // Warp cache must always use actual audio sample rate, not JSON metadata sampleRate.
    // JSON sampleRate can differ from the loaded WAV rate and would bias pitch.
    cache->sourceSampleRate = juce::jmax(1.0, sourceSampleRate);

    const double timeRatio = warpTimeRatioForHost(originalBpm, hostBpm);
    cache->timeRatio = timeRatio;

    const auto opts =
        RubberBand::RubberBandStretcher::OptionProcessOffline
      | RubberBand::RubberBandStretcher::OptionThreadingNever
      | RubberBand::RubberBandStretcher::OptionTransientsCrisp
      | RubberBand::RubberBandStretcher::OptionDetectorPercussive
      | RubberBand::RubberBandStretcher::OptionWindowShort
      | RubberBand::RubberBandStretcher::OptionChannelsTogether;

    RubberBand::RubberBandStretcher stretcher(
        (size_t) cache->sourceSampleRate,
        (size_t) srcChannels,
        opts,
        timeRatio,
        1.0);

    // Key-frame map from transient list (source frame -> stretched frame)
    std::map<size_t, size_t> keyFrames;
    keyFrames[0] = 0;

    const double sr = cache->sourceSampleRate;
    for (double t : metadata->transients)
    {
        if (t < 0.0)
            continue;

        const size_t srcFrame = (size_t) juce::jmax(0.0, std::floor(t * sr));
        const size_t dstFrame = (size_t) std::llround((double) srcFrame * timeRatio);
        keyFrames[srcFrame] = dstFrame;
    }

    const size_t lastSrc = (size_t) srcSamples;
    keyFrames[lastSrc] = (size_t) std::llround((double) lastSrc * timeRatio);

    stretcher.setTimeRatio(timeRatio);
    stretcher.setKeyFrameMap(keyFrames);

    std::array<const float*, 2> inPtrs {
        data.getReadPointer(0),
        (srcChannels > 1) ? data.getReadPointer(1) : data.getReadPointer(0)
    };

    stretcher.study(inPtrs.data(), (size_t) srcSamples, true);
    stretcher.process(inPtrs.data(), (size_t) srcSamples, true);

    const size_t pad = (size_t)(stretcher.getStartDelay() + stretcher.getLatency() + 128);
    size_t estimatedOut = (size_t) std::ceil((double) srcSamples * timeRatio + (double) pad);
    estimatedOut = juce::jmax<size_t>(estimatedOut, (size_t) srcSamples);

    cache->buffer.setSize(srcChannels, (int) estimatedOut, false, true, true);

    std::array<float*, 2> outPtrs {};
    size_t written = 0;
    int safety = 0;

    while (stretcher.available() > 0 && safety < 4096)
    {
        const size_t available = stretcher.available();
        if (available == 0)
            break;

        const size_t needed = written + available;
        if ((size_t) cache->buffer.getNumSamples() < needed)
            cache->buffer.setSize(srcChannels, (int) needed, true, true, true);

        outPtrs[0] = cache->buffer.getWritePointer(0, (int) written);
        outPtrs[1] = (srcChannels > 1) ? cache->buffer.getWritePointer(1, (int) written)
                                       : outPtrs[0];

        const size_t got = stretcher.retrieve(outPtrs.data(), available);
        written += got;

        if (got == 0)
            break;

        ++safety;
    }

    cache->buffer.setSize(srcChannels, (int) written, true, true, true);
    return cache;
}

bool MetaSamplerSound::appliesToNote(int midiNoteNumber)
{
    return midiNotes[midiNoteNumber];
}

bool MetaSamplerSound::appliesToChannel(int midiChannel)
{
    juce::ignoreUnused(midiChannel);
    return true; // applies to all channels
}
