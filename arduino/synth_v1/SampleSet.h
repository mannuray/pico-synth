#pragma once

#include <Arduino.h>

// Recorded instrument samples (from a General MIDI SoundFont, extracted by
// tools/extract_sf2.py into SampleData.h). Each instrument is a set of zones,
// one recording per range of keys.
struct SampleZone {
  const int16_t *data;   // length + 1 samples (one guard sample for interpolation)
  uint32_t length;
  uint32_t loopStart;    // looped zones repeat [loopStart, loopEnd)
  uint32_t loopEnd;
  uint8_t lowKey;
  uint8_t highKey;
  uint8_t rootKey;       // MIDI note the recording plays at its own rate
  int16_t tuneCents;
  bool looped;           // otherwise the sample plays once and the note ends
};

struct SampleSet {
  const SampleZone *zones;
  uint8_t zoneCount;
};

namespace samples {
// Sample rate of the stored samples.
uint32_t rate();
uint8_t setCount();
// The zone of `set` covering `note` (the nearest zone if none does).
const SampleZone &zoneFor(uint8_t set, uint8_t note);
}  // namespace samples
