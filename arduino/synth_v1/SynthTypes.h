#pragma once

#include <Arduino.h>

enum class Waveform : uint8_t {
  Triangle = 0,
  Saw,
  Square,
  Sine,
  FM,
  Sample      // recorded instrument samples (Preset::sampleSet)
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

// Hammond-style organ behaviour. Presets without it leave Preset::organ null.
struct OrganTraits {
  uint8_t percussionHarmonic = 0;   // 0 = off, 2 = 4' (2nd harmonic), 3 = 2 2/3' (3rd)
  uint16_t percussionDecayMs = 0;   // Hammond: ~200 ms fast, ~1000 ms slow
  int16_t percussionLevel = 0;      // peak percussion amplitude (0..32767)
  int16_t clickLevel = 0;           // key-click noise burst amplitude (0..32767)
  uint8_t rotarySlowHzX10 = 0;      // Leslie "chorale" horn speed x10 (~8 = 0.8 Hz); 0 = no rotary
  uint8_t rotaryFastHzX10 = 0;      // Leslie "tremolo" horn speed x10 (~67 = 6.7 Hz)
  uint8_t rotaryDepth = 0;          // rotary amplitude/Doppler depth, 0..255
};

// One instrument sound. Fields have defaults, so a preset only names what it
// changes (designated initializers, in declaration order).
struct Preset {
  const char *name = "";
  uint8_t gmProgram = 0;            // General MIDI program sent over USB MIDI (0-127 = GM 1-128)

  // Oscillator
  Waveform waveform = Waveform::Sample;
  uint8_t sampleSet = 0;            // Waveform::Sample: instrument in SampleData.h
  int8_t octave = 0;                // plays this many octaves away from the key's note
  bool unison = false;              // three detuned oscillators
  uint8_t detune = 0;               // unison spread

  // FM (Waveform::FM). Pair 1 always plays; pairs 2-3 add when their level > 0.
  // Ratios are modulator/carrier x100 (140 = 1.4); depth is ~10x the modulation index.
  uint16_t fmRatioX100 = 100;
  uint8_t fmDepth = 0;
  uint8_t fmFeedback = 0;
  uint16_t fm2RatioX100 = 100;      // pair 2: slightly detuned carrier, for richness
  uint8_t fm2Depth = 0;
  uint8_t fm2Level = 0;             // 0 = off, 255 = full
  uint16_t fm3RatioX100 = 100;      // pair 3: carrier at pitch, e.g. a high-ratio "tine"
  uint8_t fm3Depth = 0;
  uint8_t fm3Level = 0;

  // Amplitude envelope (times are to ~99% of the way)
  uint16_t attackMs = 5;
  uint16_t decayMs = 0;
  uint16_t decay2Ms = 0;            // slower decay below -12 dB (piano/guitar tail); 0 = single stage
  uint8_t sustainLevel = 255;
  uint16_t releaseMs = 200;

  // Tone: resonant low-pass filter plus a "spectral envelope" that starts bright
  // and settles to modSustain. It opens the filter (and drives FM depth).
  uint16_t brightness = 128;
  uint8_t resonance = 40;
  uint8_t modulationDepth = 0;      // how far the spectral envelope opens the filter
  uint16_t modDecayMs = 0;          // 0 = stays at full brightness
  uint8_t modSustain = 0;           // level the spectral envelope settles at, 0..255

  // Built-in vibrato
  uint8_t vibratoRateX10 = 55;      // Hz x10
  uint8_t vibratoCents = 0;         // +- deviation (peak-to-peak is twice this); 0 = off
  uint16_t vibratoDelayMs = 0;      // fades in over this time after the note starts

  uint8_t gain = 128;               // output level, 128 = unity
  EffectType defaultEffect = EffectType::Reverb;
  bool effectEnabled = true;        // whether choosing the preset switches its effect on
  const OrganTraits *organ = nullptr;
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
