# Ocarina of Time Flute — DARPA V2 Package

This package contains the current full prototype bundle for the Ocarina of Time inspired JUCE flute / ocarina plugin.

## Included

### Source
- `Source/OcarinaOfTimeFlute_DARPA_V2.cpp`
  - Main single-file JUCE prototype source
  - Includes:
    - expressive flute / ocarina synthesis
    - built-in song sequencer
    - easter egg unlock sequence
    - hidden Great Fairy Fountain song

### Docs
- `Docs/OcarinaOfTimeFlute_DARPA_V2_README.txt`
  - Notes on current status and next steps

### Tester
- `Tester/OcarinaOfTimeFlute_Repo_SourceOfTruth_Tester.html`
  - Browser-based mock tester
  - Mirrors repo/source-of-truth structure
  - Simulates UI, parameter layout, and unlock flow

## Secret Unlock
Play these built-in songs in succession:
1. Zelda's Lullaby
2. Epona's Song
3. Song of Storms

This unlocks:
4. Great Fairy Fountain

Original fairy clue:
- ↑ ← → ← → ↓

## Current Status
This is a serious prototype package, not yet a compiled VST3/AU plugin build.

## Recommended Next Step
Convert this into a full JUCE repo structure with:
- PluginProcessor.cpp / .h
- PluginEditor.cpp / .h
- dedicated Voice class files
- preset/state system
- production UI skin
- Projucer or CMake project files
