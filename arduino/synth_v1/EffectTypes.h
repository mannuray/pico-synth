#pragma once

#include <Arduino.h>

// Per-frame inputs shared by all effects.
struct EffectContext {
  uint32_t lfoPhase;        // shared LFO phase (advances only while the active effect uses it)
  int32_t lfo;              // triangle LFO value at lfoPhase, -32767..32767
  const int16_t *sineTable; // 512-sample sine table
};

// Modulation applied to every voice, computed once per frame.
struct VoiceModulation {
  int32_t pitchFracQ16 = 0;  // relative pitch change: phase step * (1 + pitchFracQ16 / 65536)
  int32_t gainQ8 = 256;      // voice amplitude (256 = unity)
};
