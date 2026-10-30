#include "Presets.h"

namespace {
constexpr uint32_t rotaryStepForHz(float hz) {
  return static_cast<uint32_t>((hz * 4294967296.0) / config::kSampleRate);
}

constexpr OrganTraits kJazzOrgan{rotaryStepForHz(2.4f), 20, 3};
constexpr OrganTraits kFullOrgan{rotaryStepForHz(4.8f), 36, 2};

// Physical buttons start at slot 4 and wrap: Btn N → slot (N+3) % 8
//   Btn 1 → slot 4   Btn 5 → slot 0
//   Btn 2 → slot 5   Btn 6 → slot 1
//   Btn 3 → slot 6   Btn 7 → slot 2
//   Btn 4 → slot 7   Btn 8 → slot 3

// Bank 1: Final 8
//                                                                                                                              Vib    Default
//                                                                                                   ── pair 2 ──  ── pair 3 ── Rt Dp  Effect
// Generated wavetable indices (from WaveTableData.h):
//  0:GrandPno  1:Violin   2:NylonGtr  3:Flute   4:Trumpet  5:Harmonica  6:Cello
//  7:Organ     8:Choir    9:SoftStr  10:Reed   11:AccordionReed  12:PipeOrgan  13:SoftLead

constexpr Preset kPresets[config::kPresetCount] = {

  // ── Bank 1: Acoustic / Traditional ──────────────────────────────────────────────────────────────
  //                                   A    D    S    R  Brt Mod WT FMR FMDp Uni Det Res  FB ModD  F2R F2Dp F2Lv F3R F3Dp F3Lv VbR VbD  DefFx
  {"JazzOrg",    Waveform::Wavetable,  2,   0, 255,  35, 124,  0, 7,  1,   0, false,  0,  28, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Vibrato, &kJazzOrgan},  // B1 Btn 5 — WT7:Organ
  {"FullOrg",    Waveform::Wavetable,  4,   0, 255, 110, 248,  0,12,  1,   0,  true, 24,  36, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Chorus, &kFullOrgan},   // B1 Btn 6 — WT12:PipeOrgan
  {"Accordion",  Waveform::Wavetable, 10, 100, 210, 160, 118, 14,11,  1,   0,  true, 14, 110, 0, 500,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Vibrato, nullptr},  // B1 Btn 7 — WT11:AccordionReed
  {"Clarinet",   Waveform::Wavetable, 22, 280, 184, 260, 102, 14,10,  1,   0, false,  0,  54, 0, 500,  0,  0,  0,  0,  0,  0, 80,18, EffectType::Vibrato, nullptr},  // B1 Btn 8 — WT10:Reed
  {"GrandPno",   Waveform::Wavetable,  2, 900,   0, 280, 158, 68, 0,  1,   0, false,  0,  96, 0, 600,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb, nullptr},   // B1 Btn 1 — WT0:GrandPno
  {"NylonGtr",   Waveform::Wavetable,  1, 480,   0, 260,  80, 70, 2,  1,   0, false,  0, 130, 0, 250,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb, nullptr},   // B1 Btn 2 — WT2:NylonGtr
  {"Violin",     Waveform::Wavetable, 18, 340, 168, 300, 136, 16, 1,  1,   0, false,  0, 104, 0, 600,  0,  0,  0,  0,  0,  0,120,24, EffectType::Reverb, nullptr},   // B1 Btn 3 — WT1:Violin
  {"PanFlute",   Waveform::Wavetable, 45, 260, 190, 320,  82, 18, 3,  1,   0, false,  0,  26, 0, 600,  0,  0,  0,  0,  0,  0, 60,14, EffectType::Reverb, nullptr},   // B1 Btn 4 — WT3:Flute

  // ── Bank 2: Synth / Electronic ──────────────────────────────────────────────────────────────────
  {"BitOrgan",   Waveform::Square,     1,   0, 255,  40,  60,  0, 0,  1,   0, false,  0,  20, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay, nullptr},    // B2 Btn 5 — lo-fi chiptune
  {"WahClav",    Waveform::Saw,        1, 400, 120, 180, 220, 90, 0,  1,   0, false,  0, 220, 0, 150,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay, nullptr},    // B2 Btn 6 — funky auto-wah
  {"Harmonica",  Waveform::Wavetable, 15, 200, 195, 280, 120, 18, 5,  1,   0, false,  0,  90, 0, 400,  0,  0,  0,  0,  0,  0,100,22, EffectType::Reverb, nullptr},   // B2 Btn 7 — blues harp
  {"Cello",      Waveform::Wavetable, 30, 400, 175, 400, 110, 12, 6,  1,   0, false,  0,  86, 0, 700,  0,  0,  0,  0,  0,  0, 90,28, EffectType::Reverb, nullptr},   // B2 Btn 8 — bowed cello
  {"SpacePad",   Waveform::Wavetable,300, 800, 170,2000,  90, 10, 8,  1,   0,  true, 30,  60, 0,1200,  0,  0,  0,  0,  0,  0, 40,10, EffectType::Chorus, nullptr},   // B2 Btn 1 — huge sci-fi pad
  {"FunkBass",   Waveform::Saw,        1, 300,  80, 140, 200, 80, 0,  1,   0, false,  0, 240, 0, 120,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay, nullptr},    // B2 Btn 2 — squelchy bass
  {"SynthLead",  Waveform::Wavetable,  4, 200, 180, 200, 170, 20,13,  1,   0,  true, 16, 110, 0, 300,  0,  0,  0,  0,  0,  0, 80,16, EffectType::Chorus, nullptr},   // B2 Btn 3 — fat analog lead
  {"BellTone",   Waveform::FM,         1,1400,  12, 800, 100, 14, 0,  5,  36, false,  0,  30, 0, 400,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb, nullptr},   // B2 Btn 4 — glass bell

  // ── Bank 3: Experimental / Wild ─────────────────────────────────────────────────────────────────
  {"Pluck",      Waveform::Saw,        1, 100,   0,  80, 220, 80, 0,  1,   0, false,  0, 200, 0,  80,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay, nullptr},    // B3 Btn 5 — sharp synth pluck
  {"Vox",        Waveform::Wavetable, 80, 600, 165,1000, 110, 20, 8,  1,   0,  true, 12, 160, 0, 800,  0,  0,  0,  0,  0,  0, 60,16, EffectType::Reverb, nullptr},   // B3 Btn 6 — haunting vocal
  {"Brass",      Waveform::Wavetable,  6, 200, 160, 180, 200, 60, 4,  1,   0, false,  0, 100, 0, 200,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb, nullptr},   // B3 Btn 7 — punchy brass stab
  {"RingMod",    Waveform::FM,         1, 600,  60, 300, 130, 20, 0,  8, 120, false,  0,  50,20,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay, nullptr},    // B3 Btn 8 — metallic industrial
  {"SubBass",    Waveform::Triangle,   3, 500, 200, 200,  30,  0, 0,  1,   0, false,  0,   0, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Vibrato, nullptr},  // B3 Btn 1 — deep sub
  {"Siren",      Waveform::Sine,       1,   0, 255, 300,  70,  0, 0,  1,   0, false,  0,  20, 0,   0,  0,  0,  0,  0,  0,  0,255,120,EffectType::Delay, nullptr},    // B3 Btn 2 — wobbling siren
  {"Trumpet",    Waveform::Wavetable,  8, 250, 150, 200, 180, 40, 4,  1,   0, false,  0, 110, 0, 300,  0,  0,  0,  0,  0,  0, 70,12, EffectType::Reverb, nullptr},   // B3 Btn 3 — warm trumpet
  {"Choir",      Waveform::Wavetable,200, 800, 165,1400, 106, 16, 8,  1,   0,  true, 14,  60, 0, 600,  0,  0,  0,  0,  0,  0, 50,12, EffectType::Reverb, nullptr}    // B3 Btn 4 — choir pad
};

}  // namespace

namespace presets {
const Preset &at(uint8_t presetIndex) {
  return kPresets[presetIndex % config::kPresetCount];
}
}  // namespace presets
