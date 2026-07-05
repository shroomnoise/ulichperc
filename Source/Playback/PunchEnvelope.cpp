#include "PunchEnvelope.h"

#include <cmath>
#include <limits>

#include "SampleMetadata.h"

float PunchEnvelope::getGain(double playbackTimeSec,
                             const SampleMetadata* metadata,
                             double timeRatio,
                             float punchAmount) noexcept
{
    if (!std::isfinite(playbackTimeSec)
        || !std::isfinite(timeRatio)
        || !std::isfinite(punchAmount))
    {
        return 1.0f;
    }

    const float limitedAmount = juce::jlimit(0.0f, 1.0f, punchAmount);
    if (limitedAmount <= 0.0f)
        return 1.0f;

    const float envelopeValue = getEnvelopeValue(playbackTimeSec, metadata, timeRatio);
    if (envelopeValue <= 0.0f)
        return 1.0f;

    const float additionalDb = maximumAdditionalDb * limitedAmount * envelopeValue;
    return juce::Decibels::decibelsToGain(additionalDb);
}

float PunchEnvelope::getEnvelopeValue(double playbackTimeSec,
                                      const SampleMetadata* metadata,
                                      double timeRatio) noexcept
{
    if (metadata == nullptr || metadata->transients.empty())
        return getEnvelopeValueForTransient(playbackTimeSec, 0.0);

    const double safeTimeRatio = juce::jmax(1.0e-9, timeRatio);
    const auto& transients = metadata->transients;
    int low = 0;
    int high = static_cast<int>(transients.size());

    while (low < high)
    {
        const int mid = low + (high - low) / 2;
        const double transient = transients[(size_t) mid];
        const double transientTimeSec = std::isfinite(transient) ? juce::jmax(0.0, transient) * safeTimeRatio
                                                                 : std::numeric_limits<double>::infinity();

        if (transientTimeSec <= playbackTimeSec)
            low = mid + 1;
        else
            high = mid;
    }

    float envelopeValue = 0.0f;

    if (low < static_cast<int>(transients.size()))
    {
        const double transient = transients[(size_t) low];
        if (std::isfinite(transient))
        {
            const double transientTimeSec = juce::jmax(0.0, transient) * safeTimeRatio;
            envelopeValue = juce::jmax(envelopeValue,
                                       getEnvelopeValueForTransient(playbackTimeSec, transientTimeSec));
        }
    }

    for (int index = low - 1; index >= 0; --index)
    {
        const double transient = transients[(size_t) index];
        if (!std::isfinite(transient))
            continue;

        const double transientTimeSec = juce::jmax(0.0, transient) * safeTimeRatio;
        envelopeValue = juce::jmax(envelopeValue,
                                   getEnvelopeValueForTransient(playbackTimeSec, transientTimeSec));
        break;
    }

    return envelopeValue;
}

float PunchEnvelope::getEnvelopeValueForTransient(double playbackTimeSec,
                                                  double transientStartSec) noexcept
{
    const double preRiseStartSec = juce::jmax(0.0, transientStartSec - preRiseSeconds);

    if (playbackTimeSec < transientStartSec)
    {
        const double riseDurationSec = transientStartSec - preRiseStartSec;
        if (riseDurationSec <= 0.0 || playbackTimeSec < preRiseStartSec)
            return 0.0f;

        return static_cast<float>(juce::jlimit(0.0,
                                               1.0,
                                               (playbackTimeSec - preRiseStartSec) / riseDurationSec));
    }

    const double elapsed = playbackTimeSec - transientStartSec;
    if (elapsed < 0.0 || elapsed >= decaySeconds)
        return 0.0f;

    return static_cast<float>(1.0 - (elapsed / decaySeconds));
}
