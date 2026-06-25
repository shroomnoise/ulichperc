# AGENTS.md

## Project Overview

This project is a sample-based audio plugin built with:

- C++
- JUCE
- CMake

Target platforms:

- macOS (Apple Silicon arm64)
- macOS (Intel x86_64)
- macOS (universal binaries when needed)
- Windows

Primary plugin format:

- VST3
- AU

Project-specific implementation reference:

- Before changing ULICHPERC sample loading, velocity layers, playback, warp,
  transient metadata, parameters, or `processBlock`, read `ULICHPERC.md`.

---

## Core Principles

- Do not guess. If something is unclear, ask or state assumptions explicitly.
- Prefer correctness over completeness.
- Prefer simple, clear solutions over complex abstractions.
- Do not introduce unnecessary architecture changes.
- Follow existing patterns in the codebase.

---

## Workflow Rules

For any non-trivial task:

1. Inspect relevant files first.
2. Restate the goal clearly.
3. Identify missing information or risky assumptions.
4. Provide a short plan before implementation.
5. Implement step by step.
6. Keep changes small and reviewable.
7. After changes:
   - ensure the solution matches the goal
8. At the end, summarize:
   - files changed
   - what changed
   - how to verify
   - remaining risks or TODOs

---

## Planning Rule

Always provide a plan before implementing when the task involves:

- CMake or build system changes
- Cross-platform behavior
- Audio engine / DSP changes
- Sample loading or playback logic
- Threading or synchronization
- Plugin state management
- Performance-sensitive code
- Anything affecting `processBlock`

If information is missing, ask questions before proceeding.

---

## Definition of Done

A task is complete only if:

- Code compiles OR remaining errors are clearly explained
- Platform implications (macOS / Windows) are considered
- Realtime audio constraints are respected
- Changes are summarized
- Risks and follow-ups are listed

---

## Code Quality Rules

- Use modern C++
- Use RAII
- Avoid raw owning pointers
- Prefer:
  - `std::unique_ptr`
  - `std::vector`
  - `std::array`
- Keep functions small and focused
- Use clear and explicit naming
- Avoid "god objects". Move logic in separate classes if it's better for separating responsabilities and code readability
- Do not refactor unrelated code

---

## Realtime Audio Rules (Critical)

Inside audio thread (`processBlock` and related code):

- Do NOT allocate memory
- Do NOT use locks (mutex, etc.) unless unavoidable
- Do NOT perform file I/O
- Do NOT call UI code
- Do NOT log or print
- Keep processing deterministic and fast

Clearly state if any proposed solution is NOT realtime-safe.

---

## JUCE-Specific Rules

- Respect JUCE threading model
- UI runs on message thread only
- Audio runs on audio thread only
- Handle correctly:
  - `prepareToPlay`
  - `processBlock`
  - `releaseResources`
- Handle:
  - sample rate changes
  - block size changes
- Use a clear parameter/state system (e.g. `AudioProcessorValueTreeState`) when appropriate
- Ensure plugin state serialization is reliable
- Every host-visible parameter must be saved and restored by plugin state.
  - For APVTS parameters, `getStateInformation` should serialize `parameters.copyState()`.
  - `setStateInformation` should restore the same tree with `parameters.replaceState(...)`.
- Avoid assumptions about DAW behavior

---

## CMake Rules

- Keep configuration cross-platform
- Avoid hardcoded paths
- Use target-based CMake:
  - `target_sources`
  - `target_link_libraries`
  - `target_compile_definitions`
- Separate platform-specific logic clearly
- Do not assume dependencies support all architectures
- When modifying builds, explain:
  - macOS impact (arm64 / x86_64 / universal)
  - Windows impact

---
