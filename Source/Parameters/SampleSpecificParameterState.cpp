#include "SampleSpecificParameterState.h"

#include <cmath>

namespace
{
const juce::Identifier sampleSpecificStateType { "sampleSpecific" };
const juce::Identifier sampleSpecificGroupType { "sampleGroup" };
const juce::Identifier sampleSpecificParameterType { "parameter" };
const juce::Identifier sampleGroupIndexProperty { "index" };
const juce::Identifier sampleNoteIndexProperty { "noteIndex" };
const juce::Identifier samplePitchIndexProperty { "pitchIndex" };
const juce::Identifier parameterIdProperty { "id" };
const juce::Identifier parameterValueProperty { "value" };

void removeChildrenWithType(juce::ValueTree& tree, const juce::Identifier& childType)
{
    for (int i = tree.getNumChildren(); --i >= 0;)
        if (tree.getChild(i).hasType(childType))
            tree.removeChild(i, nullptr);
}

juce::ValueTree findGroupState(const juce::ValueTree& state,
                               const PercussionSampleLibrary::SampleGroupInfo& sampleGroup,
                               int fallbackGroupIndex)
{
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (!child.hasType(sampleSpecificGroupType))
            continue;

        const int noteIndex = static_cast<int>(child.getProperty(sampleNoteIndexProperty, -1));
        const int pitchIndex = static_cast<int>(child.getProperty(samplePitchIndexProperty, -1));
        if (noteIndex == sampleGroup.noteIndex && pitchIndex == sampleGroup.pitchIndex)
            return child;

        const bool hasLegacyKey = noteIndex < 0 || pitchIndex < 0;
        if (hasLegacyKey
            && static_cast<int>(child.getProperty(sampleGroupIndexProperty, -1)) == fallbackGroupIndex)
            return child;
    }

    return {};
}

juce::ValueTree findParameterState(const juce::ValueTree& groupState,
                                   const juce::String& parameterId)
{
    for (int i = 0; i < groupState.getNumChildren(); ++i)
    {
        auto child = groupState.getChild(i);
        if (child.hasType(sampleSpecificParameterType)
            && child.getProperty(parameterIdProperty).toString() == parameterId)
        {
            return child;
        }
    }

    return {};
}
}

SampleSpecificParameterState::SampleSpecificParameterState()
    : state(sampleSpecificStateType)
{
}

float SampleSpecificParameterState::getValue(
    const juce::String& parameterId,
    const PercussionSampleLibrary::SampleGroupInfo& sampleGroup,
    int fallbackGroupIndex,
    float fallbackValue) const
{
    const juce::ScopedLock scopedLock(lock);
    const auto groupState = findGroupState(state, sampleGroup, fallbackGroupIndex);
    if (!groupState.isValid())
        return fallbackValue;

    const auto parameterState = findParameterState(groupState, parameterId);
    if (!parameterState.isValid())
        return fallbackValue;

    const float value = static_cast<float>(parameterState.getProperty(parameterValueProperty, fallbackValue));
    return std::isfinite(value) ? value : fallbackValue;
}

void SampleSpecificParameterState::setValue(
    const juce::String& parameterId,
    const PercussionSampleLibrary::SampleGroupInfo& sampleGroup,
    int fallbackGroupIndex,
    float value)
{
    if (!std::isfinite(value))
        return;

    const juce::ScopedLock scopedLock(lock);

    auto groupState = findGroupState(state, sampleGroup, fallbackGroupIndex);
    if (!groupState.isValid())
    {
        groupState = juce::ValueTree(sampleSpecificGroupType);
        state.addChild(groupState, -1, nullptr);
    }

    groupState.setProperty(sampleGroupIndexProperty, fallbackGroupIndex, nullptr);
    groupState.setProperty(sampleNoteIndexProperty, sampleGroup.noteIndex, nullptr);
    groupState.setProperty(samplePitchIndexProperty, sampleGroup.pitchIndex, nullptr);

    auto parameterState = findParameterState(groupState, parameterId);
    if (!parameterState.isValid())
    {
        parameterState = juce::ValueTree(sampleSpecificParameterType);
        parameterState.setProperty(parameterIdProperty, parameterId, nullptr);
        groupState.addChild(parameterState, -1, nullptr);
    }

    parameterState.setProperty(parameterValueProperty, value, nullptr);
}

void SampleSpecificParameterState::writeToPluginState(juce::ValueTree& pluginState) const
{
    removeChildrenWithType(pluginState, sampleSpecificStateType);

    const juce::ScopedLock scopedLock(lock);
    if (state.isValid() && state.getNumChildren() > 0)
        pluginState.addChild(state.createCopy(), -1, nullptr);
}

void SampleSpecificParameterState::restoreFromPluginState(const juce::ValueTree& pluginState)
{
    const juce::ScopedLock scopedLock(lock);
    const auto restoredState = pluginState.getChildWithName(sampleSpecificStateType);
    state = restoredState.isValid() ? restoredState.createCopy()
                                    : juce::ValueTree(sampleSpecificStateType);
}
