#include "WarpCachePrewarmer.h"

#include "PercussionSound.h"

#include <cmath>

bool WarpCachePrewarmer::update(PercussionSynthesiser& sampler,
                                bool warpEnabled,
                                bool hostTransportRunning,
                                double hostBpm,
                                double nowSec)
{
    const bool warpJustDisabled = (!warpEnabled && lastWarpEnabled);

    if (warpJustDisabled)
    {
        clearWarpCaches(sampler);
        resetState();
    }

    if (warpEnabled && hostTransportRunning)
    {
        const double targetBpm = PercussionSound::quantizeWarpBpm(hostBpm);
        const bool bpmChanged = (!hasObservedWarpBpm)
                             || (std::abs(targetBpm - lastObservedWarpBpm) > warpBpmChangeEpsilon);

        if (bpmChanged)
        {
            clearWarpCaches(sampler);
            hasObservedWarpBpm = true;
            lastObservedWarpBpm = targetBpm;
            pendingWarpPrewarmBpm = targetBpm;
            hasPendingWarpPrewarm = true;
            warpBpmDebounceUntilSec = nowSec + warpPrewarmDebounceSec;
            nextWarpPrewarmRetrySec = warpBpmDebounceUntilSec;
            pendingWarpPrewarmSoundIndex = 0;
        }

        if (hasPendingWarpPrewarm
            && nowSec >= warpBpmDebounceUntilSec
            && nowSec >= nextWarpPrewarmRetrySec)
        {
            const int totalSounds = sampler.getNumSounds();
            if (pendingWarpPrewarmSoundIndex < totalSounds)
            {
                const int inFlight = countWarpCacheBuildsInFlight(sampler);
                if (inFlight < warpPrewarmMaxInFlightBuilds)
                {
                    int idx = pendingWarpPrewarmSoundIndex;
                    if (requestNextWarpCacheForBpm(sampler, pendingWarpPrewarmBpm, idx))
                        pendingWarpPrewarmSoundIndex = idx;
                    else
                        pendingWarpPrewarmSoundIndex = totalSounds;
                }

                nextWarpPrewarmRetrySec = nowSec + warpPrewarmRetryIntervalSec;
            }
            else
            {
                if (areWarpCachesReadyForBpm(sampler, pendingWarpPrewarmBpm))
                    resetPendingPrewarm();
                else
                    nextWarpPrewarmRetrySec = nowSec + warpPrewarmRetryIntervalSec;
            }
        }
    }
    else if (warpEnabled)
    {
        resetPendingPrewarm();
    }

    lastWarpEnabled = warpEnabled;
    return warpJustDisabled;
}

bool WarpCachePrewarmer::requestNextWarpCacheForBpm(PercussionSynthesiser& sampler,
                                                    double hostBpm,
                                                    int& soundIndex) const
{
    const double bpm = juce::jmax(1.0, hostBpm);
    const int totalSounds = sampler.getNumSounds();

    while (soundIndex < totalSounds)
    {
        auto soundPtr = sampler.getSound(soundIndex++);
        auto* sound = dynamic_cast<PercussionSound*>(soundPtr.get());
        if (sound == nullptr || !sound->isWarpEnabled())
            continue;

        sound->requestWarpedCacheBuild(bpm);
        return true;
    }

    return false;
}

bool WarpCachePrewarmer::areWarpCachesReadyForBpm(PercussionSynthesiser& sampler, double hostBpm) const
{
    const double bpm = juce::jmax(1.0, hostBpm);

    for (int i = 0; i < sampler.getNumSounds(); ++i)
    {
        auto soundPtr = sampler.getSound(i);
        auto* sound = dynamic_cast<PercussionSound*>(soundPtr.get());
        if (sound == nullptr || !sound->isWarpEnabled())
            continue;

        if (!sound->getWarpedCache(bpm))
            return false;
    }

    return true;
}

int WarpCachePrewarmer::countWarpCacheBuildsInFlight(PercussionSynthesiser& sampler) const
{
    int count = 0;
    for (int i = 0; i < sampler.getNumSounds(); ++i)
    {
        auto soundPtr = sampler.getSound(i);
        auto* sound = dynamic_cast<PercussionSound*>(soundPtr.get());
        if (sound == nullptr || !sound->isWarpEnabled())
            continue;

        if (sound->isWarpCacheBuildInFlight())
            ++count;
    }

    return count;
}

void WarpCachePrewarmer::clearWarpCaches(PercussionSynthesiser& sampler)
{
    for (int i = 0; i < sampler.getNumSounds(); ++i)
    {
        auto soundPtr = sampler.getSound(i);
        if (auto* sound = dynamic_cast<PercussionSound*>(soundPtr.get()))
            sound->clearWarpedCache();
    }
}

void WarpCachePrewarmer::resetState() noexcept
{
    hasObservedWarpBpm = false;
    lastObservedWarpBpm = 0.0;
    resetPendingPrewarm();
}

void WarpCachePrewarmer::resetPendingPrewarm() noexcept
{
    hasPendingWarpPrewarm = false;
    pendingWarpPrewarmBpm = 0.0;
    warpBpmDebounceUntilSec = 0.0;
    nextWarpPrewarmRetrySec = 0.0;
    pendingWarpPrewarmSoundIndex = 0;
}
