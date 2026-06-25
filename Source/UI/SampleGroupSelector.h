#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "SampleLibrary/PercussionSampleLibrary.h"

class SampleGroupSelector final : public juce::Component
{
public:
    SampleGroupSelector();
    ~SampleGroupSelector() override;

    static int getPreferredHeight() noexcept;

    void setSampleGroups(const std::vector<PercussionSampleLibrary::SampleGroupInfo>& newSampleGroups);
    void setSelectedIndex(int newSelectedIndex);
    void setActivityVelocity(int groupIndex, float velocity);

    std::function<void(int)> onSelectedIndexChanged;

    void resized() override;

private:
    class ButtonStrip;

    juce::Viewport viewport;
    std::unique_ptr<ButtonStrip> buttonStrip;
};
