#pragma once

class NoteStartDeclicker
{
public:
    void reset() noexcept;
    void trigger() noexcept;
    float getNextGain() noexcept;

private:
    static constexpr int rampSamples = 16;
    int remainingSamples = 0;
};
