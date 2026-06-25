#pragma once

#include <array>
#include <atomic>

class SampleSpecificPitchCache final
{
public:
    SampleSpecificPitchCache();

    void reset() noexcept;
    void setPitchSemitonesForMidiNote(int midiNote, float semitones) noexcept;
    float getPitchRatioForMidiNote(int midiNote) const noexcept;

private:
    static constexpr int midiNoteCount = 128;

    std::array<std::atomic<float>, midiNoteCount> pitchRatioByMidiNote;
};
