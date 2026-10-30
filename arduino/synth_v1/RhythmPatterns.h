#pragma once

#include <Arduino.h>

// A drum pattern, written like a drum-machine grid: one string per drum, one
// character per 16th-note step.
//   'X' accent   'x' normal   'g' ghost (quiet)   '.' rest
// All lanes of a pattern have the same length, a whole number of bars
// (stepsPerBar steps each); nullptr = the drum doesn't play. The fill lanes
// replace the normal ones on the last bar of every 4-bar phrase (nullptr =
// keep the normal lane), and a crash marks the downbeat after a fill.
struct RhythmPattern {
  const char *name;
  uint8_t defaultBpm;     // tempo recalled when the pattern is selected
  uint8_t stepsPerBar;    // 16 = 4/4, 12 = 3/4
  uint8_t swingPercent;   // 50 = straight, 66 = triplet shuffle (MPC-style swing)
  uint8_t snareSample;    // snare lane sound (snare, rimshot/cross-stick, clap)
  uint8_t percSample;     // perc lane sound (clave, cowbell, ...)

  const char *kick;
  const char *snare;
  const char *hat;        // closed hi-hat
  const char *openHat;    // shares a voice with the closed hat, so each chokes the other
  const char *perc;
  const char *tom;        // 'h'/'m'/'l' = high/mid/low tom; uppercase = accent

  const char *fillKick;
  const char *fillSnare;
  const char *fillHat;
  const char *fillTom;
};

constexpr uint8_t kRhythmPatternCount = 8;

// Wraps at kRhythmPatternCount.
const RhythmPattern &rhythmPatternAt(uint8_t patternIndex);
