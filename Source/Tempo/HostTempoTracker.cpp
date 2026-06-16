#include "HostTempoTracker.h"

#include <cmath>

HostTempoUpdate HostTempoTracker::update(juce::AudioPlayHead* playHead, double nowSec)
{
    bool hostTransportRunning = false;

    if (playHead != nullptr)
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (playHead->getCurrentPosition(pos))
        {
            hostTransportRunning = (pos.isPlaying || pos.isRecording);
            if (pos.bpm > 0.0)
                hostBpmAtomic.store(pos.bpm, std::memory_order_relaxed);
        }
    }

    const double hostBpmNow = getBpm();
    const bool hostBpmMoved = (!hasHostBpmForMotion)
                           || (std::abs(hostBpmNow - lastHostBpmForMotion) > bpmMotionEpsilon);
    if (hostBpmMoved)
    {
        hasHostBpmForMotion = true;
        lastHostBpmForMotion = hostBpmNow;
        lastHostBpmChangeSec = nowSec;
    }

    const bool isMoving = hasHostBpmForMotion
                       && ((nowSec - lastHostBpmChangeSec) <= bpmMotionHoldSec);
    hostBpmMovingAtomic.store(isMoving, std::memory_order_relaxed);

    return { hostTransportRunning, hostBpmNow, isMoving };
}

void HostTempoTracker::resetMotion() noexcept
{
    hostBpmMovingAtomic.store(false, std::memory_order_relaxed);
    hasHostBpmForMotion = false;
    lastHostBpmForMotion = 0.0;
    lastHostBpmChangeSec = 0.0;
}

double HostTempoTracker::getBpm() const noexcept
{
    return juce::jmax(1.0, hostBpmAtomic.load(std::memory_order_relaxed));
}
