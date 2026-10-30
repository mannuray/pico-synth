#pragma once

#include <Arduino.h>

enum class Waveform : uint8_t {
  Triangle = 0,
  Saw,
  Square,
  Sine,
  Wavetable,
  FM
};

enum class EffectType : uint8_t {
  Vibrato = 0,
  Tremolo,
  Chorus,
  Reverb,
  Delay,
  Bitcrusher,
  Count
};

// Organ-only voice behaviour: percussion, key click and rotary speaker.
struct OrganTraits {
  uint32_t rotaryStep;      // rotary speaker phase increment per sample (from rotary Hz)
  int32_t rotaryWet;        // rotary signal mix, 0..256
  uint8_t rotaryDepthShift; // rotary phase-offset depth: larger = subtler
};

struct Preset {
  const char *name;
  Waveform waveform;
  uint16_t attackMs;
  uint16_t decayMs;
  uint8_t sustainLevel;
  uint16_t releaseMs;
  uint16_t brightness;
  uint8_t modulationDepth;
  uint8_t wavetableIndex;
  uint8_t fmRatio;
  uint8_t fmDepth;
  bool unison;
  uint8_t detune;
  uint8_t resonance;
  uint8_t fmFeedback;
  uint16_t modDecayMs;
  // FM pairs 2-3 (Algorithm 5 style: parallel carrier-modulator pairs).
  // Set level to 0 to disable a pair (zero CPU cost).
  uint8_t fm2Ratio;       // pair 2 modulator frequency ratio
  uint8_t fm2Depth;       // pair 2 modulation depth
  uint8_t fm2Level;       // pair 2 output mix (0=off, 255=full)
  uint8_t fm3Ratio;       // pair 3 modulator frequency ratio
  uint8_t fm3Depth;       // pair 3 modulation depth
  uint8_t fm3Level;       // pair 3 output mix (0=off, 255=full)
  uint8_t vibratoRate;    // built-in vibrato speed (0=off, 1-255 maps to ~2-8Hz)
  uint8_t vibratoDepth;   // built-in vibrato intensity (0-255)
  EffectType defaultEffect; // auto-enabled effect for this preset
  const OrganTraits *organ; // nullptr for non-organ presets
};

inline const char *effectName(EffectType effect) {
  switch (effect) {
    case EffectType::Vibrato:
      return "Vibrato";
    case EffectType::Tremolo:
      return "Tremolo";
    case EffectType::Chorus:
      return "Chorus";
    case EffectType::Reverb:
      return "Reverb";
    case EffectType::Delay:
      return "Delay";
    case EffectType::Bitcrusher:
      return "Bitcrush";
    case EffectType::Count:
      break;
  }
  return "Unknown";
}
