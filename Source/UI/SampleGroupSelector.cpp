#include "SampleGroupSelector.h"

#include <cmath>

namespace
{
constexpr int sampleButtonWidth = 5;
constexpr int sampleButtonHeight = 30;
constexpr int sampleButtonVisibleHeightAtRest = sampleButtonHeight / 2;
constexpr int sampleButtonRestOffsetDown = 6;
constexpr int sampleSelectedDotSize = 7;
constexpr int sampleSelectedDotGap = 2;
constexpr int sampleSelectedDotExtraLift = 10;
constexpr int sampleButtonMaximumLift = 10;
constexpr int minimumSampleButtonGap = 15;
constexpr int sampleButtonSidePadding = 8;
constexpr int sampleButtonStripHeight = sampleSelectedDotSize
                                      + sampleSelectedDotGap
                                      + sampleButtonMaximumLift
                                      + sampleButtonVisibleHeightAtRest;
constexpr int sampleButtonViewportHeight = sampleButtonStripHeight;
}

class SampleGroupSelector::ButtonStrip final : public juce::Component
{
public:
    void setSampleGroups(const std::vector<PercussionSampleLibrary::SampleGroupInfo>& newSampleGroups)
    {
        sampleGroups = newSampleGroups;
        activityVelocities.assign(sampleGroups.size(), 0.0f);
        setSelectedIndex(selectedIndex);
    }

    void setSelectedIndex(int newSelectedIndex)
    {
        if (sampleGroups.empty())
            newSelectedIndex = -1;
        else
            newSelectedIndex = juce::jlimit(0, static_cast<int>(sampleGroups.size()) - 1, newSelectedIndex);

        if (selectedIndex == newSelectedIndex)
            return;

        selectedIndex = newSelectedIndex;
        repaint();
    }

    void setActivityVelocity(int groupIndex, float velocity)
    {
        if (groupIndex < 0 || groupIndex >= static_cast<int>(activityVelocities.size()))
            return;

        const float limitedVelocity = juce::jlimit(0.0f, 1.0f, velocity);
        auto& currentVelocity = activityVelocities[(size_t) groupIndex];
        if (std::abs(currentVelocity - limitedVelocity) < 0.001f)
            return;

        const auto oldBounds = getButtonAndDotRepaintBounds(groupIndex);
        currentVelocity = limitedVelocity;
        const auto newBounds = getButtonAndDotRepaintBounds(groupIndex);
        repaint(oldBounds.getUnion(newBounds).expanded(2, 2));
    }

    int getMinimumRequiredWidth() const noexcept
    {
        const int buttonCount = static_cast<int>(sampleGroups.size());
        if (buttonCount <= 0)
            return 0;

        return sampleButtonSidePadding * 2
             + buttonCount * sampleButtonWidth
             + juce::jmax(0, buttonCount - 1) * minimumSampleButtonGap;
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black);

        for (int i = 0; i < static_cast<int>(sampleGroups.size()); ++i)
            g.fillRect(getButtonBounds(i));

        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(sampleGroups.size()))
        {
            const auto buttonBounds = getButtonBounds(selectedIndex);
            const int dotX = buttonBounds.getCentreX() - sampleSelectedDotSize / 2;
            const int dotY = getSelectedDotY();

            g.fillEllipse(static_cast<float>(dotX),
                          static_cast<float>(dotY),
                          static_cast<float>(sampleSelectedDotSize),
                          static_cast<float>(sampleSelectedDotSize));
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto position = event.getPosition();

        for (int i = 0; i < static_cast<int>(sampleGroups.size()); ++i)
        {
            if (getButtonBounds(i).expanded(3, 3).contains(position))
            {
                setSelectedIndex(i);

                if (onSelectedIndexChanged)
                    onSelectedIndexChanged(i);

                return;
            }
        }
    }

    std::function<void(int)> onSelectedIndexChanged;

private:
    int getActivityLiftPixels(int index) const noexcept
    {
        if (index < 0 || index >= static_cast<int>(activityVelocities.size()))
            return 0;

        return static_cast<int>(std::round(activityVelocities[(size_t) index]
                                           * static_cast<float>(sampleButtonMaximumLift)));
    }

    juce::Rectangle<int> getButtonBounds(int index) const noexcept
    {
        const int buttonCount = static_cast<int>(sampleGroups.size());
        if (index < 0 || index >= buttonCount)
            return {};

        const int availableWidth = juce::jmax(getWidth(), getMinimumRequiredWidth());
        int x = (availableWidth - sampleButtonWidth) / 2;

        if (buttonCount > 1)
        {
            const double availableGapWidth = static_cast<double>(
                availableWidth - sampleButtonSidePadding * 2 - buttonCount * sampleButtonWidth);
            const double gap = juce::jmax(static_cast<double>(minimumSampleButtonGap),
                                          availableGapWidth / static_cast<double>(buttonCount - 1));

            x = sampleButtonSidePadding
              + static_cast<int>(std::round(static_cast<double>(index) * (sampleButtonWidth + gap)));
        }

        const int y = getHeight()
                    - sampleButtonVisibleHeightAtRest
                    + sampleButtonRestOffsetDown
                    - getActivityLiftPixels(index);

        return { x, y, sampleButtonWidth, sampleButtonHeight };
    }

    int getSelectedDotY() const noexcept
    {
        return getHeight()
             - sampleButtonVisibleHeightAtRest
             + sampleButtonRestOffsetDown
             - sampleSelectedDotGap
             - sampleSelectedDotSize
             - sampleSelectedDotExtraLift;
    }

    juce::Rectangle<int> getButtonAndDotRepaintBounds(int index) const noexcept
    {
        auto bounds = getButtonBounds(index);

        if (index == selectedIndex)
        {
            const auto dotBounds = juce::Rectangle<int>(
                bounds.getCentreX() - sampleSelectedDotSize / 2,
                getSelectedDotY(),
                sampleSelectedDotSize,
                sampleSelectedDotSize);
            bounds = bounds.getUnion(dotBounds);
        }

        return bounds;
    }

    std::vector<PercussionSampleLibrary::SampleGroupInfo> sampleGroups;
    std::vector<float> activityVelocities;
    int selectedIndex = -1;
};

SampleGroupSelector::SampleGroupSelector()
    : buttonStrip(std::make_unique<ButtonStrip>())
{
    buttonStrip->onSelectedIndexChanged = [this] (int selectedIndex)
    {
        if (onSelectedIndexChanged)
            onSelectedIndexChanged(selectedIndex);
    };

    viewport.setViewedComponent(buttonStrip.get(), false);
    viewport.setScrollBarsShown(false, false, false, true);
    viewport.setScrollBarThickness(8);
    addAndMakeVisible(viewport);
}

SampleGroupSelector::~SampleGroupSelector()
{
    if (buttonStrip != nullptr)
        buttonStrip->onSelectedIndexChanged = {};

    viewport.setViewedComponent(nullptr, false);
}

int SampleGroupSelector::getPreferredHeight() noexcept
{
    return sampleButtonViewportHeight;
}

void SampleGroupSelector::setSampleGroups(
    const std::vector<PercussionSampleLibrary::SampleGroupInfo>& newSampleGroups)
{
    buttonStrip->setSampleGroups(newSampleGroups);
    resized();
}

void SampleGroupSelector::setSelectedIndex(int newSelectedIndex)
{
    buttonStrip->setSelectedIndex(newSelectedIndex);
}

void SampleGroupSelector::setActivityVelocity(int groupIndex, float velocity)
{
    buttonStrip->setActivityVelocity(groupIndex, velocity);
}

void SampleGroupSelector::resized()
{
    viewport.setBounds(getLocalBounds());

    const int contentWidth = juce::jmax(viewport.getMaximumVisibleWidth(),
                                        buttonStrip->getMinimumRequiredWidth());
    buttonStrip->setBounds(0, 0, contentWidth, sampleButtonStripHeight);
}
