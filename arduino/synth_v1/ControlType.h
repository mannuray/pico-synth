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
  InstrumentBank,  // toggles the bank the tone buttons choose from
  IntroEnding      // stopped: start with an intro fill; playing: end with a fill and crash
};
