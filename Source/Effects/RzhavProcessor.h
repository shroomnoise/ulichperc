#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

class RzhavProcessor
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // amount is expected in 0..1. At 0 this is a true bypass.
    void process(juce::AudioBuffer<float>& buffer, float amount) noexcept;

private:
    static constexpr int maxSupportedChannels = 2;

    double currentSampleRate = 0.0;
    std::array<float, maxSupportedChannels> heldPerChannel {};
    std::array<int, maxSupportedChannels> phasePerChannel {};
};
