#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MetaSamplerSound.h"

class LayeredSynthesiser : public juce::Synthesiser
{
public:
    void clearLayerMappings();

    void registerLayeredSound(MetaSamplerSound* sound,
                              int midiNote,
                              int velocityGroupIndex,
                              int variationIndex);

    void finalizeLayerMappings();

    void noteOn(int midiChannel,
                int midiNoteNumber,
                float velocity) override;

private:
    struct RegisteredSound
    {
        MetaSamplerSound* sound = nullptr;
        int velocityGroupIndex = 1;
        int variationIndex = 1;
    };

    struct VelocityGroup
    {
        int index = 1;
        int minVelocity = 1;
        int maxVelocity = 127;
        std::vector<MetaSamplerSound*> sounds;
    };

    struct NoteLayers
    {
        std::vector<VelocityGroup> groups;
    };

    static std::pair<int, int> computeVelocityRange(int groupIndex, int groupCount) noexcept;
    static int velocityToMidi(float velocity) noexcept;
    static int chooseGroupForVelocity(const NoteLayers& layers, int midiVelocity) noexcept;
    MetaSamplerSound* chooseVariation(int midiNote,
                                      const NoteLayers& layers,
                                      int preferredGroupIdx) noexcept;

    std::unordered_map<int, std::vector<RegisteredSound>> registeredByMidiNote;
    std::unordered_map<int, NoteLayers> noteLayers;
    std::unordered_map<int, std::vector<int>> lastVariationIndexByNote;
    juce::Random rng;
};
