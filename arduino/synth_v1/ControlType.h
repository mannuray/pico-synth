#pragma once

#include <Arduino.h>

// What a front-panel button does, independent of where it is wired.
enum class ControlType : uint8_t {
  None = 0,
  InstrumentSelect,
  BankDown,
  BankUp,
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
  RhythmFill,      // play a fill on the next bar
  SyncStart,       // arm: the rhythm starts with the first key played
  IntroEnding,     // stopped: start with an intro fill; playing: end with a fill and crash
  LeslieSpeed      // organ: rotary speaker fast/slow
};
