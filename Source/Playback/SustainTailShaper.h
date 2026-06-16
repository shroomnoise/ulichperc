#pragma once

#include "../SampleMetadata.h"

class SustainTailShaper
{
public:
    void setMetadata(const SampleMetadata* newMetadata) noexcept;
    const SampleMetadata* getMetadata() const noexcept { return metadata; }

    void resetPosition() noexcept;
    bool shouldShape(float amount) const noexcept;
    float getGain(double timeSec, float amount);

    static float getMakeupGain(float amount, bool hasTransientData) noexcept;

private:
    const SampleMetadata* metadata = nullptr;
    int currentTransientIndex = 0;
};
