#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

#include "SampleMetadata.h"

class MetaSamplerSound : public juce::SynthesiserSound
{
public:
    MetaSamplerSound(const juce::String& soundName,
                     juce::AudioFormatReader& source,
                     const juce::BigInteger& notes,
                     int midiNoteForNormalPitch,
                     double attackTimeSeconds,
                     double releaseTimeSeconds,
                     double maxSampleLengthSeconds,
                     const juce::String& wavResourceNameForMetadata,
                     bool warpEnabledIn,
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

    // Transient metadata (used by MetaSamplerVoice)
    std::unique_ptr<SampleMetadata> metadata = nullptr;

private:
    juce::String name;

    juce::AudioBuffer<float> data;

    double sourceSampleRate = 44100.0;
    int midiRootNote = 60;

    juce::BigInteger midiNotes;

    double attackTime  = 0.001;
    double releaseTime = 0.05;

    double lengthInSeconds = 0.0;
    double attack  = 0.001;
    double release = 0.05;

    // Warp configuration
    bool warpEnabled = false;
    double originalBpm = 150.0;
};
