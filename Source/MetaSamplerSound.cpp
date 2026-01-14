#include "MetaSamplerSound.h"

MetaSamplerSound::MetaSamplerSound(const juce::String& soundName,
                                   juce::AudioFormatReader& source,
                                   const juce::BigInteger& notes,
                                   int midiNoteForNormalPitch,
                                   double attackTimeSeconds,
                                   double releaseTimeSeconds,
                                   double maxSampleLengthSeconds,
                                   const juce::String& wavResourceNameForMetadata)
    : name(soundName),
      sourceSampleRate(source.sampleRate),
      midiRootNote(midiNoteForNormalPitch),
      midiNotes(notes),
      attackTime(attackTimeSeconds),
      releaseTime(releaseTimeSeconds)
{
    if (sourceSampleRate <= 0.0)
        sourceSampleRate = 44100.0;

    auto numSamples = static_cast<int>(juce::jmin((juce::int64) (sourceSampleRate * maxSampleLengthSeconds),
                                                  source.lengthInSamples));

    data.setSize((int) source.numChannels, numSamples);
    source.read(&data, 0, numSamples, 0, true, true);

    lengthInSeconds = (double) numSamples / sourceSampleRate;
    attack = attackTimeSeconds;
    release = releaseTimeSeconds;

    // load transient metadata from BinaryData (if present)
    metadata = loadMetadataForResource(wavResourceNameForMetadata);

    if (metadata)
        DBG("MetaSamplerSound: loaded transient metadata for " << wavResourceNameForMetadata);
    else
        DBG("MetaSamplerSound: no metadata for " << wavResourceNameForMetadata);
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
