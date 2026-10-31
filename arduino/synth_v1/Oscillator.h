#pragma once

#include "AudioHot.h"
#include "Config.h"
#include "Dsp.h"
#include "SynthTypes.h"

// Waveform generation. sample() is inline because it runs several times per
// voice per audio sample. The sine table is built in RAM at startup, so
// lookups never wait on the flash cache.
namespace osc {

constexpr size_t kTableSize = 512;
extern int16_t kSine[kTableSize];

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

inline int16_t AUDIO_HOT(sine)(uint32_t phase) {
  return dsp::interpolateTable(kSine, phase);
}

// phaseStep > 0 enables anti-aliasing for Saw/Square.
inline int16_t AUDIO_HOT(sample)(Waveform waveform, uint32_t phase, uint32_t phaseStep = 0) {
  switch (waveform) {
    case Waveform::Triangle:
      return dsp::triangle(phase);
    case Waveform::Saw: {
      int32_t raw = static_cast<int32_t>(phase >> 16) - 32768;
      if (phaseStep > 0) {
        raw -= polyBlepQ16(phase, phaseStep) >> 1;  // residual scaled to +-32768
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
    case Waveform::FM:
      return sine(phase);
    case Waveform::Sample:  // played by Voice from SampleData, not here
      break;
  }

  return 0;
}

}  // namespace osc
