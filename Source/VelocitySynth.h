
#pragma once
#include <JuceHeader.h>
#include "MetaSamplerSound.h"

class VelocitySynth : public juce::Synthesiser
{
public:
    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
    {
        // Only special-case the first MIDI note where you stacked 5 layers
        if (midiNoteNumber != 60)
        {
            juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
            return;
        }

        MetaSamplerSound* best = nullptr;

        // Find the sound on note 60 whose velocity range contains the incoming velocity
        for (int i = 0; i < getNumSounds(); ++i)
        {
            auto* s = dynamic_cast<MetaSamplerSound*>(getSound(i).get());
            if (s == nullptr)
                continue;

            if (!s->appliesToNote(midiNoteNumber) || !s->appliesToChannel(midiChannel))
                continue;

            if (s->matchesVelocity(velocity))
            {
                best = s;
                break;
            }
        }

        // If no zone matched (gaps), fall back to default behavior
        if (best == nullptr)
        {
            juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
            return;
        }

        // Start a voice with the chosen sound.
        if (auto* v = findFreeVoice(best, midiChannel, midiNoteNumber, true))
        {
            // IMPORTANT: pass the real velocity through — your MetaSamplerVoice volume logic stays intact
            startVoice(v, best, midiChannel, midiNoteNumber, velocity);
            return;
        }

        // If no voice is available, fall back
        juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
    }

private:
    juce::SynthesiserVoice* findFreeVoice(juce::SynthesiserSound* sound,
                                          int /*midiChannel*/,
                                          int /*midiNoteNumber*/,
                                          bool stealIfNoneAvailable)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* v = getVoice(i);
            if (v != nullptr && !v->isVoiceActive() && v->canPlaySound(sound))
                return v;
        }

        if (!stealIfNoneAvailable)
            return nullptr;

        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* v = getVoice(i);
            if (v != nullptr && v->canPlaySound(sound))
                return v;
        }

        return nullptr;
    }
};
