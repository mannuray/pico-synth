#include "Presets.h"

namespace {
// Hammond B-3 style: percussion on the 3rd harmonic with a fast (~0.2 s) decay,
// key click, Leslie horn at ~50 rpm "chorale" or ~400 rpm "tremolo" (0.8 / 6.7 Hz),
// switched with the Leslie button.
constexpr OrganTraits kHammond{
    .percussionHarmonic = 3,
    .percussionDecayMs = 250,
    .percussionLevel = 9000,
    .clickLevel = 5000,
    .rotarySlowHzX10 = 8,
    .rotaryFastHzX10 = 67,
    .rotaryDepth = 150,
};

// Wavetable indices (WaveTableData.h):
//  0 GrandPno   1 Violin   2 NylonGtr   3 Flute   4 Trumpet   5 Harmonica   6 Cello
//  7 Organ      8 Choir    9 SoftStr   10 Reed   11 AccordionReed   12 PipeOrgan   13 SoftLead
//
// gmProgram is the General MIDI Level 1 program sent on preset change, so a
// GM instrument on the computer follows along (wire value = GM number - 1).
//
// Presets are in slot order (bank * 8 + slot). Which physical button selects
// which slot is still to be confirmed against the hardware.
//
// Tuning targets (checked with a host render): similar loudness across presets
// (struck/plucked ones ~2.5 dB lower so their attacks stay below the soft clipper),
// sustained instruments keep their harmonics instead of fading to a sine,
// vibrato +-10-22 cents (20-44 peak-to-peak) at 5-6 Hz fading in after the
// attack, piano and guitar ring for seconds with a fast initial drop.
constexpr Preset kPresets[config::kPresetCount] = {
  // ── Bank 1: Keys & Folk ───────────────────────────────────────────────────
  {.name = "GrandPno", .gmProgram = 0,  // GM 1 Acoustic Grand Piano
   .wavetableIndex = 0,
   .attackMs = 2, .decayMs = 500, .decay2Ms = 7000, .sustainLevel = 0, .releaseMs = 300,
   .brightness = 220, .resonance = 30, .modulationDepth = 60, .modDecayMs = 2500, .modSustain = 130,
   .gain = 157},
  {.name = "EPiano", .gmProgram = 4,  // GM 5 Electric Piano 1
   .waveform = Waveform::FM,
   .fmRatioX100 = 100, .fmDepth = 18, .fm3RatioX100 = 1400, .fm3Depth = 30, .fm3Level = 80,
   .attackMs = 2, .decayMs = 900, .decay2Ms = 5000, .sustainLevel = 0, .releaseMs = 250,
   .brightness = 400, .resonance = 10, .modDecayMs = 1200, .modSustain = 50,
   .gain = 115, .defaultEffect = EffectType::Chorus},
  {.name = "JazzOrg", .gmProgram = 16,  // GM 17 Drawbar Organ
   .wavetableIndex = 7,
   .attackMs = 3, .releaseMs = 40,
   .brightness = 260, .resonance = 20,
   .gain = 104, .effectEnabled = false, .organ = &kHammond},
  {.name = "ChurchOrg", .gmProgram = 19,  // GM 20 Church Organ
   .wavetableIndex = 12, .unison = true, .detune = 3,
   .attackMs = 60, .releaseMs = 450,
   .brightness = 230, .resonance = 20,
   .gain = 143},
  {.name = "Accordion", .gmProgram = 21,  // GM 22 Accordion
   .wavetableIndex = 11, .unison = true, .detune = 9,
   .attackMs = 25, .decayMs = 200, .sustainLevel = 235, .releaseMs = 90,
   .brightness = 200, .resonance = 50,
   .gain = 159},
  {.name = "Harmonica", .gmProgram = 22,  // GM 23 Harmonica
   .wavetableIndex = 5,
   .attackMs = 30, .decayMs = 300, .sustainLevel = 220, .releaseMs = 120,
   .brightness = 200, .resonance = 40, .modulationDepth = 30, .modDecayMs = 400, .modSustain = 190,
   .vibratoRateX10 = 55, .vibratoCents = 20, .vibratoDelayMs = 300,
   .gain = 89},
  {.name = "NylonGtr", .gmProgram = 24,  // GM 25 Acoustic Guitar (nylon)
   .wavetableIndex = 2,
   .attackMs = 1, .decayMs = 250, .decay2Ms = 3500, .sustainLevel = 0, .releaseMs = 250,
   .brightness = 220, .resonance = 50, .modulationDepth = 70, .modDecayMs = 1000, .modSustain = 110,
   .gain = 150},
  {.name = "Bells", .gmProgram = 14,  // GM 15 Tubular Bells
   .waveform = Waveform::FM,
   .fmRatioX100 = 140, .fmDepth = 80,  // Chowning's bell: c:m = 5:7, index ~8
   .attackMs = 1, .decayMs = 1500, .decay2Ms = 8000, .sustainLevel = 0, .releaseMs = 2000,
   .brightness = 400, .resonance = 10, .modDecayMs = 3000, .modSustain = 0,
   .gain = 102},

  // ── Bank 2: Orchestral ────────────────────────────────────────────────────
  {.name = "Violin", .gmProgram = 40,  // GM 41 Violin
   .wavetableIndex = 1,
   .attackMs = 80, .decayMs = 300, .sustainLevel = 220, .releaseMs = 250,
   .brightness = 210, .resonance = 50, .modulationDepth = 40, .modDecayMs = 400, .modSustain = 200,
   .vibratoRateX10 = 60, .vibratoCents = 22, .vibratoDelayMs = 250,
   .gain = 109},
  {.name = "Cello", .gmProgram = 42,  // GM 43 Cello
   .wavetableIndex = 6, .octave = -1,
   .attackMs = 100, .decayMs = 300, .sustainLevel = 220, .releaseMs = 300,
   .brightness = 180, .resonance = 50, .modulationDepth = 30, .modDecayMs = 400, .modSustain = 200,
   .vibratoRateX10 = 55, .vibratoCents = 20, .vibratoDelayMs = 300,
   .gain = 94},
  {.name = "Strings", .gmProgram = 48,  // GM 49 String Ensemble 1
   .wavetableIndex = 9, .unison = true, .detune = 6,
   .attackMs = 250, .decayMs = 400, .sustainLevel = 230, .releaseMs = 600,
   .brightness = 190, .resonance = 30,
   .vibratoRateX10 = 55, .vibratoCents = 12, .vibratoDelayMs = 400,
   .gain = 151},
  {.name = "Flute", .gmProgram = 73,  // GM 74 Flute
   .wavetableIndex = 3,
   .attackMs = 60, .decayMs = 200, .sustainLevel = 220, .releaseMs = 150,
   .brightness = 210, .resonance = 20, .modulationDepth = 20, .modDecayMs = 300, .modSustain = 200,
   .vibratoRateX10 = 50, .vibratoCents = 15, .vibratoDelayMs = 300,
   .gain = 79},
  {.name = "Clarinet", .gmProgram = 71,  // GM 72 Clarinet
   .wavetableIndex = 10,
   .attackMs = 40, .decayMs = 200, .sustainLevel = 230, .releaseMs = 120,
   .brightness = 250, .resonance = 40, .modulationDepth = 30, .modDecayMs = 300, .modSustain = 240,
   .gain = 72},
  {.name = "Trumpet", .gmProgram = 56,  // GM 57 Trumpet
   .wavetableIndex = 4,
   .attackMs = 30, .decayMs = 200, .sustainLevel = 210, .releaseMs = 120,
   .brightness = 230, .resonance = 40, .modulationDepth = 60, .modDecayMs = 250, .modSustain = 210,
   .vibratoRateX10 = 55, .vibratoCents = 15, .vibratoDelayMs = 350,
   .gain = 126},
  {.name = "Brass", .gmProgram = 61,  // GM 62 Brass Section
   .wavetableIndex = 4, .unison = true, .detune = 8,
   .attackMs = 70, .decayMs = 400, .sustainLevel = 220, .releaseMs = 200,
   .brightness = 190, .resonance = 40, .modulationDepth = 70, .modDecayMs = 450, .modSustain = 170,
   .gain = 183},
  {.name = "Choir", .gmProgram = 52,  // GM 53 Choir Aahs
   .wavetableIndex = 8, .unison = true, .detune = 7,
   .attackMs = 250, .decayMs = 400, .sustainLevel = 230, .releaseMs = 600,
   .brightness = 180, .resonance = 30,
   .vibratoRateX10 = 50, .vibratoCents = 15, .vibratoDelayMs = 300,
   .gain = 144},

  // ── Bank 3: Synth ─────────────────────────────────────────────────────────
  {.name = "SynthLead", .gmProgram = 81,  // GM 82 Lead 2 (sawtooth)
   .waveform = Waveform::Saw, .unison = true, .detune = 6,
   .attackMs = 5, .decayMs = 300, .sustainLevel = 230, .releaseMs = 150,
   .brightness = 210, .resonance = 80, .modulationDepth = 50, .modDecayMs = 300, .modSustain = 120,
   .vibratoRateX10 = 55, .vibratoCents = 20, .vibratoDelayMs = 400,
   .gain = 141, .defaultEffect = EffectType::Delay},
  {.name = "SpacePad", .gmProgram = 95,  // GM 96 Pad 8 (sweep)
   .wavetableIndex = 13, .unison = true, .detune = 14,
   .attackMs = 900, .decayMs = 2000, .sustainLevel = 200, .releaseMs = 2500,
   .brightness = 150, .resonance = 60, .modulationDepth = 80, .modDecayMs = 3000, .modSustain = 80,
   .gain = 91, .defaultEffect = EffectType::Chorus},
  {.name = "FunkBass", .gmProgram = 38,  // GM 39 Synth Bass 1
   .waveform = Waveform::Saw, .octave = -1,
   .attackMs = 2, .decayMs = 300, .sustainLevel = 150, .releaseMs = 80,
   .brightness = 90, .resonance = 200, .modulationDepth = 120, .modDecayMs = 150, .modSustain = 0,
   .gain = 121, .effectEnabled = false},
  {.name = "SubBass", .gmProgram = 39,  // GM 40 Synth Bass 2
   .waveform = Waveform::Sine, .octave = -1,
   .attackMs = 5, .releaseMs = 100,
   .brightness = 400, .resonance = 0,
   .gain = 64, .effectEnabled = false},
  {.name = "Pluck", .gmProgram = 90,  // GM 91 Pad 3 (polysynth), nearest GM sound
   .waveform = Waveform::Saw,
   .attackMs = 1, .decayMs = 180, .decay2Ms = 700, .sustainLevel = 0, .releaseMs = 150,
   .brightness = 120, .resonance = 140, .modulationDepth = 120, .modDecayMs = 120, .modSustain = 0,
   .gain = 160, .defaultEffect = EffectType::Delay},
  {.name = "WahClav", .gmProgram = 7,  // GM 8 Clavinet
   .waveform = Waveform::Saw,
   .attackMs = 1, .decayMs = 250, .sustainLevel = 120, .releaseMs = 60,
   .brightness = 110, .resonance = 210, .modulationDepth = 130, .modDecayMs = 180, .modSustain = 0,
   .gain = 144, .effectEnabled = false},
  {.name = "Chiptune", .gmProgram = 80,  // GM 81 Lead 1 (square)
   .waveform = Waveform::Square,
   .attackMs = 1, .releaseMs = 30,
   .brightness = 400, .resonance = 0,
   .vibratoRateX10 = 60, .vibratoCents = 20, .vibratoDelayMs = 200,
   .gain = 67, .defaultEffect = EffectType::Bitcrusher},
  {.name = "RingMod", .gmProgram = 93,  // GM 94 Pad 6 (metallic), nearest GM sound
   .waveform = Waveform::FM,
   .fmRatioX100 = 353, .fmDepth = 30,  // non-integer ratio = inharmonic, metallic
   .attackMs = 2, .decayMs = 800, .sustainLevel = 120, .releaseMs = 400,
   .brightness = 400, .resonance = 10,
   .gain = 90, .defaultEffect = EffectType::Delay},
};

}  // namespace

namespace presets {
const Preset &at(uint8_t presetIndex) {
  return kPresets[presetIndex % config::kPresetCount];
}
}  // namespace presets
