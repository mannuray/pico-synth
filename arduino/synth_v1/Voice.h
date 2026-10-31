#pragma once

#include "Dsp.h"
#include "EffectTypes.h"
#include "SampleSet.h"
#include "SynthTypes.h"
#include "VoiceShape.h"

// One sounding note: oscillators (plain, unison or 3-pair FM), organ
// percussion and click, vibrato, amplitude envelope and resonant filter.
class Voice {
public:
  // keyNote is the note played; the preset's octave shifts the pitch, not the key.
  // percussion: whether organ percussion triggers (Hammond-style single trigger).
  void start(uint8_t keyNote, const Preset &preset, int16_t stereoPan, bool percussion, dsp::NoiseSource &noise);
  void release() { stage_ = Stage::Release; }
  void reset() { *this = Voice{}; }

  // Slow-changing values (filter cutoff, spectral envelope, vibrato, effect
  // modulation) are updated once per this many samples; amplitude every sample.
  static constexpr size_t kControlInterval = 8;

  // Renders `frames` samples, adding the panned output to left/right.
  // modulation has one entry per frame. Returns how many frames the voice was
  // active: fewer than `frames` if the note died away (it then resets itself).
  size_t renderBlock(const Preset &preset, const VoiceShape &shape, const VoiceModulation *modulation,
                     size_t frames, int32_t *left, int32_t *right, dsp::NoiseSource &noise);

  bool active() const { return active_; }
  bool releasing() const { return stage_ == Stage::Release; }
  uint8_t midiNote() const { return keyNote_; }
  int32_t level() const { return level_; }  // envelope level, Q30
  int16_t stereoPan() const { return stereoPan_; }  // -256 (full left) to +256 (full right)

private:
  enum class Stage : uint8_t {
    Off = 0,
    Attack,
    Decay,
    Sustain,
    Release
  };

  bool active_ = false;
  uint8_t keyNote_ = 0;
  Stage stage_ = Stage::Off;
  int16_t stereoPan_ = 0;

  // Oscillators
  uint32_t phase_ = 0;
  uint32_t phaseStep_ = 0;
  uint32_t unisonPhase_[2] = {0, 0};
  uint32_t modPhase_ = 0;
  uint32_t modPhaseStep_ = 0;
  int16_t lastModOutput_ = 0;
  uint32_t fm2CarrierPhase_ = 0;
  uint32_t fm2CarrierPhaseStep_ = 0;
  uint32_t fm2ModPhase_ = 0;
  uint32_t fm2ModPhaseStep_ = 0;
  uint32_t fm3CarrierPhase_ = 0;
  uint32_t fm3ModPhase_ = 0;
  uint32_t fm3ModPhaseStep_ = 0;

  // Envelopes (Q30 levels, Q20 rates)
  int32_t level_ = 0;
  int32_t modLevel_ = 0;
  int32_t modSustain_ = 0;
  uint32_t modDecayRate_ = 0;     // 0 = spectral envelope stays at full

  // Organ
  uint32_t percussionPhase_ = 0;
  uint32_t percussionPhaseStep_ = 0;
  int32_t percussionLevel_ = 0;
  uint32_t percussionDecayRate_ = 0;
  int32_t percussionPeak_ = 0;
  int32_t clickLevel_ = 0;

  // Vibrato
  uint32_t vibratoPhase_ = 0;
  uint32_t vibratoStep_ = 0;      // 0 = off
  uint32_t vibratoDepthStep_ = 0; // peak phase-step deviation
  int32_t vibratoFade_ = 0;       // Q15, ramps up over the vibrato delay
  int32_t vibratoFadeStep_ = 0;

  // Recorded sample playback (Waveform::Sample); position is Q20.12
  const SampleZone *zone_ = nullptr;
  uint32_t samplePos_ = 0;
  uint32_t sampleInc_ = 0;       // current increment (follows vibrato/pitch modulation)
  uint32_t sampleIncScale_ = 0;  // increment = phaseStep * scale / 2^32
  bool sampleEnded_ = false;

  dsp::StateVariableLowPass filter_;

  void advanceEnvelope(const VoiceShape &shape);
  int32_t advanceSpectralEnvelope(uint32_t samples);
  uint32_t currentPhaseStep(const VoiceModulation &modulation, uint32_t samples);
  int32_t oscillate(const Preset &preset, uint32_t phaseStep, int32_t modLevel15);
  int32_t organSounds(dsp::NoiseSource &noise);
  int32_t filterCutoff(const Preset &preset, const VoiceShape &shape, int32_t modLevel15) const;
};
