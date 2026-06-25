# ULICHPERC.md

Project-specific reference for the ULICHPERC audio plugin. This supplements
`AGENTS.md`; use it before changing sample loading, velocity layers, playback,
warp, transient handling, parameters, or `processBlock`.

## Main Source Files

- `Source/PluginProcessor.cpp`: plugin lifecycle, sampler ownership,
  parameter/state serialization, and `processBlock` orchestration.
- `Source/Parameters/PluginParameters.cpp`: APVTS parameter IDs and layout.
- `Source/Parameters/SampleSpecificParameterState.cpp`: message-thread
  storage, lookup, and serialization for future sample-specific parameter
  values. This is not realtime-safe audio state.
- `Source/Parameters/SampleSpecificPitchCache.cpp`: fixed-size, lock-free
  realtime cache for per-MIDI-note sample pitch ratios.
- `Source/Midi/MidiNoteActivityState.cpp`: fixed-size, lock-free handoff for
  per-MIDI-note UI activity velocities.
- `Source/UI/SampleGroupSelector.cpp`: bottom UI selector for choosing the
  currently edited sample group.
- `Source/SampleLibrary/PercussionSampleLibrary.cpp`: embedded BinaryData
  sample loading, sample-group inventory, and registration with the percussion
  synthesiser.
- `Source/SampleLibrary/SampleNameParser.cpp`: sample-name parsing.
- `Source/PercussionSynthesiser.cpp`: velocity-group range calculation, MIDI
  velocity to group selection, and variation selection.
- `Source/PercussionSound.cpp`: sample storage, transient metadata ownership,
  velocity-layer metadata, and offline warp-cache rendering.
- `Source/PercussionVoice.cpp`: note-start playback setup, velocity gain,
  warp playback paths, transient/sustain shaping, and per-sample rendering.
- `Source/Tempo/HostTempoTracker.cpp`: host BPM/transport and BPM-motion
  tracking.
- `Source/Warp/WarpCachePrewarmer.cpp`: warp-cache prewarm debounce, retry,
  concurrency, and cache clearing.
- `Source/Effects/RzhavProcessor.cpp`: `Rzhavchina` bit-depth and sample-rate
  reduction effect.
- `Source/SampleMetadata.cpp`: transient JSON lookup and parsing.
- `CMakeLists.txt`: plugin target, formats, BinaryData resources, JUCE, and
  RubberBand integration.

## Sample Loading And Naming

Samples are embedded from `samples/*.wav` through `juce_add_binary_data`.
Files such as `*.wav.asd` are not embedded by the current CMake glob.

The sample parser accepts a numeric note token followed by optional `vX`, `nY`,
and `pZ` tokens. Example: `1_v2_n3_p1.wav`.

- `N` is the note index.
- `vX` is the velocity group index. Missing `vX` defaults to group `1`.
- `nY` is the variation index. Missing `nY` defaults to variation `1`.
- `pZ` is the pitch slot index. Missing `pZ` defaults to pitch slot `1`.
- Duplicate `v`, `n`, or `p` tokens, non-positive values, or unknown tokens
  cause the sample to be skipped.
- `samples_` prefixes from BinaryData resource names are stripped before
  parsing.
- MIDI note mapping starts at `midiNote = 48 + mappedNoteIndex - 1`, where
  pitch slots expand the note map before the final MIDI note is assigned.
- The sample-group selector uses the same mapped-note inventory. A selectable
  group is one `(noteIndex, pitchIndex)` pair; velocity group and variation are
  ignored. For example, `1_v1_n2` and `1_v1_n3` share one selector item, while
  `4_v1_n1_p1` and `4_v1_n1_p2` are separate selector items.

## Current Velocity Layer Inventory

This inventory comes from the current `samples/*.wav` set.

| Note index | MIDI note | Loaded velocity groups and variations |
| --- | ---: | --- |
| 1 | 48 | `v1:n1-n4`, `v2:n1-n4`, `v3:n1-n4`, `v4:n1-n4` |
| 2 | 49 | `v1:n1-n3`, `v2:n1-n2`, `v3:n1-n2` |
| 3 | 50 | `v1:n1-n3`, `v2:n1-n3` |
| 4 | 51 | `v1:n1-n3`, `v2:n1-n3` |
| 5 | 52 | `v1:n1-n4` |
| 6 | 53 | `v1:n1-n3` |
| 7 | 54 | `v1:n1-n3`, `v2:n1-n3`, `v3:n1-n3`, `v4:n1-n3` |
| 8 | 55 | `v1:n1-n3` |
| 9 | 56 | `v1:n1-n4`, `v2:n1-n3`, `v3:n1-n3` |
| 10-25 | 57-72 | one default sample each: `v1:n1` |

For each MIDI note, `groupCount` is the highest velocity group index present,
not the number of non-empty groups. During playback, if the selected group has
no samples, the synthesiser searches nearby groups, checking the next higher
group before the next lower group at each distance.

Variation choice is random within the resolved group. If there is more than one
variation and the random choice repeats the last variation for that note/group,
there is a 65% chance to reroll to a different variation.

## Velocity Group Ranges

Incoming JUCE note velocity is converted once at note-on:

```cpp
midiVelocity = clamp(round(velocity * 127.0f), 1, 127);
```

The current range table is:

| Group count | Group 1 | Group 2 | Group 3 | Group 4 |
| ---: | --- | --- | --- | --- |
| 1 | 1-127 | - | - | - |
| 2 | 1-63 | 64-127 | - | - |
| 3 | 1-42 | 43-85 | 86-127 | - |
| 4 | 1-31 | 32-63 | 64-100 | 101-127 |

`groupCount == 4` uses the explicit special-case ranges above. Other group
counts use an even `ceil(groupIndex * 128 / groupCount)` partition clamped to
MIDI velocity `1..127`.

## Velocity Gain Logic

The code does not crossfade between velocity groups. It selects a single group
from the MIDI velocity, picks one variation from that group, then applies a gain
ramp within the selected group's velocity range.

Current formula from `PercussionVoice::startNote`:

```cpp
t = groupMax > groupMin
      ? clamp((midiVelocity - groupMin) / (groupMax - groupMin), 0.0f, 1.0f)
      : 0.0f;

groupSpanDb = 20.0f / groupCount;
velocityGainDb = (-0.5f * groupSpanDb) + (t * groupSpanDb) + 4.0f;
velocityGain = Decibels::decibelsToGain(velocityGainDb);
```

Important behavior:

- `velocityGroupIndex` is not used directly in the gain formula.
- Every group in a note uses the same dB ramp shape.
- Gain resets at each group boundary because the selected sample group changes
  and `t` starts again from `0`.
- The gain is calculated at note start and then applied in render as part of
  `env * velocityGain`.

| Group count | dB span per selected group | Gain at group min | Gain at group midpoint | Gain at group max |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 20.00 dB | -6.00 dB / 0.50x | +4.00 dB / 1.58x | +14.00 dB / 5.01x |
| 2 | 10.00 dB | -1.00 dB / 0.89x | +4.00 dB / 1.58x | +9.00 dB / 2.82x |
| 3 | 6.67 dB | +0.67 dB / 1.08x | +4.00 dB / 1.58x | +7.33 dB / 2.33x |
| 4 | 5.00 dB | +1.50 dB / 1.19x | +4.00 dB / 1.58x | +6.50 dB / 2.11x |

Boundary examples:

- With 4 groups, velocity `31` in group 1 reaches `+6.50 dB`, while velocity
  `32` starts group 2 at `+1.50 dB`.
- With 3 groups, velocity `42` reaches `+7.33 dB`, while velocity `43` starts
  the next group at `+0.67 dB`.
- With 2 groups, velocity `63` reaches `+9.00 dB`, while velocity `64` starts
  the next group at `-1.00 dB`.

## Parameters And State

Host-visible parameters are APVTS parameters and are serialized through
`parameters.copyState()` / `parameters.replaceState(...)`.

| Parameter ID | Display name | Type/range | Default | Main use |
| --- | --- | --- | ---: | --- |
| `rzhavchina` | `Rzhavchina` | float `0..1` | `0` | Bit depth and sample-rate reduction in `processBlock` |
| `sustainShorten` | `Pomyatost` | float `0..1` | `0` | Sustain/tail shortening in voices |
| `warpEnabled` | `Tempo sync` | bool | `true` | Enables tempo-sync warp behavior |
| `samplePitchSemitones` | `Pitch` | float `-6..6` semitones | `0` | Sample-specific pitch offset for the selected sample group |

## Sample Selection And Sample-Specific State

The editor has a bottom `SampleGroupSelector` strip. It is UI-only and runs on
the message thread. It displays one item per loaded sample group, where a group
is the stable `(noteIndex, pitchIndex)` key produced by the sample loader's
existing keyboard mapping logic. Selector buttons keep their fixed visual size,
while extra strip width is distributed into the gaps between buttons. The minimum
gap is `15 px`; if the loaded groups cannot fit with that gap, the strip scrolls
horizontally. Buttons rest clipped at the bottom edge of the editor, offset
`6 px` lower than the original half-visible position. When a related MIDI note
is held, the corresponding button lifts by up to `10 px`, scaled by note-on
velocity, and returns when note-off is received. The selected dot is anchored to
the resting button position, raised an extra `10 px`, and does not move with
button activity.

Button activity updates use a fixed MIDI-note path for scalability. The audio
thread writes note velocity and generation counters into `MidiNoteActivityState`.
The editor builds a `midiNote -> sampleGroupIndex` map once from the loaded
sample groups, polls the 128 fixed MIDI-note generations, and only updates
selector buttons whose related MIDI note changed.

`AudioPluginAudioProcessor` stores the selected sample-group index and serializes
the selected group both by index and by the stable `(noteIndex, pitchIndex)` key.
Restoring prefers the stable key and falls back to the index for older state.

`SampleSpecificParameterState` stores future per-sample values under a
`sampleSpecific` ValueTree child. It keys values by `(noteIndex, pitchIndex)`
with a legacy index fallback. This object uses `ValueTree`, `CriticalSection`,
string IDs, and linear scans. It is appropriate for UI edits and plugin-state
save/restore only; it must not be read from the audio thread.

`samplePitchSemitones` is the first sample-specific parameter. The editor reads
and writes it through the processor's sample-specific helpers, so changing the
selected sample group updates the knob to that group's stored value. The audio
thread does not read the `ValueTree`; the processor mirrors pitch values into
`SampleSpecificPitchCache`, indexed by the loaded group's MIDI note. The cache
stores precomputed pitch ratios, not raw UI semitone values, so voices avoid
calling `pow()` on the audio thread.

For normal, non-warp-protected playback, pitch is applied by updating
`SamplePlaybackRenderer::State::pitchRatio` from the realtime pitch cache each
render block. This makes pitch changes audible during an already-playing sample
or loop, but it intentionally changes playback speed and therefore duration.

For sounds whose transient metadata has `warp=true` while tempo sync is enabled,
non-neutral pitch must preserve duration. Neutral pitch can still use the
offline warp cache. Non-neutral pitch is routed through `RealtimeWarpPlayer` and
applied through RubberBand pitch scale.

For loop metadata, pitch changes on an already-playing warp-enabled loop are
debounced in `PercussionVoice` for `0.08 s` before they reach
`RealtimeWarpPlayer`. A new note still starts immediately with the current pitch
value. If the loop was using the offline warp cache or normal playback, the
switch to realtime warp also waits for the debounced pitch value, then resumes
from the current source position and triggers the note-start declicker.
`prepareToPlay` pre-prepares realtime warp resources for each voice to reduce
the chance of allocation when this switch is needed.

CPU note: a warped loop with non-neutral pitch is expected to cost more CPU than
cached warped playback. It cannot use the current offline BPM-only warp cache,
so it runs RubberBand in realtime mode with dynamic pitch support
(`OptionPitchHighConsistency`). The debounce reduces repeated `setPitchScale`
updates and avoids switching into realtime warp while the knob is still moving,
but it does not remove the steady CPU cost of playing a pitch-shifted warped
loop. A larger optimization would require pitch-aware offline warp caches, e.g.
cache by `(quantizedBpm, pitchRatio)` or a coarser pitch grid, with memory and
prewarm tradeoffs.

## Adding Sample-Specific Effects

When adding a parameter marked sample-specific:

- Keep the APVTS parameter definition in `PluginParameters.cpp` so hosts can see
  the parameter and normal ranges/defaults stay centralized.
- Add the parameter ID to `PluginParameters::isSampleSpecificParameterId`.
- UI controls may read/write `SampleSpecificParameterState` through the
  processor helpers when the selected sample group changes.
- If audio needs the value, add or extend a dedicated realtime cache for that
  effect. Do not read message-thread state directly from audio just because the
  parameter is marked sample-specific.
- Audio rendering must not read `SampleSpecificParameterState`, `ValueTree`,
  strings, or any locked structure from `processBlock`, voices, or other
  realtime paths.
- Mirror sample-specific values into a realtime-safe cache before audio uses
  them. Prefer preallocated arrays/vectors sized from the loaded sample-group
  inventory, atomics for values read by audio, or another lock-free handoff.
- Do all cache allocation/resizing outside the audio thread, such as during
  construction, sample loading, or `prepareToPlay`.
- If an effect runs per selected sample group, audio lookup should use the
  already-resolved mapped note/sample-group index, not filename parsing or
  message-thread state lookup.
- If a sample-specific value drives expensive DSP state such as RubberBand
  pitch/time settings, do not update that state at raw UI rate. Use an
  audio-thread-safe smoothing, debounce, crossfade, or offline-cache strategy
  chosen for the audible result and CPU cost.

## ProcessBlock Notes

`processBlock` clears the output buffer, updates host BPM/transport state via
`HostTempoTracker`, prewarms warp caches through `WarpCachePrewarmer` when
tempo sync is enabled and transport is running, renders the sampler, then runs
the current effect processors.

`Rzhavchina` is implemented in `RzhavProcessor` and is a true bypass at `0`.
Above `0`, it maps:

- bit depth from `12` bits down to `8` bits;
- target sample rate from `21 kHz` down to `9 kHz`, clamped by host sample rate.

Keep `processBlock` realtime-safe: no file I/O, no logging, no UI calls, no new
allocations, and no avoidable locks. Do not access `SampleGroupSelector` or
`SampleSpecificParameterState` from `processBlock`; use a prepared realtime-safe
cache for any sample-specific effect values. UI activity indicators may be fed
from fixed atomics such as `MidiNoteActivityState`, with the editor polling on
the message thread.

## Warp And Transient Metadata

Transient metadata is loaded from BinaryData JSON resources by replacing the
WAV resource suffix `_wav` with `_transients_json`.

Recognized JSON fields:

- `sampleRate`
- `warp`
- `loop`
- `ignoreTransientShaper`
- `transients`

If metadata is missing or invalid, fallback metadata is created with one
transient at `0.0`, `warp=false`, and `loop=false`.

Warp behavior:

- Original BPM is currently passed as `153.0` when sounds are constructed.
- For host tempos `<= 90 BPM`, the warp base uses half-time
  (`originalBpm * 0.5`).
- Warp BPM is quantized to `0.01 BPM`.
- Offline warp caches are always enabled.
- Cache prewarming is debounced by `0.12 s`, retries every `0.03 s`, and limits
  concurrent builds to `2`.

## Build-Specific Notes

- Project name: `ulichperc`.
- Product name: `ulichpercs`.
- Company: `shroomnoise`.
- Plugin is a synth, accepts MIDI input, and does not produce MIDI output.
- Formats from CMake:
  - Windows: `VST3`
  - macOS: `VST3`, `AU`, `Standalone`
  - other platforms: `VST3`, `Standalone`
- JUCE is fetched with CPM at the pinned tag in `CMakeLists.txt`.
- RubberBand is built from `external/rubberband/single/RubberBandSingle.cpp`.
- macOS defaults to universal `arm64;x86_64` unless overridden.
