#include "SampleSpecificRealtimeCache.h"

#include <cmath>

#include <juce_core/juce_core.h>

SampleSpecificRealtimeCache::SampleSpecificRealtimeCache()
{
    reset();
}

void SampleSpecificRealtimeCache::reset() noexcept
{
    for (auto& pitchRatio : pitchRatioByMidiNote)
        pitchRatio.store(1.0f, std::memory_order_relaxed);

    for (auto& punchAmount : punchAmountByMidiNote)
        punchAmount.store(0.0f, std::memory_order_relaxed);
}

void SampleSpecificRealtimeCache::setPitchSemitonesForMidiNote(int midiNote, float semitones) noexcept
{
    if (midiNote < 0 || midiNote >= midiNoteCount || !std::isfinite(semitones))
        return;

    const auto pitchRatio = static_cast<float>(std::pow(2.0, static_cast<double>(semitones) / 12.0));
    if (!std::isfinite(pitchRatio) || pitchRatio <= 0.0f)
        return;

    pitchRatioByMidiNote[(size_t) midiNote].store(pitchRatio, std::memory_order_relaxed);
}

float SampleSpecificRealtimeCache::getPitchRatioForMidiNote(int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote >= midiNoteCount)
        return 1.0f;

    const float pitchRatio = pitchRatioByMidiNote[(size_t) midiNote].load(std::memory_order_relaxed);
    return (std::isfinite(pitchRatio) && pitchRatio > 0.0f) ? pitchRatio : 1.0f;
}

void SampleSpecificRealtimeCache::setPunchAmountForMidiNote(int midiNote, float amount) noexcept
{
    if (midiNote < 0 || midiNote >= midiNoteCount || !std::isfinite(amount))
        return;

    punchAmountByMidiNote[(size_t) midiNote].store(juce::jlimit(0.0f, 1.0f, amount),
                                                   std::memory_order_relaxed);
}

float SampleSpecificRealtimeCache::getPunchAmountForMidiNote(int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote >= midiNoteCount)
        return 0.0f;

    const float amount = punchAmountByMidiNote[(size_t) midiNote].load(std::memory_order_relaxed);
    return std::isfinite(amount) ? juce::jlimit(0.0f, 1.0f, amount) : 0.0f;
}
