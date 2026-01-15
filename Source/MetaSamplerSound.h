#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

struct SampleMetadata
{
    double sampleRate = 44100.0;
    double lengthSec = 0.0;
    std::vector<double> transients; // transient start times in seconds
};

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

    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;

    const juce::AudioBuffer<float>& getAudioData() const noexcept { return data; }
    double getSourceSampleRate() const noexcept { return sourceSampleRate; }

    bool isWarpEnabled() const noexcept { return warpEnabled; }
    double getOriginalBpm() const noexcept { return originalBpm; }

    // You already use this in MetaSamplerVoice.cpp:
    std::unique_ptr<SampleMetadata> metadata;

private:
    std::unique_ptr<SampleMetadata> loadMetadataForResource (const juce::String& wavResourceName);

    juce::String name;

    juce::AudioBuffer<float> data;

    double sourceSampleRate = 44100.0;
    int midiRootNote = 60;

    juce::BigInteger midiNotes;

    double attackTime = 0.001;
    double releaseTime = 0.05;

    double lengthInSeconds = 0.0;
    double attack = 0.001;
    double release = 0.05;

    // NEW: warp config
    bool warpEnabled = false;
    double originalBpm = 150.0;
};
