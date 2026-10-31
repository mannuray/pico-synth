#pragma once

#include <Arduino.h>

// What a front-panel button does, independent of where it is wired.
enum class ControlType : uint8_t {
  None = 0,
  InstrumentSelect,
  OctaveDown,
  OctaveUp,
  SustainToggle,
  VolumeDown,
  VolumeUp,
  TempoDown,
  TempoUp,
  Panic,
  EffectSelect,
  EffectToggle,
  RhythmStartStop,
  RhythmSelect,
  TransposeDown,
  TransposeUp,
  RhythmFill,      // playing: fill on the next bar; stopped: arm/disarm sync start
  IntroEnding      // stopped: start with an intro fill; playing: end with a fill and crash
};
