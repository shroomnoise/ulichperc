#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

struct SampleMetadata;

class PunchEnvelope final
{
public:
    static float getGain(double playbackTimeSec,
                         const SampleMetadata* metadata,
                         double timeRatio,
                         float punchAmount) noexcept;

private:
    static float getEnvelopeValue(double playbackTimeSec,
                                  const SampleMetadata* metadata,
                                  double timeRatio) noexcept;
    static float getEnvelopeValueForTransient(double playbackTimeSec,
                                              double transientStartSec) noexcept;

    static constexpr double preRiseSeconds = 0.01;
    static constexpr double decaySeconds = 0.02;
    static constexpr float maximumAdditionalDb = 20.0f;
};
