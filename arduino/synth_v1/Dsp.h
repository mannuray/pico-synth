#pragma once

#include <Arduino.h>

// Small fixed-point helpers shared by the synth engine and the effects.
namespace dsp {

// 512-sample table: 9-bit index from top bits, 16-bit fraction for interpolation.
inline int16_t interpolateTable(const int16_t *table, uint32_t phase) {
  const uint16_t index = static_cast<uint16_t>(phase >> 23) & 0x1FFu;
  const uint16_t nextIndex = (index + 1) & 0x1FFu;
  const uint16_t frac = static_cast<uint16_t>((phase >> 7) & 0xFFFFu);
  const int32_t a = table[index];
  const int32_t b = table[nextIndex];
  return static_cast<int16_t>(a + (((b - a) * static_cast<int32_t>(frac)) >> 16));
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
