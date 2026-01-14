#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TransientMetadata.h"

class MetaSamplerSound : public juce::SynthesiserSound
{
public:
    MetaSamplerSound(const juce::String& name,
                     juce::AudioFormatReader& source,
                     const juce::BigInteger& midiNotes,
                     int midiNoteForNormalPitch,
                     double attackTimeSeconds,
                     double releaseTimeSeconds,
                     double maxSampleLengthSeconds,
                     const juce::String& wavResourceNameForMetadata);

    bool appliesToNote(int midiNoteNumber) override;
    bool appliesToChannel(int midiChannel) override;

    inline const juce::AudioBuffer<float>& getAudioData() const { return data; }
    inline double getSourceSampleRate() const { return sourceSampleRate; }
    inline int getRootMidiNote() const { return midiRootNote; }

    inline double getAttackTimeSeconds() const { return attackTime; }
    inline double getReleaseTimeSeconds() const { return releaseTime; }

    std::unique_ptr<SampleMetadata> metadata;

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
};
