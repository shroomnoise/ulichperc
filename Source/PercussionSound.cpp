#include "PercussionSound.h"
#include <rubberband/RubberBandStretcher.h>
#include <array>
#include <chrono>
#include <cmath>
#include <map>

namespace
{
    constexpr double warpBpmQuantum = 0.01;
    constexpr double warpBpmMatchEpsilon = 0.005;
    constexpr double warpPitchSemitoneQuantum = 0.01;
    constexpr double warpPitchMatchEpsilon = 1.0e-7;

    bool bpmMatches(double a, double b) noexcept
    {
        return std::abs(a - b) <= warpBpmMatchEpsilon;
    }

    bool pitchMatches(double a, double b) noexcept
    {
        return std::abs(a - b) <= warpPitchMatchEpsilon;
    }

    bool isNeutralPitchRatio(double ratio) noexcept
    {
        return std::abs(ratio - 1.0) < 1.0e-5;
    }

    bool cacheMatches(const PercussionSound::WarpedCache& cache,
                      double bpm,
                      double pitchRatio) noexcept
    {
        return bpmMatches(cache.bpm, bpm)
            && pitchMatches(cache.pitchRatio, pitchRatio);
    }
}

PercussionSound::PercussionSound(const juce::String& soundName,
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
        DBG("PercussionSound: metadata sampleRate (" << metadata->sampleRate
            << ") differs from audio sampleRate (" << sourceSampleRate
            << ") for " << wavResourceNameForMetadata
            << ". Warp processing will use audio sample rate.");
    }

    if (!metadata->hasTransients())
        metadata->transients.push_back(0.0);

    // Always trust measured audio duration over JSON.
    metadata->lengthSec = lengthInSeconds;

    warpEnabled = metadata->warp;

    DBG("PercussionSound: metadata prepared for " << wavResourceNameForMetadata
        << " (hasTransientJson=" << (metadata->hasTransientJson ? "true" : "false")
        << ", sampleRate=" << metadata->sampleRate
        << ", lengthSec=" << metadata->lengthSec
        << ", transients=" << metadata->transients.size()
        << ", loop=" << (metadata->loop ? "true" : "false")
        << ", ignoreTransientShaper=" << (metadata->ignoreTransientShaper ? "true" : "false")
        << ", warp=" << (metadata->warp ? "true" : "false") << ")");
}

double PercussionSound::quantizeWarpBpm(double hostBpm) noexcept
{
    const double safeBpm = juce::jmax(1.0, hostBpm);
    return std::round(safeBpm / warpBpmQuantum) * warpBpmQuantum;
}

double PercussionSound::quantizeWarpPitchRatio(double pitchRatio) noexcept
{
    const double safePitchRatio = juce::jmax(1.0e-9, pitchRatio);
    const double semitones = 12.0 * std::log2(safePitchRatio);
    const double quantizedSemitones = std::round(semitones / warpPitchSemitoneQuantum)
                                    * warpPitchSemitoneQuantum;

    if (std::abs(quantizedSemitones) < warpPitchSemitoneQuantum * 0.5)
        return 1.0;

    return std::pow(2.0, quantizedSemitones / 12.0);
}

double PercussionSound::warpBaseBpmForHost(double originalBpm, double hostBpm) noexcept
{
    const double safeOriginalBpm = juce::jmax(1.0, originalBpm);
    const double safeHostBpm = juce::jmax(1.0, hostBpm);

    // For very slow host tempos, switch to half-time anchor (153 -> 76.5)
    // so loops speed up instead of stretching too far.
    return (safeHostBpm <= 90.0) ? (safeOriginalBpm * 0.5) : safeOriginalBpm;
}

double PercussionSound::warpTimeRatioForHost(double originalBpm, double hostBpm) noexcept
{
    const double safeHostBpm = juce::jmax(1.0, hostBpm);
    return warpBaseBpmForHost(originalBpm, safeHostBpm) / safeHostBpm;
}

void PercussionSound::setVelocityLayerInfo(int groupIndex,
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

void PercussionSound::collectReadyWarpCache() const
{
    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (!warpCacheFuture.valid())
        return;

    if (warpCacheFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    auto readyCache = warpCacheFuture.get();
    const bool shouldPublish = (pendingWarpCacheBpm > 0.0);
    const bool isPitchedCache = !isNeutralPitchRatio(pendingWarpCachePitchRatio);
    pendingWarpCacheBpm = 0.0;
    pendingWarpCachePitchRatio = 1.0;

    if (shouldPublish && readyCache)
    {
        if (isPitchedCache)
            storePitchedWarpCacheLocked(std::move(readyCache));
        else
            warpCache = std::move(readyCache);
    }
}

std::shared_ptr<PercussionSound::WarpedCache> PercussionSound::getWarpedCache(double hostBpm) const
{
    return getWarpedCache(hostBpm, 1.0);
}

std::shared_ptr<PercussionSound::WarpedCache> PercussionSound::getWarpedCache(double hostBpm,
                                                                              double pitchRatio) const
{
    if (!warpEnabled || metadata == nullptr)
        return nullptr;

    const double bpm = quantizeWarpBpm(hostBpm);
    const double pitch = quantizeWarpPitchRatio(pitchRatio);
    collectReadyWarpCache();

    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (isNeutralPitchRatio(pitch))
    {
        if (warpCache && cacheMatches(*warpCache, bpm, 1.0))
            return warpCache;

        return nullptr;
    }

    if (pitchedWarpCache && cacheMatches(*pitchedWarpCache, bpm, pitch))
        return pitchedWarpCache;

    return nullptr;
}

void PercussionSound::storePitchedWarpCacheLocked(std::shared_ptr<WarpedCache> cache) const
{
    if (!cache)
        return;

    pitchedWarpCache = std::move(cache);
}

void PercussionSound::requestWarpedCacheBuild(double hostBpm) const
{
    requestWarpedCacheBuild(hostBpm, 1.0);
}

void PercussionSound::requestWarpedCacheBuild(double hostBpm, double pitchRatio) const
{
    if (!warpEnabled || metadata == nullptr)
        return;

    const double bpm = quantizeWarpBpm(hostBpm);
    const double pitch = quantizeWarpPitchRatio(pitchRatio);
    collectReadyWarpCache();

    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (isNeutralPitchRatio(pitch))
    {
        if (warpCache && cacheMatches(*warpCache, bpm, 1.0))
            return;
    }
    else if (pitchedWarpCache && cacheMatches(*pitchedWarpCache, bpm, pitch))
    {
        return;
    }

    if (warpCacheFuture.valid())
    {
        if (bpmMatches(pendingWarpCacheBpm, bpm)
            && pitchMatches(pendingWarpCachePitchRatio, pitch))
        {
            return;
        }

        // Keep background work bounded to one build per sound.
        if (warpCacheFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;

        auto readyCache = warpCacheFuture.get();
        const bool shouldPublish = (pendingWarpCacheBpm > 0.0);
        const bool wasPitchedCache = !isNeutralPitchRatio(pendingWarpCachePitchRatio);
        pendingWarpCacheBpm = 0.0;
        pendingWarpCachePitchRatio = 1.0;

        if (shouldPublish && readyCache)
        {
            if (wasPitchedCache)
                storePitchedWarpCacheLocked(std::move(readyCache));
            else
                warpCache = std::move(readyCache);
        }

        if (isNeutralPitchRatio(pitch))
        {
            if (warpCache && cacheMatches(*warpCache, bpm, 1.0))
                return;
        }
        else if (pitchedWarpCache && cacheMatches(*pitchedWarpCache, bpm, pitch))
        {
            return;
        }
    }

    if (isNeutralPitchRatio(pitch))
        warpCache.reset();
    else
        pitchedWarpCache.reset();

    pendingWarpCacheBpm = bpm;
    pendingWarpCachePitchRatio = pitch;

    warpCacheFuture = std::async(std::launch::async, [this, bpm, pitch]() -> std::shared_ptr<WarpedCache>
    {
        auto builtCache = renderWarpedCache(bpm, pitch);
        if (!builtCache)
            return {};

        builtCache->bpm = bpm;
        builtCache->pitchRatio = pitch;
        return std::shared_ptr<WarpedCache>(std::move(builtCache));
    });
}

bool PercussionSound::isWarpCacheBuildInFlight() const
{
    if (!warpEnabled || metadata == nullptr)
        return false;

    collectReadyWarpCache();
    std::lock_guard<std::mutex> lock(warpCacheMutex);

    if (!warpCacheFuture.valid())
        return false;

    return warpCacheFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

void PercussionSound::clearWarpedCache() const
{
    collectReadyWarpCache();
    std::lock_guard<std::mutex> lock(warpCacheMutex);
    pendingWarpCacheBpm = 0.0;
    pendingWarpCachePitchRatio = 1.0;
    warpCache.reset();
    pitchedWarpCache.reset();
}

std::unique_ptr<PercussionSound::WarpedCache> PercussionSound::renderWarpedCache(double hostBpm,
                                                                                 double pitchRatio) const
{
    if (metadata == nullptr)
        return nullptr;

    const int srcChannels = juce::jlimit(1, 2, data.getNumChannels());
    const int srcSamples  = data.getNumSamples();

    if (srcSamples <= 0)
        return nullptr;

    auto cache = std::make_unique<WarpedCache>();
    cache->bpm = hostBpm;
    cache->pitchRatio = quantizeWarpPitchRatio(pitchRatio);
    // Warp cache must always use actual audio sample rate, not JSON metadata sampleRate.
    // JSON sampleRate can differ from the loaded WAV rate and would bias pitch.
    cache->sourceSampleRate = juce::jmax(1.0, sourceSampleRate);

    const double timeRatio = warpTimeRatioForHost(originalBpm, hostBpm);
    cache->timeRatio = timeRatio;
    const int targetOutputSamples = juce::jmax(1, (int) std::llround((double) srcSamples * timeRatio));

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
        cache->pitchRatio);

    // RubberBand's offline pitch shift stretches to timeRatio * pitchRatio,
    // then resamples back to timeRatio. Key-frame targets must live in that
    // pre-resample stretch domain or pitched loop transients drift in tempo.
    const double keyFrameTargetRatio = timeRatio * cache->pitchRatio;

    // Key-frame map from transient list (source frame -> pre-resample stretched frame)
    std::map<size_t, size_t> keyFrames;
    keyFrames[0] = 0;

    const double sr = cache->sourceSampleRate;
    for (double t : metadata->transients)
    {
        if (t < 0.0)
            continue;

        const size_t srcFrame = (size_t) juce::jmax(0.0, std::floor(t * sr));
        const size_t dstFrame = (size_t) std::llround((double) srcFrame * keyFrameTargetRatio);
        keyFrames[srcFrame] = dstFrame;
    }

    const size_t lastSrc = (size_t) srcSamples;
    keyFrames[lastSrc] = (size_t) std::llround((double) lastSrc * keyFrameTargetRatio);

    stretcher.setTimeRatio(timeRatio);
    stretcher.setPitchScale(cache->pitchRatio);
    stretcher.setExpectedInputDuration((size_t) srcSamples);
    stretcher.setKeyFrameMap(keyFrames);

    std::array<const float*, 2> inPtrs {
        data.getReadPointer(0),
        (srcChannels > 1) ? data.getReadPointer(1) : data.getReadPointer(0)
    };

    stretcher.study(inPtrs.data(), (size_t) srcSamples, true);
    stretcher.process(inPtrs.data(), (size_t) srcSamples, true);

    const size_t pad = (size_t)(stretcher.getStartDelay() + stretcher.getLatency() + 128);
    size_t estimatedOut = (size_t) targetOutputSamples + pad;
    estimatedOut = juce::jmax<size_t>(estimatedOut, (size_t) srcSamples);

    cache->buffer.setSize(srcChannels, (int) estimatedOut, false, true, true);

    std::array<float*, 2> outPtrs {};
    size_t written = 0;
    int safety = 0;

    while (safety < 4096)
    {
        const int availableFrames = stretcher.available();
        if (availableFrames <= 0)
            break;

        const size_t available = (size_t) availableFrames;
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

    if (written < (size_t) targetOutputSamples)
    {
        const int clearStart = (int) written;
        cache->buffer.clear(clearStart, targetOutputSamples - clearStart);
    }

    cache->buffer.setSize(srcChannels, targetOutputSamples, true, true, true);
    return cache;
}

bool PercussionSound::appliesToNote(int midiNoteNumber)
{
    return midiNotes[midiNoteNumber];
}

bool PercussionSound::appliesToChannel(int midiChannel)
{
    juce::ignoreUnused(midiChannel);
    return true; // applies to all channels
}
