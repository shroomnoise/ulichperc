#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "PercussionSound.h"
#include "Playback/NoteStartDeclicker.h"
#include "Playback/RealtimeWarpPlayer.h"
#include "Playback/SamplePlaybackRenderer.h"
#include "Playback/SustainTailShaper.h"

// Voice that plays PercussionSound and shortens sustain tails
// based on transient JSON metadata.
class PercussionVoice : public juce::SynthesiserVoice
{
public:
    PercussionVoice();

    // Connects the voice to the "sustainShorten" parameter (0..1)
    void setSustainParam(std::atomic<float>* p);

    // Only play PercussionSound instances
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
    void setHostBpmMovingParam(std::atomic<bool>* p) { hostBpmMovingParam = p; }

private:
    void beginPlayback(float velocity);
    void clearActivePlayback();
    void resetWarpFlags();
    void maybeSwitchWarpCacheToRealtime();
    float getSustainAmount() const noexcept;

    PercussionSound* currentSound = nullptr;
    std::shared_ptr<PercussionSound::WarpedCache> activeWarpCache;
    const juce::AudioBuffer<float>* activeBuffer = nullptr;
    const SampleMetadata* metadata = nullptr;

    SamplePlaybackRenderer::State playbackState;
    SamplePlaybackRenderer sampleRenderer;
    RealtimeWarpPlayer realtimeWarpPlayer;
    SustainTailShaper sustainShaper;
    NoteStartDeclicker noteStartDeclicker;

    // Simple ADSR for amplitude (attack/release from PercussionSound)
    juce::ADSR adsr;

    // Pointer to APVTS parameter for sustain shortening
    std::atomic<float>* sustainAmountParam = nullptr;
    float velocityGain = 1.0f;
    
    std::atomic<double>* hostBpmParam = nullptr;
    std::atomic<bool>* warpEnabledParam = nullptr;
    std::atomic<bool>* hostBpmMovingParam = nullptr;

    bool isWarping = false;
    bool isRealtimeWarping = false;
};
