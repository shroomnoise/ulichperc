#include "MetaSamplerVoice.h"
#include <cmath>

MetaSamplerVoice::MetaSamplerVoice()
{
    adsr.setParameters({ 0.00f, 0.0f, 1.0f, 0.06f });
}

void MetaSamplerVoice::setSustainParam(std::atomic<float>* p)
{
    sustainAmountParam = p;
}

bool MetaSamplerVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<MetaSamplerSound*>(sound) != nullptr;
}

void MetaSamplerVoice::startNote(int midiNoteNumber, float velocity,
                                 juce::SynthesiserSound* s,
                                 int /*currentPitchWheelPosition*/)
{
    currentSound = dynamic_cast<MetaSamplerSound*>(s);
    activeWarpCache.reset();
    activeBuffer = nullptr;
    activeSourceSampleRate = 44100.0;
    sourceSamplePosition = 0.0;
    currentTransientIndex = 0;
    metadata = nullptr;
    warpedOutputTimeSec = 0.0;

    juce::ignoreUnused(midiNoteNumber);

    {
        const float v = juce::jlimit(0.0f, 1.0f, velocity);
        velocityGain = std::sqrt(v);
    }

    isWarping = false;
    isRealtimeWarping = false;
    usingWarpCache = false;
    rbEnded = false;
    rbSrcPos = 0;
    currentTimeRatio = 1.0;

    if (currentSound != nullptr)
    {
        metadata = currentSound->metadata.get();

        double sourceSR   = currentSound->getSourceSampleRate();
        double playbackSR = getSampleRate();

        if (sourceSR <= 0.0)   sourceSR = 44100.0;
        if (playbackSR <= 0.0) playbackSR = 44100.0;

        activeBuffer = &currentSound->getAudioData();
        activeSourceSampleRate = sourceSR;
        pitchRatio = (activeSourceSampleRate / playbackSR);

        adsr.setSampleRate(playbackSR);
        adsr.noteOn();

        // Enable Complex-style warp only for marked sounds, unless host BPM matches original (≈153)
        const bool warpToggle = (warpEnabledParam != nullptr)
                                && warpEnabledParam->load(std::memory_order_relaxed);
        const double hostBpm = (hostBpmParam != nullptr)
                                 ? juce::jmax(1.0, hostBpmParam->load(std::memory_order_relaxed))
                                 : 0.0;
        const double bpmDiff = std::abs(hostBpm - currentSound->getOriginalBpm());
        const bool nearOriginalBpm = (hostBpmParam != nullptr && bpmDiff < 0.1); // treat ~153 as "no warp"

        isWarping = (currentSound->isWarpEnabled() && hostBpmParam != nullptr && warpToggle && !nearOriginalBpm);
        isRealtimeWarping = false;
        usingWarpCache = false;

        if (isWarping && metadata != nullptr && hostBpmParam != nullptr)
        {
            activeWarpCache = currentSound->getWarpedCache(hostBpm);
            if (activeWarpCache)
            {
                usingWarpCache = true;
                activeBuffer = &activeWarpCache->buffer;
                activeSourceSampleRate = activeWarpCache->sourceSampleRate;
                currentTimeRatio = activeWarpCache->timeRatio;
                pitchRatio = (activeSourceSampleRate / playbackSR);
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

        if (isRealtimeWarping)
        {
            const int chans = juce::jlimit(1, 2, activeBuffer->getNumChannels());

            const bool rebuildRb = (!rb)
                                   || rbSampleRate != (size_t)playbackSR
                                   || rbChannels   != chans;

            if (rebuildRb)
            {
                const auto rbOpts =
                    RubberBand::RubberBandStretcher::OptionProcessRealTime
                  | RubberBand::RubberBandStretcher::OptionThreadingNever
                  | RubberBand::RubberBandStretcher::OptionTransientsCrisp   // sharper percussive onsets
                  | RubberBand::RubberBandStretcher::OptionDetectorPercussive
                  | RubberBand::RubberBandStretcher::OptionWindowShort       // shorter window reduces smearing
                  | RubberBand::RubberBandStretcher::OptionChannelsTogether; // keep stereo transients aligned

                rb = std::make_unique<RubberBand::RubberBandStretcher>(
                    (size_t)playbackSR,
                    (size_t)chans,
                    rbOpts
                );
                rbSampleRate = (size_t)playbackSR;
                rbChannels   = chans;
            }

            rb->reset();

            // Preallocate to avoid allocations in render (best-effort)
            const int initial = 4096;
            rbIn.setSize (chans, initial, false, false, true);
            rbOut.setSize(chans, initial, false, false, true);
        }
    }
    else
    {
        clearCurrentNote();
        activeWarpCache.reset();
        activeBuffer = nullptr;
    }
}


void MetaSamplerVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
        currentSound = nullptr;
        activeWarpCache.reset();
        activeBuffer = nullptr;
        metadata = nullptr;
        currentTransientIndex = 0;

        isWarping = false;
        isRealtimeWarping = false;
        usingWarpCache = false;
        rbEnded = false;
        rbSrcPos = 0;
        currentTimeRatio = 1.0;
        if (rb) rb->reset();
    }
}

void MetaSamplerVoice::pitchWheelMoved(int) {}
void MetaSamplerVoice::controllerMoved(int, int) {}

void MetaSamplerVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                       int startSample,
                                       int numSamples)
{
    if (currentSound == nullptr)
        return;

    if (activeBuffer == nullptr)
        activeBuffer = &currentSound->getAudioData();

    const auto& data = *activeBuffer;
    const int sourceNumSamples = data.getNumSamples();
    const int sourceNumChans   = data.getNumChannels();
    const int outNumChans      = outputBuffer.getNumChannels();

    if (sourceNumSamples <= 0 || sourceNumChans <= 0 || !adsr.isActive())
    {
        clearCurrentNote();
        currentSound = nullptr;
        activeBuffer = nullptr;
        activeWarpCache.reset();
        metadata = nullptr;
        return;
    }

    // -------------------- WARP PATH (Complex-like) --------------------
    if (isRealtimeWarping && rb && hostBpmParam != nullptr)
    {
        const double hostBpm = juce::jmax(1.0, hostBpmParam->load(std::memory_order_relaxed));
        const double ratio   = currentSound->getOriginalBpm() / hostBpm; // 153 / host

        if (std::abs(ratio - currentTimeRatio) > 1e-6)
        {
            currentTimeRatio = ratio;
            rb->setTimeRatio(ratio);
        }

        const int chans = juce::jlimit(1, 2, sourceNumChans);

        if (rbIn.getNumChannels() != chans || rbIn.getNumSamples() < numSamples)
        {
            rbIn.setSize (chans, numSamples, false, false, true);
            rbOut.setSize(chans, numSamples, false, false, true);
        }

        // NOTE: sustain-shortening disabled in warp mode (source time != output time)
        int produced = 0;

        while (produced < numSamples)
        {
            int available = (int) rb->available();

            // If no output is available, feed more input (unless ended)
            if (available <= 0)
            {
                if (!rbEnded)
                {
                    const int remaining    = sourceNumSamples - rbSrcPos;
                    if (remaining <= 0)
                    {
                        rb->process(nullptr, 0, true);
                        rbEnded = true;
                        adsr.noteOff();
                        continue;
                    }

                    const size_t required  = juce::jmax<size_t>(1, rb->getSamplesRequired());
                    const int requiredInt  = (int) juce::jlimit<size_t>(1, (size_t) INT_MAX, required);

                    // Ensure input buffer can hold the required block
                    if (rbIn.getNumSamples() < requiredInt)
                    {
                        rbIn.setSize (chans, requiredInt, false, false, true);
                        rbOut.setSize(chans, juce::jmax(requiredInt, numSamples), false, false, true);
                    }

                    const bool isLastBlock = (remaining <= requiredInt);
                    const int toFeed       = juce::jmin(requiredInt, remaining);

                    if (toFeed > 0)
                    {
                        rbIn.copyFrom(0, 0, data, 0, rbSrcPos, toFeed);
                        if (chans > 1)
                            rbIn.copyFrom(1, 0, data, 1, rbSrcPos, toFeed);

                        const float* in0 = rbIn.getReadPointer(0);
                        const float* in1 = (chans > 1) ? rbIn.getReadPointer(1) : in0;
                        rbInPtrs[0] = in0;
                        rbInPtrs[1] = in1;

                        rb->process(rbInPtrs.data(), (size_t)toFeed, isLastBlock);
                        rbSrcPos = juce::jmin(rbSrcPos + toFeed, sourceNumSamples);
                        if (isLastBlock)
                        {
                            rbEnded = true;
                            adsr.noteOff();
                        }
                        continue;
                    }

                    // End of input: tell Rubber Band, release ADSR (NO LOOPING)
                    rb->process(nullptr, 0, true);
                    rbEnded = true;
                    adsr.noteOff();
                    continue;
                }

                // Ended and no output available: just run ADSR to finish tail (silence)
                for (; produced < numSamples; ++produced)
                {
                    adsr.getNextSample();
                    if (!adsr.isActive())
                    {
                        clearCurrentNote();
                        currentSound = nullptr;
                        activeBuffer = nullptr;
                        activeWarpCache.reset();
                        metadata = nullptr;
                        return;
                    }
                }
                return;
            }

            const int toGet = juce::jmin(available, numSamples - produced);

            float* out0 = rbOut.getWritePointer(0);
            float* out1 = (chans > 1) ? rbOut.getWritePointer(1) : out0;

            rbOutPtrs[0] = out0;
            rbOutPtrs[1] = out1;

            rb->retrieve(rbOutPtrs.data(), (size_t)toGet);

            for (int i = 0; i < toGet; ++i)
            {
                const float env = adsr.getNextSample();
                const float g   = env * velocityGain;

                if (!adsr.isActive())
                {
                    clearCurrentNote();
                    currentSound = nullptr;
                    activeBuffer = nullptr;
                    activeWarpCache.reset();
                    metadata = nullptr;
                    return;
                }

                const float inL = rbOut.getSample(0, i);
                const float inR = (outNumChans > 1) ? rbOut.getSample(juce::jmin(1, chans - 1), i) : inL;

                const float sampleL = inL * g;
                const float sampleR = inR * g;

                outputBuffer.addSample(0, startSample + produced + i, sampleL);
                if (outNumChans > 1)
                    outputBuffer.addSample(1, startSample + produced + i, sampleR);

                for (int ch = 2; ch < outNumChans; ++ch)
                    outputBuffer.addSample(ch, startSample + produced + i, 0.5f * (sampleL + sampleR));
            }

            produced += toGet;
        }

        return;
    }
    // ------------------ END WARP PATH ------------------

    // ------------------ ORIGINAL (NON-WARP) PATH ------------------
    float sustainAmount = sustainAmountParam != nullptr ? sustainAmountParam->load() : 0.0f;
    sustainAmount = juce::jlimit(0.0f, 1.0f, sustainAmount);

    const float* srcL = data.getReadPointer(0);
    const float* srcR = (sourceNumChans > 1) ? data.getReadPointer(1) : nullptr;

    float* out0 = (outNumChans > 0) ? outputBuffer.getWritePointer(0, startSample) : nullptr;
    float* out1 = (outNumChans > 1) ? outputBuffer.getWritePointer(1, startSample) : nullptr;

    const bool doSustainShorten = (metadata != nullptr && sustainAmount > 0.0f);

    if (outNumChans >= 2 && out0 != nullptr && out1 != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (sourceSamplePosition >= (double)sourceNumSamples)
            {
                adsr.noteOff();
                if (!adsr.isActive())
                {
                    clearCurrentNote();
                    currentSound = nullptr;
                    activeBuffer = nullptr;
                    activeWarpCache.reset();
                    metadata = nullptr;
                    break;
                }
            }

            const int pos = (int)sourceSamplePosition;
            if (pos + 1 >= sourceNumSamples)
            {
                sourceSamplePosition += pitchRatio;
                continue;
            }

            const float frac = (float)(sourceSamplePosition - (double)pos);

            const float s1L = srcL[pos];
            const float s2L = srcL[pos + 1];
            float inL = s1L + frac * (s2L - s1L);

            float inR = inL;
            if (srcR != nullptr)
            {
                const float s1R = srcR[pos];
                const float s2R = srcR[pos + 1];
                inR = s1R + frac * (s2R - s1R);
            }

            const float env = adsr.getNextSample();
            const float g = env * velocityGain;

            float sampleL = inL * g;
            float sampleR = inR * g;

            if (doSustainShorten)
            {
                const double playbackTimeSec = sourceSamplePosition / juce::jmax(1e-9, activeSourceSampleRate);
                const double timeSec = playbackTimeSec / juce::jmax(1e-9, (usingWarpCache ? currentTimeRatio : 1.0));
                const float sustainGain = computeSustainGain(timeSec, sustainAmount);
                sampleL *= sustainGain;
                sampleR *= sustainGain;
            }

            out0[i] += sampleL;
            out1[i] += sampleR;

            for (int ch = 2; ch < outNumChans; ++ch)
            {
                float* out = outputBuffer.getWritePointer(ch, startSample);
                out[i] += 0.5f * (sampleL + sampleR);
            }

            sourceSamplePosition += pitchRatio;
        }

        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        if (sourceSamplePosition >= (double)sourceNumSamples)
        {
            adsr.noteOff();
            if (!adsr.isActive())
            {
                clearCurrentNote();
                currentSound = nullptr;
                activeBuffer = nullptr;
                activeWarpCache.reset();
                metadata = nullptr;
                break;
            }
        }

        const int pos = (int)sourceSamplePosition;
        if (pos + 1 >= sourceNumSamples)
        {
            sourceSamplePosition += pitchRatio;
            continue;
        }

        const float frac = (float)(sourceSamplePosition - (double)pos);

        const float s1L = srcL[pos];
        const float s2L = srcL[pos + 1];
        float inL = s1L + frac * (s2L - s1L);

        float inR = inL;
        if (srcR != nullptr)
        {
            const float s1R = srcR[pos];
            const float s2R = srcR[pos + 1];
            inR = s1R + frac * (s2R - s1R);
        }

        const float env = adsr.getNextSample();
        const float g = env * velocityGain;

        float sampleL = inL * g;
        float sampleR = inR * g;

        if (doSustainShorten)
        {
            const double playbackTimeSec = sourceSamplePosition / juce::jmax(1e-9, activeSourceSampleRate);
            const double timeSec = playbackTimeSec / juce::jmax(1e-9, (usingWarpCache ? currentTimeRatio : 1.0));
            const float sustainGain = computeSustainGain(timeSec, sustainAmount);
            sampleL *= sustainGain;
            sampleR *= sustainGain;
        }

        for (int ch = 0; ch < outNumChans; ++ch)
        {
            const float v = (ch == 0 ? sampleL
                                     : (ch == 1 ? sampleR
                                                : 0.5f * (sampleL + sampleR)));
            outputBuffer.addSample(ch, startSample + i, v);
        }

        sourceSamplePosition += pitchRatio;
    }
}

float MetaSamplerVoice::computeSustainGain(double timeSec, float amount)
{
    if (metadata == nullptr || metadata->transients.empty() || amount <= 0.0f)
        return 1.0f;

    const auto& ts = metadata->transients;

    while (currentTransientIndex + 1 < (int)ts.size()
           && timeSec >= ts[(size_t)currentTransientIndex + 1])
        ++currentTransientIndex;

    const double t0 = ts[(size_t)currentTransientIndex];
    const double t1 = (currentTransientIndex + 1 < (int)ts.size())
                        ? ts[(size_t)currentTransientIndex + 1]
                        : metadata->lengthSec;

    if (timeSec < t0) return 1.0f;

    const double segLen = t1 - t0;
    if (segLen <= 0.0) return 1.0f;

    const double a = juce::jlimit(0.0, 1.0, (double)amount);

    constexpr double holdCurve = 15.0;
    const double holdFrac  = std::pow(1.0 - a, holdCurve);
    const double fadeStart = t0 + holdFrac * segLen;

    if (timeSec <= fadeStart) return 1.0f;
    if (timeSec >= t1)        return 0.0f;

    const double x = (timeSec - fadeStart) / juce::jmax(1e-9, (t1 - fadeStart));

    constexpr double kMin = 0.2;
    constexpr double kMax = 9.0;
    const double k = kMin + (kMax - kMin) * a;

    const double e0 = std::exp(-k * 0.0);
    const double e1 = std::exp(-k * 1.0);
    const double ex = std::exp(-k * x);

    const double g = (ex - e1) / (e0 - e1);

    return (float)juce::jlimit(0.0, 1.0, g);
}
