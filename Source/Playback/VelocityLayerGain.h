#pragma once

#include "../PercussionSound.h"

class VelocityLayerGain
{
public:
    static float calculate(float noteVelocity, const PercussionSound* sound) noexcept;
};
