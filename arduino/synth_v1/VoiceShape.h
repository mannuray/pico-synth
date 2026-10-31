#pragma once

#include "SynthTypes.h"

// Envelope levels are Q30 (kEnvelopeFull = 1.0); rates are Q20 fractions per sample.
constexpr int32_t kEnvelopeFull = 1 << 30;

// Per-preset envelope and filter constants shared by all voices.
struct VoiceShape {
  uint32_t attackRate = 1;
  uint32_t decayRate = 1;
  uint32_t decay2Rate = 1;   // decay rate below the knee (-12 dB); equals decayRate when single-stage
  uint32_t releaseRate = 1;
  int32_t sustainLevel = 0;  // Q30
  uint8_t filterBase = 32;
  uint8_t filterEnvDepth = 64;
  int32_t baseCutoff = 32;       // precomputed: filterBase + brightness/5
  int32_t resonanceDamping = 180; // precomputed: 180 - resonance/2
};

VoiceShape computeVoiceShape(const Preset &preset);

// Converts milliseconds to an exponential envelope rate (Q20 fraction per sample).
// After ms milliseconds, the envelope will have completed ~99% of its transition.
uint32_t msToEnvRate(uint32_t ms);

// level * rateQ20 / 2^20 without 64-bit math (level is Q30, so level >> 15 fits in 16 bits).
inline int32_t scaleByRate(int32_t level, uint32_t rateQ20) {
  const int32_t high = level >> 15;
  return high * static_cast<int32_t>(rateQ20 >> 5) + ((high * static_cast<int32_t>(rateQ20 & 31u)) >> 5);
}
