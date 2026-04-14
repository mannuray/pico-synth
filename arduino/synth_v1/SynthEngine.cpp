#include "SynthEngine.h"
#include "WaveTableData.h"

#include <math.h>

namespace {
constexpr uint8_t kJazzOrganPresetIndex = 0;
constexpr uint8_t kFullOrganPresetIndex = 1;
constexpr int32_t kOrganPercussionStartLevel = 22000;
constexpr uint32_t kOrganPercussionDecayDivisor = 35u;
constexpr float kJazzOrganRotaryHz = 2.4f;
constexpr float kFullOrganRotaryHz = 4.8f;
constexpr int32_t kJazzOrganRotaryWet = 20;
constexpr int32_t kFullOrganRotaryWet = 36;
constexpr int32_t kOrganClickStartLevel = 6000;
constexpr int32_t kOrganClickDecayPerSample = 136;  // ~1ms decay at 44.1kHz
constexpr int32_t kEnvelopeSnapThreshold = 64;
constexpr int32_t kReleaseFloor = 64;

// Precomputed phase steps for constant-frequency oscillators (avoids float in inner loop).
constexpr uint32_t kLfoPhaseStep = static_cast<uint32_t>((5.0 * 4294967296.0) / config::kSampleRate);
constexpr uint32_t kMetronomeAccentStep = static_cast<uint32_t>((1760.0 * 4294967296.0) / config::kSampleRate);
constexpr uint32_t kMetronomeNormalStep = static_cast<uint32_t>((1320.0 * 4294967296.0) / config::kSampleRate);
constexpr uint32_t kJazzOrganRotaryStep = static_cast<uint32_t>((kJazzOrganRotaryHz * 4294967296.0) / config::kSampleRate);
constexpr uint32_t kFullOrganRotaryStep = static_cast<uint32_t>((kFullOrganRotaryHz * 4294967296.0) / config::kSampleRate);

// PolyBLEP residual for anti-aliased oscillators.
// Returns correction scaled to [-amplitude, +amplitude].
// Only called when phase is within one phaseStep of a discontinuity (~once per period).
inline int32_t polyBlepQ16(uint32_t phase, uint32_t phaseStep) {
  if (phase < phaseStep) {
    const int32_t t = static_cast<int32_t>(
        (static_cast<uint64_t>(phase) << 16) / phaseStep);
    return 2 * t - (static_cast<int32_t>((static_cast<int64_t>(t) * t) >> 16)) - 65536;
  }
  const uint32_t dist = ~phase;
  if (dist < phaseStep) {
    const int32_t t = static_cast<int32_t>(
        (static_cast<uint64_t>(dist) << 16) / phaseStep);
    return (static_cast<int32_t>((static_cast<int64_t>(t) * t) >> 16)) - 2 * t + 65536;
  }
  return 0;
}

// Convert milliseconds to exponential envelope rate (Q15).
// The rate controls how fast the envelope approaches its target.
// After ms milliseconds, the envelope will have completed ~99% of its transition.
inline int16_t msToEnvRate(uint32_t ms) {
  const float samples = static_cast<float>(ms) * config::kSampleRate / 1000.0f;
  const float rate = 32768.0f * (1.0f - expf(-4.6f / samples));
  return static_cast<int16_t>(max(1L, min(32767L, static_cast<long>(rate))));
}

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

const Preset kPresets[config::kPresetCount] = {

  // ── Bank 1: Acoustic / Traditional ──────────────────────────────────────────────────────────────
  //                                   A    D    S    R  Brt Mod WT FMR FMDp Uni Det Res  FB ModD  F2R F2Dp F2Lv F3R F3Dp F3Lv VbR VbD  DefFx
  {"JazzOrg",    Waveform::Wavetable,  2,   0, 255,  35, 124,  0, 7,  1,   0, false,  0,  28, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Vibrato},  // B1 Btn 5 — WT7:Organ
  {"FullOrg",    Waveform::Wavetable,  4,   0, 255, 110, 248,  0,12,  1,   0,  true, 24,  36, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Chorus},   // B1 Btn 6 — WT12:PipeOrgan
  {"Accordion",  Waveform::Wavetable, 10, 100, 210, 160, 118, 14,11,  1,   0,  true, 14, 110, 0, 500,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Vibrato},  // B1 Btn 7 — WT11:AccordionReed
  {"Clarinet",   Waveform::Wavetable, 22, 280, 184, 260, 102, 14,10,  1,   0, false,  0,  54, 0, 500,  0,  0,  0,  0,  0,  0, 80,18, EffectType::Vibrato},  // B1 Btn 8 — WT10:Reed
  {"GrandPno",   Waveform::Wavetable,  2, 900,   0, 280, 158, 68, 0,  1,   0, false,  0,  96, 0, 600,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb},   // B1 Btn 1 — WT0:GrandPno
  {"NylonGtr",   Waveform::Wavetable,  1, 480,   0, 260,  80, 70, 2,  1,   0, false,  0, 130, 0, 250,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb},   // B1 Btn 2 — WT2:NylonGtr
  {"Violin",     Waveform::Wavetable, 18, 340, 168, 300, 136, 16, 1,  1,   0, false,  0, 104, 0, 600,  0,  0,  0,  0,  0,  0,120,24, EffectType::Reverb},   // B1 Btn 3 — WT1:Violin
  {"PanFlute",   Waveform::Wavetable, 45, 260, 190, 320,  82, 18, 3,  1,   0, false,  0,  26, 0, 600,  0,  0,  0,  0,  0,  0, 60,14, EffectType::Reverb},   // B1 Btn 4 — WT3:Flute

  // ── Bank 2: Synth / Electronic ──────────────────────────────────────────────────────────────────
  {"BitOrgan",   Waveform::Square,     1,   0, 255,  40,  60,  0, 0,  1,   0, false,  0,  20, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay},    // B2 Btn 5 — lo-fi chiptune
  {"WahClav",    Waveform::Saw,        1, 400, 120, 180, 220, 90, 0,  1,   0, false,  0, 220, 0, 150,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay},    // B2 Btn 6 — funky auto-wah
  {"Harmonica",  Waveform::Wavetable, 15, 200, 195, 280, 120, 18, 5,  1,   0, false,  0,  90, 0, 400,  0,  0,  0,  0,  0,  0,100,22, EffectType::Reverb},   // B2 Btn 7 — blues harp
  {"Cello",      Waveform::Wavetable, 30, 400, 175, 400, 110, 12, 6,  1,   0, false,  0,  86, 0, 700,  0,  0,  0,  0,  0,  0, 90,28, EffectType::Reverb},   // B2 Btn 8 — bowed cello
  {"SpacePad",   Waveform::Wavetable,300, 800, 170,2000,  90, 10, 8,  1,   0,  true, 30,  60, 0,1200,  0,  0,  0,  0,  0,  0, 40,10, EffectType::Chorus},   // B2 Btn 1 — huge sci-fi pad
  {"FunkBass",   Waveform::Saw,        1, 300,  80, 140, 200, 80, 0,  1,   0, false,  0, 240, 0, 120,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay},    // B2 Btn 2 — squelchy bass
  {"SynthLead",  Waveform::Wavetable,  4, 200, 180, 200, 170, 20,13,  1,   0,  true, 16, 110, 0, 300,  0,  0,  0,  0,  0,  0, 80,16, EffectType::Chorus},   // B2 Btn 3 — fat analog lead
  {"BellTone",   Waveform::FM,         1,1400,  12, 800, 100, 14, 0,  5,  36, false,  0,  30, 0, 400,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb},   // B2 Btn 4 — glass bell

  // ── Bank 3: Experimental / Wild ─────────────────────────────────────────────────────────────────
  {"Pluck",      Waveform::Saw,        1, 100,   0,  80, 220, 80, 0,  1,   0, false,  0, 200, 0,  80,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay},    // B3 Btn 5 — sharp synth pluck
  {"Vox",        Waveform::Wavetable, 80, 600, 165,1000, 110, 20, 8,  1,   0,  true, 12, 160, 0, 800,  0,  0,  0,  0,  0,  0, 60,16, EffectType::Reverb},   // B3 Btn 6 — haunting vocal
  {"Brass",      Waveform::Wavetable,  6, 200, 160, 180, 200, 60, 4,  1,   0, false,  0, 100, 0, 200,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Reverb},   // B3 Btn 7 — punchy brass stab
  {"RingMod",    Waveform::FM,         1, 600,  60, 300, 130, 20, 0,  8, 120, false,  0,  50,20,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Delay},    // B3 Btn 8 — metallic industrial
  {"SubBass",    Waveform::Triangle,   3, 500, 200, 200,  30,  0, 0,  1,   0, false,  0,   0, 0,   0,  0,  0,  0,  0,  0,  0,  0, 0, EffectType::Vibrato},  // B3 Btn 1 — deep sub
  {"Siren",      Waveform::Sine,       1,   0, 255, 300,  70,  0, 0,  1,   0, false,  0,  20, 0,   0,  0,  0,  0,  0,  0,  0,255,120,EffectType::Delay},    // B3 Btn 2 — wobbling siren
  {"Trumpet",    Waveform::Wavetable,  8, 250, 150, 200, 180, 40, 4,  1,   0, false,  0, 110, 0, 300,  0,  0,  0,  0,  0,  0, 70,12, EffectType::Reverb},   // B3 Btn 3 — warm trumpet
  {"Choir",      Waveform::Wavetable,200, 800, 165,1400, 106, 16, 8,  1,   0,  true, 14,  60, 0, 600,  0,  0,  0,  0,  0,  0, 50,12, EffectType::Reverb}    // B3 Btn 4 — choir pad
};

constexpr uint32_t kMidiToPhaseStepLut[128] = {
  1592507, 1687202, 1787529, 1893821, 2006433, 2125742, 2252145, 2386065,
  2527947, 2678267, 2837525, 3006253, 3185014, 3374405, 3575058, 3787642,
  4012867, 4251484, 4504291, 4772130, 5055895, 5356535, 5675051, 6012507,
  6370029, 6748811, 7150116, 7575284, 8025734, 8502969, 9008582, 9544260,
  10111791, 10713070, 11350102, 12025014, 12740059, 13497622, 14300233, 15150569,
  16051469, 17005939, 18017164, 19088521, 20223583, 21426140, 22700205, 24050029,
  25480118, 26995245, 28600466, 30301138, 32102938, 34011878, 36034329, 38177042,
  40447167, 42852281, 45400410, 48100059, 50960237, 53990491, 57200933, 60602277,
  64205876, 68023756, 72068659, 76354085, 80894335, 85704562, 90800821, 96200119,
  101920475, 107980982, 114401866, 121204555, 128411752, 136047513, 144137319, 152708170,
  161788670, 171409125, 181601642, 192400238, 203840951, 215961965, 228803732, 242409110,
  256823505, 272095026, 288274638, 305416340, 323577341, 342818251, 363203285, 384800476,
  407681903, 431923931, 457607464, 484818220, 513647011, 544190052, 576549277, 610832681,
  647154682, 685636502, 726406570, 769600953, 815363807, 863847862, 915214929, 969636440,
  1027294023, 1088380105, 1153098554, 1221665362, 1294309365, 1371273005, 1452813141, 1539201906,
  1630727614, 1727695724, 1830429858, 1939272881, 2054588047, 2176760210, 2306197108, 2443330725
};

inline uint32_t hzToPhaseStep(float hz) {
  return static_cast<uint32_t>((hz * 4294967296.0f) / config::kSampleRate);
}

inline int16_t softClip(int32_t sample) {
  constexpr int32_t kThreshold = 24000;
  if (sample > kThreshold) {
    sample = kThreshold + ((sample - kThreshold) / 4);
  } else if (sample < -kThreshold) {
    sample = -kThreshold + ((sample + kThreshold) / 4);
  }

  return static_cast<int16_t>(constrain(sample, -32768, 32767));
}
} // namespace

SynthEngine::SynthEngine(AudioOutput &audioOutput) : audioOutput_(audioOutput) {}

bool SynthEngine::begin() {
  initializeTables();
  rhythm_.begin(config::kSampleRate, kSineTable);
  rhythm_.setBpm(metronomeBpm_);
  recomputeEnvelope(presetAt(presetIndex_));
  return audioOutput_.begin(config::kSampleRate, config::kBitsPerSample);
}

void SynthEngine::noteOn(uint8_t midiNote) {
  const Preset &preset = presetAt(presetIndex_);
  Voice *voice = allocateVoice(midiNote);
  voice->active = true;
  voice->midiNote = midiNote;
  // Random start phase: prevents phasing artifacts when retriggering the same note.
  noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
  voice->phase = noiseSeed_;
  voice->phaseStep = midiNoteToPhaseStep(midiNote);
  voice->modPhase = 0;
  voice->modPhaseStep = static_cast<uint32_t>(
      min<uint64_t>(0xFFFFFFFFull,
                    static_cast<uint64_t>(voice->phaseStep) *
                        max<uint8_t>(1, preset.fmRatio)));
  voice->percussionPhase = 0;
  voice->percussionPhaseStep = voice->phaseStep * 4u;

  // FM pairs 2-3 (Algorithm 5 style).
  if (preset.fm2Level > 0) {
    voice->fm2CarrierPhase = 0;
    // Pair 2 carrier slightly detuned for richness (reuses detune field).
    voice->fm2CarrierPhaseStep = voice->phaseStep +
        ((voice->phaseStep >> 10) * preset.detune);
    voice->fm2ModPhase = 0;
    voice->fm2ModPhaseStep = static_cast<uint32_t>(
        min<uint64_t>(0xFFFFFFFFull,
                      static_cast<uint64_t>(voice->phaseStep) *
                          max<uint8_t>(1, preset.fm2Ratio)));
  }
  if (preset.fm3Level > 0) {
    voice->fm3CarrierPhase = 0;
    voice->fm3ModPhase = 0;
    voice->fm3ModPhaseStep = static_cast<uint32_t>(
        min<uint64_t>(0xFFFFFFFFull,
                      static_cast<uint64_t>(voice->phaseStep) *
                          max<uint8_t>(1, preset.fm3Ratio)));
  }

  if (preset.unison) {
    voice->unisonPhase[0] = 0;
    voice->unisonPhase[1] = 0;
  }

  // Independent spectral envelope: used by FM depth and non-FM brightness movement.
  // Uses exponential decay for natural brightness evolution.
  if (preset.modDecayMs == 0 || preset.modulationDepth == 0) {
    voice->modLevel = 0;
    voice->modDecayStep = 0;
  } else {
    voice->modLevel = 32767;
    voice->modDecayStep = msToEnvRate(preset.modDecayMs);
  }
  voice->lastModOutput = 0;

  if (presetIndex_ == kJazzOrganPresetIndex || presetIndex_ == kFullOrganPresetIndex) {
    voice->percussionLevel = kOrganPercussionStartLevel;
    const uint32_t percussionDecaySamples = max<uint32_t>(1, config::kSampleRate / kOrganPercussionDecayDivisor);
    voice->percussionDecayStep = max<int32_t>(1, voice->percussionLevel / static_cast<int32_t>(percussionDecaySamples));
    voice->clickLevel = kOrganClickStartLevel;
  } else {
    voice->percussionLevel = 0;
    voice->percussionDecayStep = 0;
    voice->clickLevel = 0;
  }

  voice->level = 0;
  voice->filterLow = 0;
  voice->filterBand = 0;

  // Per-voice stereo spread: alternate voices across the stereo field.
  // voiceIndex 0→center-left, 1→center-right, 2→more left, 3→more right, etc.
  const uint8_t voiceIndex = static_cast<uint8_t>(voice - &voices_[0]);
  static constexpr int16_t kStereoPan[8] = {-80, 80, -160, 160, -40, 40, -120, 120};
  voice->stereoPan = kStereoPan[voiceIndex & 7];

  // Per-voice vibrato: precompute step, random start phase so voices don't vibrate in sync.
  voice->vibratoStep = (preset.vibratoDepth > 0) ?
      hzToPhaseStep(2.0f + preset.vibratoRate * 0.024f) : 0;
  noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
  voice->vibratoPhase = noiseSeed_;

  voice->stage = EnvelopeStage::Attack;
}

void SynthEngine::noteOff(uint8_t midiNote) {
  for (Voice &voice : voices_) {
    if (!voice.active || voice.midiNote != midiNote) {
      continue;
    }

    if (!sustainEnabled_) {
      voice.stage = EnvelopeStage::Release;
    }
  }
}

void SynthEngine::allNotesOff() {
  for (Voice &voice : voices_) {
    voice = Voice{};
  }
}

void SynthEngine::setSustainEnabled(bool enabled) {
  sustainEnabled_ = enabled;
}

bool SynthEngine::isSustainEnabled() const {
  return sustainEnabled_;
}

void SynthEngine::setPreset(uint8_t presetIndex) {
  presetIndex_ = presetIndex % config::kPresetCount;
  const Preset &preset = presetAt(presetIndex_);
  recomputeEnvelope(preset);
  // Apply per-preset default effect and auto-enable it.
  effect_ = preset.defaultEffect;
  effectEnabled_ = true;
}

void SynthEngine::setMasterVolume(int16_t volume) {
  masterVolume_ = constrain(volume, config::kMinMasterVolume, config::kMaxMasterVolume);
}

void SynthEngine::setEffect(EffectType effect) {
  effect_ = effect;
  bitcrushHeld_ = 0;
  bitcrushCounter_ = 0;
}

void SynthEngine::setEffectEnabled(bool enabled) {
  effectEnabled_ = enabled;
}

void SynthEngine::setMetronome(bool enabled) {
  metronomeEnabled_ = enabled;
}

void SynthEngine::setMetronomeBpm(uint8_t bpm) {
  metronomeBpm_ = constrain(bpm, config::kMetronomeMinBpm, config::kMetronomeMaxBpm);
  rhythm_.setBpm(metronomeBpm_);
}

void SynthEngine::setRhythmPlaying(bool enabled) {
  rhythm_.setPlaying(enabled);
}

bool SynthEngine::isRhythmPlaying() const {
  return rhythm_.isPlaying();
}

void SynthEngine::setRhythmPattern(uint8_t patternIndex) {
  rhythm_.setPattern(patternIndex);
}

uint8_t SynthEngine::rhythmPattern() const {
  return rhythm_.currentPattern();
}

void SynthEngine::setAttackOffset(int8_t offset) {
  attackOffset_ = offset;
  recomputeEnvelope(presetAt(presetIndex_));
}

void SynthEngine::setDecayOffset(int8_t offset) {
  decayOffset_ = offset;
  recomputeEnvelope(presetAt(presetIndex_));
}

void SynthEngine::setSustainOffset(int8_t offset) {
  sustainOffset_ = offset;
  recomputeEnvelope(presetAt(presetIndex_));
}

void SynthEngine::setReleaseOffset(int8_t offset) {
  releaseOffset_ = offset;
  recomputeEnvelope(presetAt(presetIndex_));
}

const Preset &SynthEngine::presetAt(uint8_t presetIndex) {
  return kPresets[presetIndex % config::kPresetCount];
}

const RhythmPattern &SynthEngine::rhythmPatternAt(uint8_t patternIndex) {
  return RhythmEngine::patternAt(patternIndex);
}

SynthEngine::Voice *SynthEngine::allocateVoice(uint8_t midiNote) {
  (void)midiNote;
  for (Voice &voice : voices_) {
    if (!voice.active) {
      return &voice;
    }
  }

  for (Voice &voice : voices_) {
    if (voice.stage == EnvelopeStage::Release) {
      return &voice;
    }
  }

  return &voices_[0];
}

uint32_t SynthEngine::midiNoteToPhaseStep(uint8_t midiNote) {
  return kMidiToPhaseStepLut[midiNote & 0x7F];
}

void SynthEngine::initializeTables() {
  // All table data (sine + wavetables) is pre-generated in flash via WaveTableData.h.
  // Nothing to compute at runtime.
  tablesInitialized_ = true;
}

void SynthEngine::recomputeEnvelope(const Preset &preset) {
  auto applyOffset = [](uint32_t base, int8_t offset) -> uint32_t {
    if (offset == 0) return base;
    if (offset > 0) return min<uint32_t>(10000, base << offset);
    return max<uint32_t>(1, base >> (-offset));
  };

  const uint32_t attackMs = applyOffset(max<uint32_t>(1, preset.attackMs), attackOffset_);
  const uint32_t decayMs = applyOffset(max<uint32_t>(1, preset.decayMs), decayOffset_);
  const uint32_t releaseMs = applyOffset(max<uint32_t>(1, preset.releaseMs), releaseOffset_);
  const int32_t sustainLevel = (sustainOffset_ == 0) ? static_cast<int32_t>(preset.sustainLevel) :
      constrain(static_cast<int32_t>(preset.sustainLevel) + (sustainOffset_ * 32), 0, 255);

  envelope_.attackRate = msToEnvRate(attackMs);
  envelope_.decayRate = (decayMs > 0) ? msToEnvRate(decayMs) : 32767;
  envelope_.releaseRate = msToEnvRate(releaseMs);
  envelope_.sustainLevel = sustainLevel << 7;
  envelope_.filterBase = static_cast<uint8_t>(constrain(12 + (preset.brightness / 4), 12, 180));
  envelope_.filterEnvDepth = static_cast<uint8_t>(constrain(16 + (preset.brightness / 2), 16, 180));
  envelope_.baseCutoff = constrain(static_cast<int32_t>(envelope_.filterBase) + (preset.brightness / 7), 10, 150);
  envelope_.resonanceDamping = constrain(180 - (preset.resonance / 2), 28, 180);
}

int16_t SynthEngine::sampleWave(Waveform waveform, uint32_t phase, uint8_t wavetableIndex, uint32_t phaseStep, int32_t framePos) const {
  switch (waveform) {
    case Waveform::Triangle: {
      const uint16_t top = static_cast<uint16_t>(phase >> 16);
      int32_t value = (top < 32768u) ? top : (65535u - top);
      value = (value * 2) - 32767;
      return static_cast<int16_t>(value);
    }
    case Waveform::Saw: {
      int32_t raw = static_cast<int32_t>(phase >> 16) - 32768;
      if (phaseStep > 0) {
        raw -= (polyBlepQ16(phase, phaseStep) * 32768L) >> 16;
      }
      return static_cast<int16_t>(constrain(raw, -32768, 32767));
    }
    case Waveform::Square: {
      int32_t raw = (phase & 0x80000000u) ? 22000 : -22000;
      if (phaseStep > 0) {
        const int32_t blep1 = (polyBlepQ16(phase, phaseStep) * 22000L) >> 16;
        const int32_t blep2 = (polyBlepQ16(phase - 0x80000000u, phaseStep) * 22000L) >> 16;
        raw -= blep1 - blep2;
      }
      return static_cast<int16_t>(constrain(raw, -32768, 32767));
    }
    case Waveform::Sine:
      return interpolateTable(kSineTable, phase);
    case Waveform::Wavetable: {
      const uint8_t ti = wavetableIndex % kGenTableCount;
      if (framePos <= 0) {
        return interpolateTable(kWaveTableData[ti][0], phase);
      }
      // Multi-frame crossfade: framePos 0=bright(frame0), 32767=dark(frame3).
      const int32_t scaled = framePos * (kGenFrameCount - 1);
      const uint8_t frameA = static_cast<uint8_t>(scaled >> 15);
      const uint8_t frameB = (frameA < kGenFrameCount - 1) ? frameA + 1 : frameA;
      const int32_t blend = (scaled - (static_cast<int32_t>(frameA) << 15)) >> 7;
      const int16_t a = interpolateTable(kWaveTableData[ti][frameA], phase);
      const int16_t b = interpolateTable(kWaveTableData[ti][frameB], phase);
      return static_cast<int16_t>(a + (((b - a) * blend) >> 8));
    }
    case Waveform::FM:
      return interpolateTable(kSineTable, phase);
  }

  return 0;
}

int16_t SynthEngine::interpolateTable(const int16_t *table, uint32_t phase) const {
  // 512-sample table: 9-bit index from top bits, 16-bit fraction for interpolation.
  const uint16_t index = static_cast<uint16_t>(phase >> 23) & 0x1FFu;
  const uint16_t nextIndex = (index + 1) & 0x1FFu;
  const uint16_t frac = static_cast<uint16_t>((phase >> 7) & 0xFFFFu);
  const int32_t a = table[index];
  const int32_t b = table[nextIndex];
  return static_cast<int16_t>(a + (((b - a) * static_cast<int32_t>(frac)) >> 16));
}

int32_t SynthEngine::renderVoice(Voice &voice, const Preset &preset, int32_t lfo, bool metronomeAccent) {
  if (!voice.active) {
    return 0;
  }

  // Exponential envelope: fast initial movement, natural tail.
  switch (voice.stage) {
    case EnvelopeStage::Attack: {
      const int32_t distance = 32767 - voice.level;
      voice.level += max<int32_t>(1, (distance * envelope_.attackRate) >> 15);
      if (voice.level >= 32767 - kEnvelopeSnapThreshold) {
        voice.level = 32767;
        voice.stage = (envelope_.sustainLevel < 32767) ? EnvelopeStage::Decay : EnvelopeStage::Sustain;
      }
      break;
    }
    case EnvelopeStage::Decay: {
      const int32_t distance = voice.level - envelope_.sustainLevel;
      voice.level -= max<int32_t>(1, (distance * envelope_.decayRate) >> 15);
      if (voice.level <= envelope_.sustainLevel + kEnvelopeSnapThreshold) {
        voice.level = envelope_.sustainLevel;
        voice.stage = EnvelopeStage::Sustain;
      }
      break;
    }
    case EnvelopeStage::Sustain:
      voice.level = envelope_.sustainLevel;
      break;
    case EnvelopeStage::Release: {
      voice.level -= max<int32_t>(1, (voice.level * envelope_.releaseRate) >> 15);
      if (voice.level <= kReleaseFloor) {
        voice = Voice{};
        return 0;
      }
      break;
    }
    case EnvelopeStage::Off:
      voice = Voice{};
      return 0;
  }

  // Exponential spectral modulation decay (brightness evolution).
  if (voice.modDecayStep > 0) {
    voice.modLevel -= max<int32_t>(1, (voice.modLevel * voice.modDecayStep) >> 15);
    if (voice.modLevel < kReleaseFloor) {
      voice.modLevel = 0;
    }
  }
  if (voice.percussionDecayStep > 0) {
    voice.percussionLevel -= voice.percussionDecayStep;
    if (voice.percussionLevel < 0) {
      voice.percussionLevel = 0;
    }
  }
  // Key click: broadband noise burst for organ attack realism.
  if (voice.clickLevel > 0) {
    voice.clickLevel -= kOrganClickDecayPerSample;
    if (voice.clickLevel < 0) {
      voice.clickLevel = 0;
    }
  }

  uint32_t phaseStep = voice.phaseStep;

  // Built-in per-preset vibrato (independent of global effect toggle).
  if (voice.vibratoStep > 0) {
    voice.vibratoPhase += voice.vibratoStep;
    const int32_t vib = sampleWave(Waveform::Sine, voice.vibratoPhase);
    phaseStep += static_cast<uint32_t>((vib * preset.vibratoDepth) >> 6);
  }

  if (effectEnabled_) {
    switch (effect_) {
      case EffectType::Vibrato:
        phaseStep += static_cast<uint32_t>((lfo * preset.modulationDepth) >> 5);
        break;
      case EffectType::Chorus:
        phaseStep += static_cast<uint32_t>((lfo * preset.modulationDepth) >> 6);
        break;
      default:
        break;
    }
  }

  // Multi-frame wavetable position: modLevel 32767=bright → framePos 0,
  // modLevel 0=dark → framePos 32767. Drives frame crossfading in sampleWave.
  const int32_t framePos = (preset.modDecayMs > 0) ? (32767 - voice.modLevel) : 0;

  int32_t sample = 0;
  if (preset.waveform == Waveform::FM) {
    // Pair 1 (body): carrier at base frequency, modulator at fmRatio.
    const int32_t fbMod = (static_cast<int32_t>(voice.lastModOutput) * preset.fmFeedback) >> 5;
    voice.modPhase += (voice.modPhaseStep + fbMod);
    int32_t modulator = sampleWave(Waveform::Sine, voice.modPhase);
    voice.lastModOutput = modulator;
    const int32_t depth1 = (static_cast<int64_t>(voice.modLevel) * preset.fmDepth) >> 8;
    const uint32_t phaseMod1 = static_cast<uint32_t>((static_cast<int64_t>(modulator) * depth1) << 7);
    voice.phase += phaseStep;
    sample = sampleWave(Waveform::Sine, voice.phase + phaseMod1);

    // Pair 2 (richness): slightly detuned carrier, independent modulator.
    if (preset.fm2Level > 0) {
      voice.fm2ModPhase += voice.fm2ModPhaseStep;
      const int32_t mod2 = sampleWave(Waveform::Sine, voice.fm2ModPhase);
      const int32_t depth2 = (static_cast<int64_t>(voice.modLevel) * preset.fm2Depth) >> 8;
      const uint32_t phaseMod2 = static_cast<uint32_t>((static_cast<int64_t>(mod2) * depth2) << 7);
      voice.fm2CarrierPhase += voice.fm2CarrierPhaseStep;
      sample += (sampleWave(Waveform::Sine, voice.fm2CarrierPhase + phaseMod2) *
                 static_cast<int32_t>(preset.fm2Level)) >> 8;
    }

    // Pair 3 (tine/bell): carrier at base freq, high-ratio modulator for metallic attack.
    if (preset.fm3Level > 0) {
      voice.fm3ModPhase += voice.fm3ModPhaseStep;
      const int32_t mod3 = sampleWave(Waveform::Sine, voice.fm3ModPhase);
      const int32_t depth3 = (static_cast<int64_t>(voice.modLevel) * preset.fm3Depth) >> 8;
      const uint32_t phaseMod3 = static_cast<uint32_t>((static_cast<int64_t>(mod3) * depth3) << 7);
      voice.fm3CarrierPhase += phaseStep;
      sample += (sampleWave(Waveform::Sine, voice.fm3CarrierPhase + phaseMod3) *
                 static_cast<int32_t>(preset.fm3Level)) >> 8;
    }
  } else if (preset.unison) {
    // Unison: sum three oscillators (base, slightly detuned up, slightly detuned down)
    voice.phase += phaseStep;
    const uint32_t detuneStep = (phaseStep >> 10) * preset.detune;
    voice.unisonPhase[0] += (phaseStep + detuneStep);
    voice.unisonPhase[1] += (phaseStep - detuneStep);

    sample = sampleWave(preset.waveform, voice.phase, preset.wavetableIndex, phaseStep, framePos);
    sample += sampleWave(preset.waveform, voice.unisonPhase[0], preset.wavetableIndex, phaseStep + detuneStep, framePos);
    sample += sampleWave(preset.waveform, voice.unisonPhase[1], preset.wavetableIndex, phaseStep - detuneStep, framePos);
    sample /= 3;
  } else {
    voice.phase += phaseStep;
    sample = sampleWave(preset.waveform, voice.phase, preset.wavetableIndex, phaseStep, framePos);
  }

  // Organ key click: inject filtered noise burst at note onset.
  if (voice.clickLevel > 0) {
    noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
    const int16_t noiseVal = static_cast<int16_t>(noiseSeed_ >> 16);
    sample += (static_cast<int32_t>(noiseVal) * voice.clickLevel) >> 15;
  }

  if ((presetIndex_ == kJazzOrganPresetIndex || presetIndex_ == kFullOrganPresetIndex) && voice.percussionLevel > 0) {
    voice.percussionPhase += voice.percussionPhaseStep;
    const int32_t percussion = (static_cast<int32_t>(sampleWave(Waveform::Sine, voice.percussionPhase)) * voice.percussionLevel) >> 15;
    sample += percussion;
  }

  if (effectEnabled_ && effect_ == EffectType::Tremolo) {
    sample = (sample * (180 + (lfo >> 9))) >> 8;
  }

  if (metronomeAccent) {
    sample += sample >> 3;  // ~1.125x boost, avoids division
  }

  const int32_t envAmount = (voice.level * envelope_.filterEnvDepth) >> 15;
  const int32_t spectralEnv = (static_cast<int64_t>(voice.modLevel) * preset.modulationDepth) >> 15;
  // Note-dependent brightness: higher notes get brighter filter cutoff (+1 per 4 semitones above C3).
  const int32_t noteBrightness = (static_cast<int32_t>(voice.midiNote) - 48) >> 2;
  const int32_t filterCutoff = constrain(envelope_.baseCutoff + (envAmount >> 1) + spectralEnv + noteBrightness, 10, 188);
  voice.filterLow += (filterCutoff * voice.filterBand) >> 8;
  const int32_t filterHigh = sample - voice.filterLow - ((voice.filterBand * envelope_.resonanceDamping) >> 8);
  voice.filterBand += (filterCutoff * filterHigh) >> 8;
  sample = voice.filterLow;

  return (sample * voice.level) >> 15;
}

void SynthEngine::renderFrames(size_t frameCount, uint16_t micLevel) {
  const Preset &preset = presetAt(presetIndex_);
  (void)micLevel;
  const bool needsLfo = effectEnabled_ &&
      (effect_ == EffectType::Vibrato || effect_ == EffectType::Chorus || effect_ == EffectType::Tremolo);
  const bool organRotary = (presetIndex_ == kJazzOrganPresetIndex || presetIndex_ == kFullOrganPresetIndex);

  static int16_t outL[config::kAudioBlockFrames];
  static int16_t outR[config::kAudioBlockFrames];

  const uint32_t startUs = micros();

  // Precompute rotary phase step outside inner loop (avoids float per sample).
  const uint32_t rotaryStep = organRotary ?
      ((presetIndex_ == kJazzOrganPresetIndex) ? kJazzOrganRotaryStep : kFullOrganRotaryStep) : 0;
  const int32_t rotaryWet = organRotary ?
      ((presetIndex_ == kJazzOrganPresetIndex) ? kJazzOrganRotaryWet : kFullOrganRotaryWet) : 0;

  // Find first active voice for rotary wet signal. -1 = none active.
  int8_t rotaryVoiceIdx = -1;
  if (organRotary) {
    for (uint8_t v = 0; v < config::kPolyphony; ++v) {
      if (voices_[v].active) { rotaryVoiceIdx = v; break; }
    }
  }

  for (size_t i = 0; i < frameCount; ++i) {
    int32_t lfo = 0;
    if (needsLfo) {
      lfoPhase_ += kLfoPhaseStep;
      lfo = sampleWave(Waveform::Triangle, lfoPhase_);
    }

    bool metronomeAccent = false;
    int32_t metronomeSample = 0;
    if (metronomeEnabled_) {
      if (metronomeSamplesUntilClick_ == 0) {
        metronomeClickSamples_ = config::kSampleRate / 80;
        metronomeSamplesUntilClick_ = (config::kSampleRate * 60u) / metronomeBpm_;
        metronomeIsAccentBeat_ = true;
      } else {
        metronomeSamplesUntilClick_--;
      }

      if (metronomeClickSamples_ > 0) {
        metronomeAccent = metronomeIsAccentBeat_;
        metronomePhase_ += metronomeAccent ? kMetronomeAccentStep : kMetronomeNormalStep;
        const int32_t clickWave = (metronomePhase_ & 0x80000000u) ? 1 : -1;
        const int32_t clickEnvelope = metronomeAccent ? 18000 : 12000;
        metronomeSample = clickWave * clickEnvelope;
        metronomeClickSamples_--;
      } else {
        metronomePhase_ = 0;
        metronomeIsAccentBeat_ = false;
      }
    }

    const int32_t rhythmSample = rhythm_.renderSample();
    int32_t tonalMixL = 0;
    int32_t tonalMixR = 0;
    uint8_t activeVoiceCount = 0;
    for (Voice &voice : voices_) {
      if (!voice.active) continue;
      ++activeVoiceCount;
      const int32_t voiceSample = renderVoice(voice, preset, lfo, metronomeAccent);
      // Per-voice stereo panning: pan is -256..+256.
      const int32_t panR = 256 + voice.stereoPan;  // 0..512
      const int32_t panL = 512 - panR;              // 512..0
      tonalMixL += (voiceSample * panL) >> 9;
      tonalMixR += (voiceSample * panR) >> 9;
    }

    if (activeVoiceCount > 1) {
      tonalMixL = (tonalMixL * 3) / (activeVoiceCount + 2);
      tonalMixR = (tonalMixR * 3) / (activeVoiceCount + 2);
    }

    const int32_t mixL = tonalMixL + rhythmSample;
    const int32_t mixR = tonalMixR + rhythmSample;
    // Collapse to mono for effect processing.
    int32_t mix = ((mixL + mixR) >> 1) * masterVolume_ >> 15;

    if (effectEnabled_) {
      if (effect_ == EffectType::Delay) {
        const int16_t delayed = delayBuffer_[delayIndex_];
        delayBuffer_[delayIndex_] = static_cast<int16_t>((mix + (delayed >> 1)) >> 1);
        mix += delayed >> 1;
        delayIndex_ = (delayIndex_ + 1) & (kDelaySamples - 1);
      } else if (effect_ == EffectType::Bitcrusher) {
        if (bitcrushCounter_++ >= 3) {
          bitcrushHeld_ = (mix >> 12) << 12;
          bitcrushCounter_ = 0;
        }
        mix = bitcrushHeld_;
      }
    }

    mix += metronomeSample;

    if (organRotary && rotaryVoiceIdx >= 0) {
      const uint32_t rotaryPhase = lfoPhase_ + rotaryStep;
      const int16_t rotaryL = sampleWave(Waveform::Sine, rotaryPhase);
      const int16_t rotaryR = sampleWave(Waveform::Sine, rotaryPhase + 0x40000000u);
      const int32_t ampL = 224 + (rotaryL >> 10);
      const int32_t ampR = 224 + (rotaryR >> 10);
      const int32_t phaseOffset = (presetIndex_ == kJazzOrganPresetIndex) ? (rotaryL >> 3) : (rotaryL >> 2);
      const int32_t stereoOffset = (presetIndex_ == kJazzOrganPresetIndex) ? (rotaryR >> 3) : (rotaryR >> 2);
      const uint32_t rotaryBasePhase = voices_[rotaryVoiceIdx].phase;
      const int32_t leftSample = sampleWave(preset.waveform, rotaryBasePhase + static_cast<uint32_t>(phaseOffset), preset.wavetableIndex);
      const int32_t rightSample = sampleWave(preset.waveform, rotaryBasePhase + static_cast<uint32_t>(stereoOffset), preset.wavetableIndex);
      const int32_t leftMix = (mix * ampL) >> 8;
      const int32_t rightMix = (mix * ampR) >> 8;
      outL[i] = softClip((leftMix * (256 - rotaryWet) + leftSample * rotaryWet) >> 8);
      outR[i] = softClip((rightMix * (256 - rotaryWet) + rightSample * rotaryWet) >> 8);
    } else if (effectEnabled_ && effect_ == EffectType::Chorus) {
      const int16_t chorusLfoLeft = sampleWave(Waveform::Sine, lfoPhase_);
      const int16_t chorusLfoRight = sampleWave(Waveform::Sine, lfoPhase_ + 0x40000000u);
      const size_t baseDelay = 132;
      const size_t sweep = 28 + (preset.modulationDepth / 2);
      const size_t leftDelay = baseDelay + ((static_cast<int32_t>(chorusLfoLeft) + 32768) * static_cast<int32_t>(sweep) >> 16);
      const size_t rightDelay = baseDelay + ((static_cast<int32_t>(chorusLfoRight) + 32768) * static_cast<int32_t>(sweep) >> 16);
      const size_t leftTap = (chorusIndex_ + kChorusBufferSamples - (leftDelay & (kChorusBufferSamples - 1))) & (kChorusBufferSamples - 1);
      const size_t rightTap = (chorusIndex_ + kChorusBufferSamples - (rightDelay & (kChorusBufferSamples - 1))) & (kChorusBufferSamples - 1);
      const int32_t wetLeft = chorusBuffer_[leftTap];
      const int32_t wetRight = chorusBuffer_[rightTap];
      chorusBuffer_[chorusIndex_] = softClip(mix);
      chorusIndex_ = (chorusIndex_ + 1) & (kChorusBufferSamples - 1);
      outL[i] = softClip((mix * 3 + wetLeft * 2) >> 2);
      outR[i] = softClip((mix * 3 + wetRight * 2) >> 2);
    } else if (effectEnabled_ && effect_ == EffectType::Reverb) {
      const int32_t dry = mix;
      const int32_t tapL1 = reverbBufferL1_[reverbIndexL1_];
      const int32_t tapL2 = reverbBufferL2_[reverbIndexL2_];
      const int32_t tapR1 = reverbBufferR1_[reverbIndexR1_];
      const int32_t tapR2 = reverbBufferR2_[reverbIndexR2_];
      const int32_t wetLeft = tapL1 + (tapL2 >> 1) + (tapR1 >> 2);
      const int32_t wetRight = tapR1 + (tapR2 >> 1) + (tapL1 >> 2);

      reverbBufferL1_[reverbIndexL1_] = softClip((dry + (tapL2 >> 1) + (tapR1 >> 2)) >> 1);
      reverbBufferL2_[reverbIndexL2_] = softClip((dry + (tapL1 >> 1) + (tapR2 >> 3)) >> 1);
      reverbBufferR1_[reverbIndexR1_] = softClip((dry + (tapR2 >> 1) + (tapL1 >> 2)) >> 1);
      reverbBufferR2_[reverbIndexR2_] = softClip((dry + (tapR1 >> 1) + (tapL2 >> 3)) >> 1);

      if (++reverbIndexL1_ >= kReverbShortSamples) reverbIndexL1_ = 0;
      if (++reverbIndexL2_ >= kReverbLongSamples) reverbIndexL2_ = 0;
      if (++reverbIndexR1_ >= kReverbMediumSamples) reverbIndexR1_ = 0;
      if (++reverbIndexR2_ >= kReverbXLargeSamples) reverbIndexR2_ = 0;

      outL[i] = softClip((dry * 3 + wetLeft) >> 2);
      outR[i] = softClip((dry * 3 + wetRight) >> 2);
    } else {
      // Stereo output from per-voice panning (no effect active).
      // Add 1-bit triangular dither to reduce quantization noise on quiet tails.
      noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
      const int32_t dither = static_cast<int32_t>((noiseSeed_ >> 30) & 3) - 1; // -1, 0, or 1
      outL[i] = softClip(((mixL * masterVolume_) >> 15) + metronomeSample + dither);
      outR[i] = softClip(((mixR * masterVolume_) >> 15) + metronomeSample + dither);
      continue;  // skip the mono mix path below
    }
  }

  lastProcessingUs_ = micros() - startUs;

  for (size_t i = 0; i < frameCount; ++i) {
    audioOutput_.writeStereo(outL[i], outR[i]);
  }
}
