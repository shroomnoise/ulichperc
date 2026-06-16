#include "PercussionVoice.h"

#include "Playback/VelocityLayerGain.h"

#include <cmath>

PercussionVoice::PercussionVoice()
{
    adsr.setParameters({ 0.00f, 0.0f, 1.0f, 0.06f });
}

void PercussionVoice::setSustainParam(std::atomic<float>* p)
{
    sustainAmountParam = p;
}

bool PercussionVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PercussionSound*>(sound) != nullptr;
}

void PercussionVoice::startNote(int midiNoteNumber,
                                float velocity,
                                juce::SynthesiserSound* sound,
                                int currentPitchWheelPosition)
{
    juce::ignoreUnused(midiNoteNumber, currentPitchWheelPosition);

    currentSound = dynamic_cast<PercussionSound*>(sound);
    beginPlayback(velocity);
}

void PercussionVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        // If sample has no transient JSON, treat it as one-shot and ignore note-off.
        if (metadata != nullptr && !metadata->hasTransientJson)
            return;

        adsr.noteOff();
        return;
    }

    adsr.reset();
    clearActivePlayback();
}

void PercussionVoice::pitchWheelMoved(int) {}
void PercussionVoice::controllerMoved(int, int) {}

void PercussionVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                      int startSample,
                                      int numSamples)
{
    if (currentSound == nullptr)
        return;

    if (activeBuffer == nullptr)
        activeBuffer = &currentSound->getAudioData();

    maybeSwitchWarpCacheToRealtime();

    const auto& data = *activeBuffer;
    const int sourceNumSamples = data.getNumSamples();
    const int sourceNumChans = data.getNumChannels();
    const bool loopWhileHeld = (metadata != nullptr && metadata->loop && isKeyDown());

    if (sourceNumSamples <= 0 || sourceNumChans <= 0 || !adsr.isActive())
    {
        clearActivePlayback();
        return;
    }

    const float sustainAmount = getSustainAmount();
    const bool hasTransientData = (metadata != nullptr && !metadata->transients.empty());
    const float sustainMakeupGain = SustainTailShaper::getMakeupGain(sustainAmount, hasTransientData);

    if (isRealtimeWarping && realtimeWarpPlayer.isReady() && hostBpmParam != nullptr)
    {
        const double hostBpm = juce::jmax(1.0, hostBpmParam->load(std::memory_order_relaxed));
        const auto result = realtimeWarpPlayer.render(outputBuffer,
                                                      startSample,
                                                      numSamples,
                                                      *currentSound,
                                                      data,
                                                      playbackState.activeSourceSampleRate,
                                                      juce::jmax(1.0, getSampleRate()),
                                                      hostBpm,
                                                      loopWhileHeld,
                                                      adsr,
                                                      velocityGain,
                                                      sustainAmount,
                                                      sustainMakeupGain,
                                                      sustainShaper,
                                                      noteStartDeclicker);

        if (result.finished)
            clearActivePlayback();

        return;
    }

    const auto result = sampleRenderer.render(outputBuffer,
                                             startSample,
                                             numSamples,
                                             data,
                                             playbackState,
                                             loopWhileHeld,
                                             adsr,
                                             velocityGain,
                                             sustainAmount,
                                             sustainMakeupGain,
                                             sustainShaper,
                                             noteStartDeclicker);

    if (result.finished)
        clearActivePlayback();
}

void PercussionVoice::beginPlayback(float velocity)
{
    activeWarpCache.reset();
    activeBuffer = nullptr;
    metadata = nullptr;
    playbackState = {};
    playbackState.activeSourceSampleRate = 44100.0;
    sustainShaper.setMetadata(nullptr);
    noteStartDeclicker.reset();
    realtimeWarpPlayer.reset();
    resetWarpFlags();

    velocityGain = VelocityLayerGain::calculate(velocity, currentSound);

    if (currentSound == nullptr)
    {
        clearActivePlayback();
        return;
    }

    metadata = currentSound->metadata.get();
    sustainShaper.setMetadata(metadata);

    double sourceSampleRate = currentSound->getSourceSampleRate();
    double playbackSampleRate = getSampleRate();

    if (sourceSampleRate <= 0.0)
        sourceSampleRate = 44100.0;
    if (playbackSampleRate <= 0.0)
        playbackSampleRate = 44100.0;

    activeBuffer = &currentSound->getAudioData();
    playbackState.activeSourceSampleRate = sourceSampleRate;
    playbackState.sourceSamplePosition = 0.0;
    playbackState.pitchRatio = playbackState.activeSourceSampleRate / playbackSampleRate;
    playbackState.currentTimeRatio = 1.0;
    playbackState.usingWarpCache = false;

    adsr.setSampleRate(playbackSampleRate);
    adsr.noteOn();

    const bool warpToggle = (warpEnabledParam != nullptr)
                         && warpEnabledParam->load(std::memory_order_relaxed);
    const bool bpmIsMoving = (hostBpmMovingParam != nullptr)
                          && hostBpmMovingParam->load(std::memory_order_relaxed);
    const double hostBpm = (hostBpmParam != nullptr)
                             ? juce::jmax(1.0, hostBpmParam->load(std::memory_order_relaxed))
                             : 0.0;
    const double warpBaseBpm = PercussionSound::warpBaseBpmForHost(currentSound->getOriginalBpm(),
                                                                   hostBpm);
    const double bpmDiff = std::abs(hostBpm - warpBaseBpm);
    const bool nearOriginalBpm = (hostBpmParam != nullptr && bpmDiff < 0.1);

    isWarping = currentSound->isWarpEnabled()
             && hostBpmParam != nullptr
             && warpToggle
             && !nearOriginalBpm;

    if (isWarping && bpmIsMoving)
        noteStartDeclicker.trigger();

    if (isWarping && metadata != nullptr && hostBpmParam != nullptr)
    {
        currentSound->requestWarpedCacheBuild(hostBpm);
        activeWarpCache = currentSound->getWarpedCache(hostBpm);

        if (activeWarpCache)
        {
            playbackState.usingWarpCache = true;
            activeBuffer = &activeWarpCache->buffer;
            playbackState.activeSourceSampleRate = activeWarpCache->sourceSampleRate;
            playbackState.currentTimeRatio = activeWarpCache->timeRatio;
            playbackState.pitchRatio = playbackState.activeSourceSampleRate / playbackSampleRate;
        }
        else
        {
            isRealtimeWarping = true;
        }
    }
    else if (isWarping)
    {
        isRealtimeWarping = true;
    }

    if (isRealtimeWarping && activeBuffer != nullptr)
    {
        const int channels = juce::jlimit(1, 2, activeBuffer->getNumChannels());
        const double hostBpmAtNoteStart = (hostBpmParam != nullptr)
                                            ? juce::jmax(1.0, hostBpmParam->load(std::memory_order_relaxed))
                                            : currentSound->getOriginalBpm();
        playbackState.currentTimeRatio = PercussionSound::warpTimeRatioForHost(currentSound->getOriginalBpm(),
                                                                                hostBpmAtNoteStart);

        if (!realtimeWarpPlayer.start(0,
                                      0.0,
                                      playbackState.currentTimeRatio,
                                      playbackState.activeSourceSampleRate,
                                      playbackSampleRate,
                                      channels))
            isRealtimeWarping = false;
    }
}

void PercussionVoice::clearActivePlayback()
{
    clearCurrentNote();

    currentSound = nullptr;
    activeWarpCache.reset();
    activeBuffer = nullptr;
    metadata = nullptr;

    playbackState = {};
    sustainShaper.setMetadata(nullptr);
    noteStartDeclicker.reset();
    realtimeWarpPlayer.reset();
    resetWarpFlags();
}

void PercussionVoice::resetWarpFlags()
{
    isWarping = false;
    isRealtimeWarping = false;
    playbackState.usingWarpCache = false;
    playbackState.currentTimeRatio = 1.0;
}

void PercussionVoice::maybeSwitchWarpCacheToRealtime()
{
    const bool hostBpmIsMoving = (hostBpmMovingParam != nullptr)
                              && hostBpmMovingParam->load(std::memory_order_relaxed);
    const double hostBpmNow = (hostBpmParam != nullptr)
                                ? juce::jmax(1.0, hostBpmParam->load(std::memory_order_relaxed))
                                : 0.0;
    const double quantizedHostBpm = PercussionSound::quantizeWarpBpm(hostBpmNow);
    const bool cacheBpmMismatch = (activeWarpCache == nullptr)
                               || (std::abs(activeWarpCache->bpm - quantizedHostBpm) > 0.005);

    if (!playbackState.usingWarpCache || isRealtimeWarping || !hostBpmIsMoving || !cacheBpmMismatch)
        return;

    const double cacheRatio = (activeWarpCache != nullptr)
                                ? activeWarpCache->timeRatio
                                : juce::jmax(1e-9, playbackState.currentTimeRatio);
    const double cacheSampleRate = juce::jmax(1e-9, playbackState.activeSourceSampleRate);
    const double warpedTimeSec = playbackState.sourceSamplePosition / cacheSampleRate;
    const double sourceTimeSec = warpedTimeSec / juce::jmax(1e-9, cacheRatio);

    const auto& originalData = currentSound->getAudioData();
    const int originalNumSamples = originalData.getNumSamples();
    if (originalNumSamples <= 0)
        return;

    const double sourceSampleRate = juce::jmax(1.0, currentSound->getSourceSampleRate());
    const int startSourceSample = juce::jlimit(0,
                                               juce::jmax(0, originalNumSamples - 1),
                                               (int) std::floor(sourceTimeSec * sourceSampleRate));
    const double playbackSampleRate = juce::jmax(1.0, getSampleRate());
    const int channels = juce::jlimit(1, 2, originalData.getNumChannels());
    const double timeRatio = PercussionSound::warpTimeRatioForHost(currentSound->getOriginalBpm(), hostBpmNow);

    if (!realtimeWarpPlayer.start(startSourceSample,
                                  sourceTimeSec,
                                  timeRatio,
                                  sourceSampleRate,
                                  playbackSampleRate,
                                  channels))
        return;

    activeWarpCache.reset();
    activeBuffer = &originalData;
    playbackState.activeSourceSampleRate = sourceSampleRate;
    playbackState.sourceSamplePosition = (double) startSourceSample;
    playbackState.pitchRatio = playbackState.activeSourceSampleRate / playbackSampleRate;
    playbackState.currentTimeRatio = timeRatio;
    playbackState.usingWarpCache = false;
    isRealtimeWarping = true;
    noteStartDeclicker.trigger();
}

float PercussionVoice::getSustainAmount() const noexcept
{
    const float amount = (sustainAmountParam != nullptr)
                           ? sustainAmountParam->load(std::memory_order_relaxed)
                           : 0.0f;
    return juce::jlimit(0.0f, 1.0f, amount);
}
