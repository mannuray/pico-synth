#pragma once

#include "SynthTypes.h"

// User tweaks applied on top of the preset's envelope (each -2..2;
// positive = longer time / higher sustain level).
struct EnvelopeOffsets {
  int8_t attack = 0;
  int8_t decay = 0;
  int8_t sustain = 0;
  int8_t release = 0;
};

// Per-preset envelope and filter constants shared by all voices.
struct VoiceShape {
  int16_t attackRate = 1;    // exponential approach rate (Q15)
  int16_t decayRate = 1;     // exponential decay rate (Q15)
  int16_t releaseRate = 1;   // exponential release rate (Q15)
  int32_t sustainLevel = 0;
  uint8_t filterBase = 32;
  uint8_t filterEnvDepth = 64;
  int32_t baseCutoff = 32;       // precomputed: filterBase + brightness/7
  int32_t resonanceDamping = 180; // precomputed: 180 - resonance/2
};

VoiceShape computeVoiceShape(const Preset &preset, const EnvelopeOffsets &offsets);

// Convert milliseconds to exponential envelope rate (Q15).
// After ms milliseconds, the envelope will have completed ~99% of its transition.
int16_t msToEnvRate(uint32_t ms);
