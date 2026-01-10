#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TransientMetadata.h"

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
                                   int minVelocityIn,
                                   int maxVelocityIn);

    bool appliesToNote(int midiNoteNumber) override;
    bool appliesToChannel(int midiChannel) override;

    inline const juce::AudioBuffer<float>& getAudioData() const { return data; }
    inline double getSourceSampleRate() const { return sourceSampleRate; }
    inline int getRootMidiNote() const { return midiRootNote; }

    inline double getAttackTimeSeconds() const { return attackTime; }
    inline double getReleaseTimeSeconds() const { return releaseTime; }

    std::unique_ptr<SampleMetadata> metadata;

    int getMinVelocity() const noexcept { return minVelocity; } // 0..127
    int getMaxVelocity() const noexcept { return maxVelocity; } // 0..127

    bool matchesVelocity (float v01) const noexcept
    {
        const int v = juce::jlimit(0, 127, (int) std::round(v01 * 127.0f));
        return v >= minVelocity && v <= maxVelocity;
    }

private:
    juce::String name;
    juce::AudioBuffer<float> data;
    double sourceSampleRate = 44100.0;
    int midiRootNote = 60;
    juce::BigInteger midiNotes;
    double lengthInSeconds = 0.0;
    double attack = 0.0;
    double release = 0.0;

    double attackTime = 0.0;
    double releaseTime = 0.0;

    int minVelocity = 0;
    int maxVelocity = 127;
};
