#pragma once

#include <juce_core/juce_core.h>
#include "BinaryData.h"

// One continuous region, either attack or sustain
struct ZoneInfo
{
    double startSec = 0.0;
    double endSec   = 0.0;
    bool   isAttack = false;  // true = "attack", false = "sustain"
};

// Metadata for a sample: zones parsed from your JSON
struct SampleMetadata
{
    double sampleRate = 48000.0;
    double lengthSec  = 0.0;
    bool hasTransientJson = false;
    bool warp = false;
    bool loop = false;
    bool ignoreTransientShaper = false;
    std::vector<double> transients;

    bool hasTransients() const noexcept { return !transients.empty(); }
};

// Given a BinaryData WAV resource name (e.g. "samples_sample_1_wav"),
// find and load the corresponding JSON metadata resource
// (e.g. "transients_sample_1_transients_json").
std::unique_ptr<SampleMetadata> loadMetadataForResource(const juce::String& wavResourceName);
