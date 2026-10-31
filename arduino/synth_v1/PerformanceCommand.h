#pragma once

#include <Arduino.h>

// A change to performance state. Values are in musical units; each sink
// converts them to its own representation (synth parameter, MIDI CC, ...).
enum class CommandType : uint8_t {
  SelectBank = 0,  // bank index
  SelectPreset,    // preset index (bank * instruments per bank + slot)
  Sustain,         // 0 = off, 1 = on
  Volume,          // master volume, config::kMinMasterVolume..kMaxMasterVolume
  Tempo,           // BPM, config::kTempoMinBpm..kTempoMaxBpm
  Effect,          // EffectType
  EffectEnabled,   // 0 = off, 1 = on
  RhythmPlaying,   // a RhythmTransport value
  RhythmPattern,   // pattern index
  OctaveShift,     // octaves, config::kMinOctaveShift..kMaxOctaveShift
  Transpose,       // semitones, config::kMinTranspose..kMaxTranspose
  RhythmFill,      // value unused: play a fill on the next bar
  RotaryFast,      // 0 = Leslie slow (chorale), 1 = fast (tremolo)
  AllNotesOff      // value unused
};

// Values of CommandType::RhythmPlaying.
enum RhythmTransport : int16_t {
  kRhythmStop = 0,
  kRhythmStart = 1,
  kRhythmStartWithIntro = 2,   // a fill bar first, then the groove
  kRhythmStopWithEnding = 3,   // finish with a fill bar and a crash
};

struct PerformanceCommand {
  CommandType type = CommandType::AllNotesOff;
  int16_t value = 0;
};
