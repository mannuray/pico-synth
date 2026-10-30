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
  AttackUp,
  DecayUp,
  SustainUp,
  ReleaseUp
};
