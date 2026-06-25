#include "PercussionVoice.h"

#include "Playback/VelocityLayerGain.h"

#include <cmath>

namespace
{
    constexpr double warpLoopPitchDebounceSeconds = 0.08;
    constexpr double pitchRatioChangeEpsilon = 1e-6;

    bool isNeutralPitchRatio(double ratio) noexcept
    {
        return std::abs(ratio - 1.0) < 1e-5;
    }

    bool pitchRatiosDiffer(double a, double b) noexcept
    {
        return std::abs(a - b) > pitchRatioChangeEpsilon;
    }
}

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

void PercussionVoice::prepareRealtimeWarpResources(double playbackSampleRate, int samplesPerBlock)
{
    realtimeWarpPlayer.prepare(playbackSampleRate, 2, samplesPerBlock);
}

void PercussionVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                      int startSample,
                                      int numSamples)
{
    if (currentSound == nullptr)
        return;

    if (activeBuffer == nullptr)
        activeBuffer = &currentSound->getAudioData();

    const double effectiveSamplePitchRatio = updateWarpLoopPitchDebounce(numSamples);

    maybeSwitchWarpCacheToRealtime(effectiveSamplePitchRatio);
    maybeSwitchLengthPreservedPitchToRealtime(effectiveSamplePitchRatio);

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
                                                      effectiveSamplePitchRatio,
                                                      sustainShaper,
                                                      noteStartDeclicker);

        if (result.finished)
            clearActivePlayback();

        return;
    }

    updateSampleRendererPitchRatio();

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

    const double samplePitchRatio = getCurrentSamplePitchRatio();
    resetWarpLoopPitchDebounce(samplePitchRatio);

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
    const bool shouldTimeWarp = currentSound->isWarpEnabled()
                             && hostBpmParam != nullptr
                             && warpToggle
                             && !nearOriginalBpm;
    const bool shouldPreservePitchLength = currentSound->isWarpEnabled()
                                        && hostBpmParam != nullptr
                                        && warpToggle;
    const bool needsLengthPreservingPitch = shouldPreservePitchLength
                                         && !isNeutralPitchRatio(samplePitchRatio);

    isWarping = shouldTimeWarp || needsLengthPreservingPitch;

    if (shouldTimeWarp && bpmIsMoving)
        noteStartDeclicker.trigger();

    if (shouldTimeWarp && metadata != nullptr && hostBpmParam != nullptr)
    {
        currentSound->requestWarpedCacheBuild(hostBpm);
        activeWarpCache = currentSound->getWarpedCache(hostBpm);

        if (activeWarpCache && !needsLengthPreservingPitch)
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
    else if (shouldTimeWarp || needsLengthPreservingPitch)
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
                                      channels,
                                      samplePitchRatio))
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
    resetWarpLoopPitchDebounce(1.0);
    resetWarpFlags();
}

void PercussionVoice::resetWarpFlags()
{
    isWarping = false;
    isRealtimeWarping = false;
    playbackState.usingWarpCache = false;
    playbackState.currentTimeRatio = 1.0;
}

void PercussionVoice::maybeSwitchWarpCacheToRealtime(double effectiveSamplePitchRatio)
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

    switchToRealtimeWarpFromOriginal(startSourceSample,
                                     sourceTimeSec,
                                     timeRatio,
                                     sourceSampleRate,
                                     playbackSampleRate,
                                     channels,
                                     effectiveSamplePitchRatio,
                                     true);
}

void PercussionVoice::maybeSwitchLengthPreservedPitchToRealtime(double effectiveSamplePitchRatio)
{
    if (currentSound == nullptr
        || isRealtimeWarping
        || !shouldPreserveLengthForPitch()
        || isNeutralPitchRatio(effectiveSamplePitchRatio))
    {
        return;
    }

    const double hostBpmNow = getCurrentHostBpm();
    const double playbackSampleRate = juce::jmax(1.0, getSampleRate());
    const double timeRatio = PercussionSound::warpTimeRatioForHost(currentSound->getOriginalBpm(), hostBpmNow);

    if (playbackState.usingWarpCache)
    {
        const double cacheRatio = (activeWarpCache != nullptr)
                                    ? activeWarpCache->timeRatio
                                    : juce::jmax(1e-9, playbackState.currentTimeRatio);
        const double cacheSampleRate = juce::jmax(1e-9, playbackState.activeSourceSampleRate);
        const double warpedTimeSec = playbackState.sourceSamplePosition / cacheSampleRate;
        const double sourceTimeSec = warpedTimeSec / juce::jmax(1e-9, cacheRatio);
        const auto& originalData = currentSound->getAudioData();
        const double sourceSampleRate = juce::jmax(1.0, currentSound->getSourceSampleRate());
        const int startSourceSample = juce::jlimit(0,
                                                   juce::jmax(0, originalData.getNumSamples() - 1),
                                                   (int) std::floor(sourceTimeSec * sourceSampleRate));

        switchToRealtimeWarpFromOriginal(startSourceSample,
                                         sourceTimeSec,
                                         timeRatio,
                                         sourceSampleRate,
                                         playbackSampleRate,
                                         juce::jlimit(1, 2, originalData.getNumChannels()),
                                         effectiveSamplePitchRatio,
                                         true);
        return;
    }

    const auto& originalData = currentSound->getAudioData();
    const double sourceSampleRate = juce::jmax(1.0, currentSound->getSourceSampleRate());
    const int startSourceSample = juce::jlimit(0,
                                               juce::jmax(0, originalData.getNumSamples() - 1),
                                               (int) std::floor(playbackState.sourceSamplePosition));
    const double sourceTimeSec = static_cast<double>(startSourceSample) / sourceSampleRate;

    switchToRealtimeWarpFromOriginal(startSourceSample,
                                     sourceTimeSec,
                                     timeRatio,
                                     sourceSampleRate,
                                     playbackSampleRate,
                                     juce::jlimit(1, 2, originalData.getNumChannels()),
                                     effectiveSamplePitchRatio,
                                     true);
}

bool PercussionVoice::switchToRealtimeWarpFromOriginal(int startSourceSample,
                                                       double sourceTimeSec,
                                                       double timeRatio,
                                                       double sourceSampleRate,
                                                       double playbackSampleRate,
                                                       int channels,
                                                       double samplePitchRatio,
                                                       bool triggerDeclick)
{
    if (currentSound == nullptr)
        return false;

    const auto& originalData = currentSound->getAudioData();
    if (!realtimeWarpPlayer.start(startSourceSample,
                                  sourceTimeSec,
                                  timeRatio,
                                  sourceSampleRate,
                                  playbackSampleRate,
                                  channels,
                                  samplePitchRatio))
    {
        return false;
    }

    activeWarpCache.reset();
    activeBuffer = &originalData;
    playbackState.activeSourceSampleRate = sourceSampleRate;
    playbackState.sourceSamplePosition = static_cast<double>(startSourceSample);
    playbackState.pitchRatio = playbackState.activeSourceSampleRate / playbackSampleRate;
    playbackState.currentTimeRatio = timeRatio;
    playbackState.usingWarpCache = false;
    isWarping = true;
    isRealtimeWarping = true;

    if (triggerDeclick)
        noteStartDeclicker.trigger();

    return true;
}

double PercussionVoice::updateWarpLoopPitchDebounce(int numSamples)
{
    const double currentPitchRatio = getCurrentSamplePitchRatio();

    if (!shouldDebounceWarpLoopPitch())
    {
        resetWarpLoopPitchDebounce(currentPitchRatio);
        return currentPitchRatio;
    }

    if (pitchRatiosDiffer(currentPitchRatio, pendingWarpLoopPitchRatio))
    {
        pendingWarpLoopPitchRatio = currentPitchRatio;
        warpLoopPitchDebounceSamplesRemaining = getWarpLoopPitchDebounceSampleCount();
    }
    else if (warpLoopPitchDebounceSamplesRemaining > 0)
    {
        warpLoopPitchDebounceSamplesRemaining = juce::jmax(0,
                                                           warpLoopPitchDebounceSamplesRemaining
                                                            - juce::jmax(0, numSamples));

        if (warpLoopPitchDebounceSamplesRemaining == 0
            && pitchRatiosDiffer(appliedWarpLoopPitchRatio, pendingWarpLoopPitchRatio))
        {
            appliedWarpLoopPitchRatio = pendingWarpLoopPitchRatio;
            noteStartDeclicker.trigger();
        }
    }

    return appliedWarpLoopPitchRatio;
}

void PercussionVoice::resetWarpLoopPitchDebounce(double pitchRatio) noexcept
{
    appliedWarpLoopPitchRatio = pitchRatio;
    pendingWarpLoopPitchRatio = pitchRatio;
    warpLoopPitchDebounceSamplesRemaining = 0;
}

float PercussionVoice::getSustainAmount() const noexcept
{
    const float amount = (sustainAmountParam != nullptr)
                           ? sustainAmountParam->load(std::memory_order_relaxed)
                           : 0.0f;
    return juce::jlimit(0.0f, 1.0f, amount);
}

void PercussionVoice::updateSampleRendererPitchRatio() noexcept
{
    const double playbackSampleRate = juce::jmax(1.0, getSampleRate());
    double pitchRatio = 1.0;

    if (!shouldPreserveLengthForPitch())
        pitchRatio = getCurrentSamplePitchRatio();

    playbackState.pitchRatio = (playbackState.activeSourceSampleRate / playbackSampleRate) * pitchRatio;
}

double PercussionVoice::getCurrentSamplePitchRatio() const noexcept
{
    if (currentSound == nullptr || samplePitchCache == nullptr)
        return 1.0;

    return samplePitchCache->getPitchRatioForMidiNote(currentSound->getMidiRootNote());
}

double PercussionVoice::getCurrentHostBpm() const noexcept
{
    if (hostBpmParam != nullptr)
        return juce::jmax(1.0, hostBpmParam->load(std::memory_order_relaxed));

    return (currentSound != nullptr) ? currentSound->getOriginalBpm() : 1.0;
}

bool PercussionVoice::shouldPreserveLengthForPitch() const noexcept
{
    return currentSound != nullptr
        && currentSound->isWarpEnabled()
        && hostBpmParam != nullptr
        && warpEnabledParam != nullptr
        && warpEnabledParam->load(std::memory_order_relaxed);
}

bool PercussionVoice::shouldDebounceWarpLoopPitch() const noexcept
{
    return shouldPreserveLengthForPitch()
        && metadata != nullptr
        && metadata->loop;
}

int PercussionVoice::getWarpLoopPitchDebounceSampleCount() const noexcept
{
    const double sampleRate = juce::jmax(1.0, getSampleRate());
    return juce::jmax(1, (int) std::ceil(warpLoopPitchDebounceSeconds * sampleRate));
}
