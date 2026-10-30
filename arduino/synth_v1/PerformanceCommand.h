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
  RhythmPlaying,   // 0 = stopped, 1 = playing
  RhythmPattern,   // pattern index
  OctaveShift,     // octaves, config::kMinOctaveShift..kMaxOctaveShift
  Transpose,       // semitones, config::kMinTranspose..kMaxTranspose
  AttackOffset,    // envelope offset, config::kMinEnvelopeOffset..kMaxEnvelopeOffset
  DecayOffset,     // envelope offset, same range
  SustainOffset,   // envelope offset, same range
  ReleaseOffset,   // envelope offset, same range
  AllNotesOff      // value unused
};

struct PerformanceCommand {
  CommandType type = CommandType::AllNotesOff;
  int16_t value = 0;
};
