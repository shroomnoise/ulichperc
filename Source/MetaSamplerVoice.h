#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "MetaSamplerSound.h"
#include <rubberband/RubberBandStretcher.h>

// Voice that plays MetaSamplerSound and shortens sustain tails
// based on transient JSON metadata.
class MetaSamplerVoice : public juce::SynthesiserVoice
{
public:
    MetaSamplerVoice();

    // Connects the voice to the "sustainShorten" parameter (0..1)
    void setSustainParam(std::atomic<float>* p);

    // Only play MetaSamplerSound instances
    bool canPlaySound(juce::SynthesiserSound* sound) override;

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound,
                   int currentPitchWheelPosition) override;

    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample,
                         int numSamples) override;
    void setHostBpmParam(std::atomic<double>* p) { hostBpmParam = p; }
    void setWarpEnabledParam(std::atomic<bool>* p) { warpEnabledParam = p; }

private:
    MetaSamplerSound* currentSound = nullptr;

    // Playback state in source sample domain
    double sourceSamplePosition = 0.0;  // index into source buffer
    double pitchRatio           = 1.0;  // how many source samples per output sample

    // Simple ADSR for amplitude (attack/release from MetaSamplerSound)
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    // Pointer to APVTS parameter for sustain shortening
    std::atomic<float>* sustainAmountParam = nullptr;

    // Transient metadata for current sound
    const SampleMetadata* metadata = nullptr;
    int currentTransientIndex = 0;

    // Per-sample gain inside sustain zones (shortens tail as knob increases)
    float computeSustainGain(double timeSec, float amount);
    float velocityGain = 1.0f;
    
    std::atomic<double>* hostBpmParam = nullptr;
    std::atomic<bool>* warpEnabledParam = nullptr;

    bool isWarping = false;
    double currentTimeRatio = 1.0;
    int rbSrcPos = 0;
    bool rbEnded = false;

    std::unique_ptr<RubberBand::RubberBandStretcher> rb;
    size_t rbSampleRate = 0;
    int rbChannels = 0;
    juce::AudioBuffer<float> rbIn, rbOut;
    std::array<const float*, 2> rbInPtrs {};
    std::array<float*, 2> rbOutPtrs {};
    double warpedOutputTimeSec = 0.0;
};
