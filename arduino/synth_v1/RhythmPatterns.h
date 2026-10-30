#pragma once

#include <Arduino.h>

struct RhythmPattern {
  const char *name;
  uint16_t kickMask;
  uint16_t snareMask;
  uint16_t hatMask;
  uint16_t percMask;
  uint16_t accentMask;
  uint8_t percSample;  // which perc sample to use (index into kDrumSamples)
  bool openHat;         // use open hat instead of closed on hat hits
};

constexpr uint8_t kRhythmPatternCount = 8;

// Wraps at kRhythmPatternCount.
const RhythmPattern &rhythmPatternAt(uint8_t patternIndex);
