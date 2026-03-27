
Ocarina of Time Flute — DARPA V2
================================

What this is
------------
A stronger JUCE single-file source prototype for an Ocarina of Time inspired flute / ocarina plugin.

Included
--------
- 3 built-in songs:
  - Zelda's Lullaby
  - Epona's Song
  - Song of Storms
- Hidden 4th unlock:
  - Great Fairy Fountain
- Easter egg hint in the UI:
  - Fairy pattern clue: ↑ ← → ← → ↓
- More advanced voice than the first prototype:
  - breath articulation
  - velocity brightness
  - mod wheel vibrato scaling
  - aftertouch air boost
  - legato glide
  - stereo air shimmer

What it is not yet
------------------
- Not a compiled VST3/AU
- Not full physical modeling yet
- Not multisampled
- Not finished production UI art pass

Recommended next step
---------------------
Drop this into a JUCE plugin project and compile.
After that, the real world-class pass should be:
1. split into PluginProcessor / PluginEditor / Voice files
2. add proper physical modeling / waveguide bore behavior
3. add polished Zelda-grade visual skin
4. add preset browser + performance macros
