#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters/PluginParameters.h"
#include "PercussionVoice.h"
#include "SampleLibrary/PercussionSampleLibrary.h"

//==============================================================================
namespace
{
    constexpr int percussionVoiceCount = 8;
    constexpr double percussionOriginalBpm = 153.0;
}

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, PluginParameters::stateType, PluginParameters::createParameterLayout())
{
    DBG("=== AudioPluginAudioProcessor constructor ===");

    rzhavParam = parameters.getRawParameterValue(PluginParameters::rzhavchinaId);
    sustainShortenParam = parameters.getRawParameterValue(PluginParameters::sustainShortenId);
    warpParamRaw = parameters.getRawParameterValue(PluginParameters::warpEnabledId);

    sampler.clearVoices();
    addPercussionVoices();
    DBG("Added " << sampler.getNumVoices() << " sampler voices");

    PercussionSampleLibrary::loadEmbeddedSamples(sampler, percussionOriginalBpm);
    DBG("=== Constructor done ===");
}

void AudioPluginAudioProcessor::addPercussionVoices()
{
    for (int i = 0; i < percussionVoiceCount; ++i)
    {
        auto* v = new PercussionVoice();
        v->setSustainParam(sustainShortenParam);
        v->setWarpEnabledParam(&warpEnabledAtomic);
        v->setHostBpmParam(hostTempo.getBpmAtomic());
        v->setHostBpmMovingParam(hostTempo.getMovingAtomic());
        sampler.addVoice(v);
    }
}

void AudioPluginAudioProcessor::updateVoiceSharedState()
{
    for (int i = 0; i < sampler.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<PercussionVoice*>(sampler.getVoice(i)))
        {
            v->setSustainParam(sustainShortenParam);
            v->setWarpEnabledParam(&warpEnabledAtomic);
            v->setHostBpmParam(hostTempo.getBpmAtomic());
            v->setHostBpmMovingParam(hostTempo.getMovingAtomic());
        }
    }
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    suspendProcessing(true);
    sampler.allNotesOff(1, false);
    sampler.clearLayerMappings();
    sampler.clearVoices();
    sampler.clearSounds();
}

//==============================================================================

void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    sampler.setCurrentPlaybackSampleRate(sampleRate);
    rzhavProcessor.prepare(sampleRate);
    updateVoiceSharedState();
}

void AudioPluginAudioProcessor::releaseResources()
{
    rzhavProcessor.reset();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto tempo = hostTempo.update(getPlayHead(), nowSec);

    buffer.clear();

    if (warpParamRaw != nullptr)
        warpEnabledAtomic.store(warpParamRaw->load(std::memory_order_relaxed) >= 0.5f,
                                std::memory_order_relaxed);

    const bool warpEnabledNow = warpEnabledAtomic.load(std::memory_order_relaxed);
    if (warpCachePrewarmer.update(sampler,
                                  warpEnabledNow,
                                  tempo.transportRunning,
                                  tempo.bpm,
                                  nowSec))
        hostTempo.resetMotion();

    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    float rzhavAmount = 0.0f;
    if (rzhavParam != nullptr)
        rzhavAmount = rzhavParam->load(std::memory_order_relaxed);

    rzhavProcessor.process(buffer, rzhavAmount);
}

//==============================================================================

bool AudioPluginAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
// Required virtuals

const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }
bool AudioPluginAudioProcessor::acceptsMidi() const { return true; }
bool AudioPluginAudioProcessor::producesMidi() const { return false; }
bool AudioPluginAudioProcessor::isMidiEffect() const { return false; }
double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms() { return 1; }
int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }
void AudioPluginAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String AudioPluginAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================

void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(juce::ValueTree::fromXml(*xml));

            if (warpParamRaw != nullptr)
            {
                const bool restoredWarpEnabled = warpParamRaw->load(std::memory_order_relaxed) >= 0.5f;
                warpEnabledAtomic.store(restoredWarpEnabled, std::memory_order_relaxed);
                warpCachePrewarmer.syncEnabledState(restoredWarpEnabled);
            }
        }
    }
}

//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
