#pragma once
// Look up presets and rhythm patterns by name, so tests survive reordering.
#include "Presets.h"
#include "RhythmPatterns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline uint8_t findPreset(const char *name) {
  for (uint8_t i = 0; i < config::kPresetCount; ++i) {
    if (strcmp(presets::at(i).name, name) == 0) return i;
  }
  printf("no preset named %s\n", name);
  abort();
}

inline uint8_t findPattern(const char *name) {
  for (uint8_t i = 0; i < kRhythmPatternCount; ++i) {
    if (strcmp(rhythmPatternAt(i).name, name) == 0) return i;
  }
  printf("no pattern named %s\n", name);
  abort();
}
