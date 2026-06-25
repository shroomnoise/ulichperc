#pragma once

#include "PercussionSynthesiser.h"

#include <vector>

namespace PercussionSampleLibrary
{
    struct SampleGroupInfo
    {
        int noteIndex = 0;
        int pitchIndex = 1;
        int mappedNoteIndex = 0;
        int midiNote = 0;
    };

    void loadEmbeddedSamples(PercussionSynthesiser& sampler,
                             double originalBpm,
                             std::vector<SampleGroupInfo>* loadedSampleGroups = nullptr);
}
