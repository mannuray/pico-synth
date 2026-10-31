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
// Slot N is tone button T(N+1).
//
// Tuning targets (checked with a host render): similar loudness across presets
// (struck/plucked ones ~2.5 dB lower so their attacks stay below the soft clipper),
// sustained instruments keep their harmonics instead of fading to a sine,
// vibrato +-10-22 cents (20-44 peak-to-peak) at 5-6 Hz fading in after the
// attack, piano and guitar ring for seconds with a fast initial drop.
constexpr Preset kPresets[config::kPresetCount] = {
  // T1-T8, matching the labels printed on the keyboard. All play recorded
  // samples from the FluidR3_GM SoundFont (SampleData.h, same order). The
  // filter is left open so it doesn't colour the recordings. Plucked and
  // struck instruments get a two-stage decay that continues the recording's
  // own decay past the point where the stored sample was cut.
  {.name = "Piano", .gmProgram = 0,  // GM 1 Acoustic Grand Piano
   .waveform = Waveform::Sample, .sampleSet = 0,
   .attackMs = 1, .decayMs = 1500, .decay2Ms = 7000, .sustainLevel = 0, .releaseMs = 350,
   .brightness = 400, .resonance = 0,
   .gain = 128},
  {.name = "Organ", .gmProgram = 16,  // GM 17 Drawbar Organ
   .waveform = Waveform::Sample, .sampleSet = 1,
   .attackMs = 3, .releaseMs = 40,
   .brightness = 400, .resonance = 0,
   .gain = 104, .effectEnabled = false, .organ = &kHammond},
  {.name = "Violin", .gmProgram = 40,  // GM 41 Violin
   .waveform = Waveform::Sample, .sampleSet = 2,
   .attackMs = 40, .releaseMs = 250,
   .brightness = 400, .resonance = 0,
   .gain = 112},
  {.name = "Trumpet", .gmProgram = 56,  // GM 57 Trumpet
   .waveform = Waveform::Sample, .sampleSet = 3,
   .attackMs = 10, .releaseMs = 120,
   .brightness = 400, .resonance = 0,
   .gain = 128},
  {.name = "Banjo", .gmProgram = 105,  // GM 106 Banjo
   .waveform = Waveform::Sample, .sampleSet = 4,
   .attackMs = 1, .decayMs = 600, .decay2Ms = 1800, .sustainLevel = 0, .releaseMs = 150,
   .brightness = 400, .resonance = 0,
   .gain = 128},
  {.name = "Bell", .gmProgram = 14,  // GM 15 Tubular Bells
   .waveform = Waveform::Sample, .sampleSet = 5,
   .attackMs = 1, .decayMs = 2000, .decay2Ms = 8000, .sustainLevel = 0, .releaseMs = 1500,
   .brightness = 400, .resonance = 0,
   .gain = 144},
  {.name = "MusicBox", .gmProgram = 10,  // GM 11 Music Box
   .waveform = Waveform::Sample, .sampleSet = 6,
   .attackMs = 1, .decayMs = 800, .decay2Ms = 2500, .sustainLevel = 0, .releaseMs = 800,
   .brightness = 400, .resonance = 0,
   .gain = 160},
  {.name = "Guitar", .gmProgram = 24,  // GM 25 Acoustic Guitar (nylon)
   .waveform = Waveform::Sample, .sampleSet = 7,
   .attackMs = 1, .decayMs = 1000, .decay2Ms = 3500, .sustainLevel = 0, .releaseMs = 250,
   .brightness = 400, .resonance = 0,
   .gain = 128},
};

}  // namespace

namespace presets {
const Preset &at(uint8_t presetIndex) {
  return kPresets[presetIndex % config::kPresetCount];
}
}  // namespace presets
