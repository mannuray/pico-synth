#pragma once

#include "Config.h"
#include "Dsp.h"
#include "SynthTypes.h"

// Waveform generation. sample() is inline because it runs several times per
// voice per audio sample. The tables live in Oscillator.cpp, the only file that
// includes the generated WaveTableData.h, so flash holds one copy.
namespace osc {

constexpr uint8_t kTableCount = 14;  // checked against WaveTableData.h in Oscillator.cpp
constexpr uint8_t kFrameCount = 4;

extern const int16_t (&kSine)[512];
extern const int16_t *const (&kWaveTables)[kTableCount][kFrameCount];

// MIDI note -> phase increment per sample (32-bit phase).
uint32_t noteToPhaseStep(uint8_t midiNote);

inline uint32_t hzToPhaseStep(float hz) {
  return static_cast<uint32_t>((hz * 4294967296.0f) / config::kSampleRate);
}

// PolyBLEP residual for anti-aliased oscillators.
// Returns correction scaled to [-amplitude, +amplitude].
// Only called when phase is within one phaseStep of a discontinuity (~once per period).
inline int32_t polyBlepQ16(uint32_t phase, uint32_t phaseStep) {
  if (phase < phaseStep) {
    const int32_t t = static_cast<int32_t>(
        (static_cast<uint64_t>(phase) << 16) / phaseStep);
    return 2 * t - (static_cast<int32_t>((static_cast<int64_t>(t) * t) >> 16)) - 65536;
  }
  const uint32_t dist = ~phase;
  if (dist < phaseStep) {
    const int32_t t = static_cast<int32_t>(
        (static_cast<uint64_t>(dist) << 16) / phaseStep);
    return (static_cast<int32_t>((static_cast<int64_t>(t) * t) >> 16)) - 2 * t + 65536;
  }
  return 0;
}

inline int16_t sine(uint32_t phase) {
  return dsp::interpolateTable(kSine, phase);
}

// phaseStep > 0 enables anti-aliasing for Saw/Square. framePos (0..32767)
// crossfades wavetable frames from bright (frame 0) to dark (last frame).
inline int16_t sample(Waveform waveform, uint32_t phase, uint8_t wavetableIndex = 0, uint32_t phaseStep = 0, int32_t framePos = 0) {
  switch (waveform) {
    case Waveform::Triangle:
      return dsp::triangle(phase);
    case Waveform::Saw: {
      int32_t raw = static_cast<int32_t>(phase >> 16) - 32768;
      if (phaseStep > 0) {
        raw -= (polyBlepQ16(phase, phaseStep) * 32768L) >> 16;
      }
      return static_cast<int16_t>(constrain(raw, -32768, 32767));
    }
    case Waveform::Square: {
      int32_t raw = (phase & 0x80000000u) ? 22000 : -22000;
      if (phaseStep > 0) {
        const int32_t blep1 = (polyBlepQ16(phase, phaseStep) * 22000L) >> 16;
        const int32_t blep2 = (polyBlepQ16(phase - 0x80000000u, phaseStep) * 22000L) >> 16;
        raw -= blep1 - blep2;
      }
      return static_cast<int16_t>(constrain(raw, -32768, 32767));
    }
    case Waveform::Sine:
      return sine(phase);
    case Waveform::Wavetable: {
      const uint8_t ti = wavetableIndex % kTableCount;
      if (framePos <= 0) {
        return dsp::interpolateTable(kWaveTables[ti][0], phase);
      }
      // Multi-frame crossfade: framePos 0=bright(frame0), 32767=dark(frame3).
      const int32_t scaled = framePos * (kFrameCount - 1);
      const uint8_t frameA = static_cast<uint8_t>(scaled >> 15);
      const uint8_t frameB = (frameA < kFrameCount - 1) ? frameA + 1 : frameA;
      const int32_t blend = (scaled - (static_cast<int32_t>(frameA) << 15)) >> 7;
      const int16_t a = dsp::interpolateTable(kWaveTables[ti][frameA], phase);
      const int16_t b = dsp::interpolateTable(kWaveTables[ti][frameB], phase);
      return static_cast<int16_t>(a + (((b - a) * blend) >> 8));
    }
    case Waveform::FM:
      return sine(phase);
  }

  return 0;
}

}  // namespace osc
