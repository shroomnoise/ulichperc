#include "SampleSpecificPitchCache.h"

#include <cmath>

SampleSpecificPitchCache::SampleSpecificPitchCache()
{
    reset();
}

void SampleSpecificPitchCache::reset() noexcept
{
    for (auto& pitchRatio : pitchRatioByMidiNote)
        pitchRatio.store(1.0f, std::memory_order_relaxed);
}

void SampleSpecificPitchCache::setPitchSemitonesForMidiNote(int midiNote, float semitones) noexcept
{
    if (midiNote < 0 || midiNote >= midiNoteCount || !std::isfinite(semitones))
        return;

    const auto pitchRatio = static_cast<float>(std::pow(2.0, static_cast<double>(semitones) / 12.0));
    if (!std::isfinite(pitchRatio) || pitchRatio <= 0.0f)
        return;

    pitchRatioByMidiNote[(size_t) midiNote].store(pitchRatio, std::memory_order_relaxed);
}

float SampleSpecificPitchCache::getPitchRatioForMidiNote(int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote >= midiNoteCount)
        return 1.0f;

    const float pitchRatio = pitchRatioByMidiNote[(size_t) midiNote].load(std::memory_order_relaxed);
    return (std::isfinite(pitchRatio) && pitchRatio > 0.0f) ? pitchRatio : 1.0f;
}
