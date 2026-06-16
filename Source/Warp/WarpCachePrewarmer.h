#pragma once

#include "PercussionSynthesiser.h"

class WarpCachePrewarmer
{
public:
    // Returns true when warp was just disabled and caches were cleared.
    bool update(PercussionSynthesiser& sampler,
                bool warpEnabled,
                bool hostTransportRunning,
                double hostBpm,
                double nowSec);

    void clearWarpCaches(PercussionSynthesiser& sampler);
    void syncEnabledState(bool warpEnabled) noexcept { lastWarpEnabled = warpEnabled; }

private:
    bool requestNextWarpCacheForBpm(PercussionSynthesiser& sampler, double hostBpm, int& soundIndex) const;
    bool areWarpCachesReadyForBpm(PercussionSynthesiser& sampler, double hostBpm) const;
    int countWarpCacheBuildsInFlight(PercussionSynthesiser& sampler) const;
    void resetState() noexcept;
    void resetPendingPrewarm() noexcept;

    bool lastWarpEnabled = true;
    double lastObservedWarpBpm = 0.0;
    double pendingWarpPrewarmBpm = 0.0;
    double warpBpmDebounceUntilSec = 0.0;
    double nextWarpPrewarmRetrySec = 0.0;
    int pendingWarpPrewarmSoundIndex = 0;
    bool hasObservedWarpBpm = false;
    bool hasPendingWarpPrewarm = false;

    static constexpr double warpPrewarmDebounceSec = 0.12;
    static constexpr double warpPrewarmRetryIntervalSec = 0.03;
    static constexpr double warpBpmChangeEpsilon = 0.01;
    static constexpr int warpPrewarmMaxInFlightBuilds = 2;
};
