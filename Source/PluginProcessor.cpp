#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "BinaryData.h"
#include "MetaSamplerVoice.h"
#include "MetaSamplerSound.h"

//==============================================================================

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "params", {
          std::make_unique<juce::AudioParameterFloat>("rzhavchina", "Rzhavchina", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
          std::make_unique<juce::AudioParameterFloat>("sustainShorten", "Sustain Shorten", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
          std::make_unique<juce::AudioParameterBool>("warpEnabled", "Warp Enabled", true)
      })
{
    DBG("=== AudioPluginAudioProcessor constructor ===");

    bitDepthParam   = parameters.getRawParameterValue("bitDepth");
    rateDivideParam = parameters.getRawParameterValue("rateDivide");
    rzhavParam = parameters.getRawParameterValue("rzhavchina");
    sustainShortenParam  = parameters.getRawParameterValue("sustainShorten");
    warpParamRaw = parameters.getRawParameterValue("warpEnabled");

    // Voices
    sampler.clearVoices();

    for (int i = 0; i < 8; ++i) {
        auto* v = new MetaSamplerVoice();
        v->setSustainParam(sustainShortenParam);
        v->setWarpEnabledParam(&warpEnabledAtomic);
        v->setHostBpmParam(&hostBpmAtomic);
        sampler.addVoice(v);
    }
    DBG("Added " << sampler.getNumVoices() << " sampler voices");

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Inspect BinaryData
    DBG("BinaryData::namedResourceListSize = " << BinaryData::namedResourceListSize);
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        DBG("  Resource[" << i << "]: " << BinaryData::namedResourceList[i]);
    }

    // Load all embedded *_wav resources as samples
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        juce::String name (BinaryData::namedResourceList[i]);
        DBG("Checking resource: " << name);

        // JUCE's BinaryData names for wavs look like: "samples_1_wav"
        if (! name.endsWithIgnoreCase("_wav"))
        {
            DBG("  Skipped (not a wav resource)");
            continue;
        }

        int dataSize = 0;
        const void* data = BinaryData::getNamedResource(name.toRawUTF8(), dataSize);
        if (data == nullptr || dataSize <= 0)
        {
            DBG("  ERROR: getNamedResource failed for " << name);
            continue;
        }

        std::unique_ptr<juce::MemoryInputStream> stream(
            new juce::MemoryInputStream(data, static_cast<size_t>(dataSize), false));

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(stream)));
        if (! reader)
        {
            DBG("  ERROR: AudioFormatReader creation failed for " << name);
            continue;
        }

        // Strip "_wav" and get trailing int:
        // "samples_11_wav" -> "samples_11" -> 11
        juce::String cleanName = name.upToLastOccurrenceOf("_wav", false, false);
        int fileNumber = cleanName.getTrailingIntValue();

        if (fileNumber <= 0)
        {
            DBG("  Skipped: couldn't extract valid number from '" << cleanName << "'");
            continue;
        }

        int midiNote = 60 + fileNumber - 1; // 1 -> 60 (C4), 2 -> 61, etc.
        DBG("  Parsed fileNumber=" << fileNumber << ", midiNote=" << midiNote);

        const bool shouldWarp = (fileNumber == 6 || fileNumber == 21 || fileNumber == 28 || fileNumber == 30 || fileNumber == 33);

        auto* sound = new MetaSamplerSound(
            cleanName,
            *reader,
            juce::BigInteger().setRange(midiNote, 1, true),
            midiNote,
            0.001,   // attack
            0.05,    // release
            reader->lengthInSamples / reader->sampleRate,
            name,
            shouldWarp,
            150.0
        );

        sampler.addSound(sound);
        DBG("  Added MetaSamplerSound for " << cleanName);
    }

    DBG("Total sampler sounds loaded: " << sampler.getNumSounds());
    DBG("=== Constructor done ===");
}



AudioPluginAudioProcessor::~AudioPluginAudioProcessor() = default;

//==============================================================================

void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    sampler.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < sampler.getNumVoices(); ++i)
    if (auto* v = dynamic_cast<MetaSamplerVoice*>(sampler.getVoice(i)))
    {
        v->setHostBpmParam(&hostBpmAtomic);
        v->setWarpEnabledParam(&warpEnabledAtomic);
    }
}

void AudioPluginAudioProcessor::releaseResources() {}

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

    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (ph->getCurrentPosition(pos) && pos.bpm > 0.0)
            hostBpmAtomic.store(pos.bpm, std::memory_order_relaxed);
    }

    buffer.clear();

    // Update warp flag for all voices (bool, thread-safe)
    if (warpParamRaw != nullptr)
        warpEnabledAtomic.store(warpParamRaw->load(std::memory_order_relaxed) >= 0.5f,
                                std::memory_order_relaxed);
    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Keep state sized correctly (even if bypassed; cheap + safe)
    if ((int)heldPerChannel.size() != numChannels)
    {
        heldPerChannel.assign(numChannels, 0.0f);
        phasePerChannel.assign(numChannels, 0);
    }

    const double hostFs = getSampleRate();
    if (hostFs <= 0.0)
        return;

    // ---- Read knob (expected 0..1) ----
    float k = 0.0f;
    if (rzhavParam != nullptr)
        k = juce::jlimit(0.0f, 1.0f, rzhavParam->load());

    // TRUE BYPASS at zero: leave samples completely untouched
    if (k <= 0.0f)
        return;

    // ---- Map knob -> bit depth: 12 -> 8 ----
    const float bitsF = juce::jmap(k, 12.0f, 8.0f);
    const int bits = juce::jlimit(8, 12, (int) std::round(bitsF));

    // ---- Map knob -> target sample rate: 21k -> 9k ----
    const double srMax = std::min(21000.0, hostFs); // can't exceed hostFs
    const double srMin = 9000.0;
    const double targetFs = juce::jlimit(srMin, srMax, juce::jmap((double)k, srMax, srMin));

    int rateDivide = (int) std::round(hostFs / targetFs);
    rateDivide = juce::jlimit(1, 4096, rateDivide);

    // Quantizer setup (8..12 bits => safe)
    const float levels = (float)((1 << bits) - 1);
    const float invLevels = 1.0f / levels;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);

        float held = heldPerChannel[(size_t)ch];
        int phase  = phasePerChannel[(size_t)ch];

        for (int i = 0; i < numSamples; ++i)
        {
            // --- Sample-rate reduction (counter-based sample & hold) ---
            if (rateDivide == 1)
            {
                held = data[i]; // keep state consistent
            }
            else
            {
                if (phase == 0)
                    held = data[i];
                else
                    data[i] = held;

                ++phase;
                if (phase >= rateDivide)
                    phase = 0;
            }

            // --- Bit-depth reduction ---
            float x = juce::jlimit(-1.0f, 1.0f, data[i]);
            x = std::round(x * levels) * invLevels;
            data[i] = x;
        }

        heldPerChannel[(size_t)ch]  = held;
        phasePerChannel[(size_t)ch] = phase;
    }
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
    juce::ignoreUnused(destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
