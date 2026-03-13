#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <future>
#include <memory>
#include <mutex>

#include "SampleMetadata.h"

class MetaSamplerSound : public juce::SynthesiserSound
{
public:
    struct WarpedCache
    {
        double bpm = 0.0;
        double timeRatio = 1.0;
        double sourceSampleRate = 44100.0;
        juce::AudioBuffer<float> buffer;
    };

    MetaSamplerSound(const juce::String& soundName,
                     juce::AudioFormatReader& source,
                     const juce::BigInteger& notes,
                     int midiNoteForNormalPitch,
                     double attackTimeSeconds,
                     double releaseTimeSeconds,
                     double maxSampleLengthSeconds,
                     const juce::String& wavResourceNameForMetadata,
                     double originalBpmIn);

    // SynthesiserSound overrides
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;

    // Audio data access
    const juce::AudioBuffer<float>& getAudioData() const noexcept { return data; }
    double getSourceSampleRate() const noexcept { return sourceSampleRate; }

    // Warp info
    bool isWarpEnabled() const noexcept { return warpEnabled; }
    double getOriginalBpm() const noexcept { return originalBpm; }

    static double quantizeWarpBpm(double hostBpm) noexcept;

    std::shared_ptr<WarpedCache> getWarpedCache(double hostBpm) const;
    void requestWarpedCacheBuild(double hostBpm) const;
    bool isWarpCacheBuildInFlight() const;
    void clearWarpedCache() const;

    // Transient metadata (used by MetaSamplerVoice)
    std::unique_ptr<SampleMetadata> metadata = nullptr;

private:
    void collectReadyWarpCache() const;
    std::unique_ptr<WarpedCache> renderWarpedCache(double hostBpm) const;

    juce::String name;

    juce::AudioBuffer<float> data;

    double sourceSampleRate = 48000.0;
    int midiRootNote = 60;

    juce::BigInteger midiNotes;

    double attackTime  = 0.001;
    double releaseTime = 0.05;

    double lengthInSeconds = 0.0;
    double attack  = 0.001;
    double release = 0.05;

    // Warp configuration
    bool warpEnabled = false;
    double originalBpm = 153.0;

    // Cache for pre-rendered warped audio (one BPM at a time)
    mutable std::shared_ptr<WarpedCache> warpCache;
    mutable std::mutex warpCacheMutex;
    mutable double pendingWarpCacheBpm = 0.0;
    mutable std::future<std::shared_ptr<WarpedCache>> warpCacheFuture;
};
