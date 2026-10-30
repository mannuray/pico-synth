#pragma once

#include <Arduino.h>

// Small fixed-point helpers shared by the synth engine and the effects.
namespace dsp {

// 512-sample table: 9-bit index from top bits, 16-bit fraction for interpolation.
inline int16_t interpolateTable(const int16_t *table, uint32_t phase) {
  const uint16_t index = static_cast<uint16_t>(phase >> 23) & 0x1FFu;
  const uint16_t nextIndex = (index + 1) & 0x1FFu;
  const int32_t frac = static_cast<int32_t>((phase >> 8) & 0x7FFFu);  // 15 bits so (b - a) * frac fits in 32 bits
  const int32_t a = table[index];
  const int32_t b = table[nextIndex];
  return static_cast<int16_t>(a + (((b - a) * frac) >> 15));
}

inline int16_t triangle(uint32_t phase) {
  const uint16_t top = static_cast<uint16_t>(phase >> 16);
  int32_t value = (top < 32768u) ? top : (65535u - top);
  value = (value * 2) - 32767;
  return static_cast<int16_t>(value);
}

// Linear congruential generator shared by everything that needs randomness
// (start phases, key-click noise, dither).
struct NoiseSource {
  uint32_t seed = 1;

  uint32_t next() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
  }
};

inline int32_t clamp16(int32_t x) {
  return constrain(x, -32767, 32767);
}

// Chamberlin state-variable low-pass. cutoff and damping are Q8 (256 = 1.0);
// lower damping = more resonance. State is clamped to 16 bits so a resonant
// peak can't overflow later multiplies.
struct StateVariableLowPass {
  int32_t low = 0;
  int32_t band = 0;

  int32_t process(int32_t input, int32_t cutoff, int32_t damping) {
    low += (cutoff * band) >> 8;
    const int32_t high = clamp16(input) - low - ((band * damping) >> 8);
    band = clamp16(band + ((cutoff * high) >> 8));
    low = clamp16(low);
    return low;
  }
};

inline int16_t softClip(int32_t sample) {
  constexpr int32_t kThreshold = 24000;
  if (sample > kThreshold) {
    sample = kThreshold + ((sample - kThreshold) / 4);
  } else if (sample < -kThreshold) {
    sample = -kThreshold + ((sample + kThreshold) / 4);
  }

  return static_cast<int16_t>(constrain(sample, -32768, 32767));
}

}  // namespace dsp
