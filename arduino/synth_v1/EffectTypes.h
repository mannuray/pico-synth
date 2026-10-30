#pragma once

#include <Arduino.h>

// Per-frame inputs shared by all effects.
struct EffectContext {
  uint32_t lfoPhase;        // shared LFO phase (advances only while the active effect uses it)
  int32_t lfo;              // triangle LFO value at lfoPhase
  uint8_t modulationDepth;  // active preset's modulation depth
  const int16_t *sineTable; // 512-sample sine table
};

// Modulation applied to every voice, computed once per frame.
struct VoiceModulation {
  int32_t pitchOffset = 0;  // added to each voice's phase step
  int32_t gainQ8 = 256;     // voice amplitude (256 = unity)
};
