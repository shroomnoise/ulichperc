#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* bitDepthParam = nullptr;
    std::atomic<float>* sustainShortenParam = nullptr;
    std::atomic<float>* warpParamRaw = nullptr;
    
    std::vector<float> heldPerChannel;
    std::vector<int> phasePerChannel;
    std::atomic<float>* rzhavParam = nullptr;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    std::atomic<float>* rateDivideParam = nullptr;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    bool requestNextWarpCacheForBpm(double hostBpm, int& soundIndex) const;
    bool areWarpCachesReadyForBpm(double hostBpm) const;
    int countWarpCacheBuildsInFlight() const;
    void clearWarpCaches();

    //==============================================================================
    juce::Synthesiser sampler;
    std::unique_ptr<juce::AudioFormatReader> reader;
    std::atomic<double> hostBpmAtomic { 153.0 };
    std::atomic<bool> warpEnabledAtomic { true };
    std::atomic<bool> hostBpmMovingAtomic { false };
    bool lastWarpEnabled = true;
    double lastHostBpmForMotion = 0.0;
    double lastHostBpmChangeSec = 0.0;
    bool hasHostBpmForMotion = false;
    double lastObservedWarpBpm = 0.0;
    double pendingWarpPrewarmBpm = 0.0;
    double warpBpmDebounceUntilSec = 0.0;
    double nextWarpPrewarmRetrySec = 0.0;
    int pendingWarpPrewarmSoundIndex = 0;
    bool hasObservedWarpBpm = false;
    bool hasPendingWarpPrewarm = false;
    static constexpr double warpPrewarmDebounceSec = 0.12;      // 120 ms stable BPM required
    static constexpr double warpPrewarmRetryIntervalSec = 0.03; // retry pending build every 30 ms
    static constexpr double warpBpmChangeEpsilon = 0.01;        // ignore tiny BPM jitter
    static constexpr int warpPrewarmMaxInFlightBuilds = 2;      // faster warmup with controlled CPU
    static constexpr double hostBpmMotionEpsilon = 0.01;        // detect host BPM movement
    static constexpr double hostBpmMotionHoldSec = 0.25;        // keep "moving" state briefly

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
