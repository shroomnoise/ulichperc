#pragma once

#include <array>
#include <atomic>

class SampleSpecificRealtimeCache final
{
public:
    SampleSpecificRealtimeCache();

    void reset() noexcept;
    void setPitchSemitonesForMidiNote(int midiNote, float semitones) noexcept;
    float getPitchRatioForMidiNote(int midiNote) const noexcept;
    void setPunchAmountForMidiNote(int midiNote, float amount) noexcept;
    float getPunchAmountForMidiNote(int midiNote) const noexcept;

private:
    static constexpr int midiNoteCount = 128;

    std::array<std::atomic<float>, midiNoteCount> pitchRatioByMidiNote;
    std::array<std::atomic<float>, midiNoteCount> punchAmountByMidiNote;
};
