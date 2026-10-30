#pragma once

#include "Dsp.h"
#include "EffectTypes.h"
#include "SynthTypes.h"
#include "VoiceShape.h"

// One sounding note: oscillators (plain, unison or 3-pair FM), organ
// percussion and click, amplitude envelope and resonant filter.
class Voice {
public:
  void start(uint8_t midiNote, const Preset &preset, int16_t stereoPan, dsp::NoiseSource &noise);
  void release() { stage_ = Stage::Release; }
  void reset() { *this = Voice{}; }

  // Advances one sample. Returns 0 and resets itself once the release ends.
  int32_t render(const Preset &preset, const VoiceShape &shape, const VoiceModulation &modulation, dsp::NoiseSource &noise);

  bool active() const { return active_; }
  bool releasing() const { return stage_ == Stage::Release; }
  uint8_t midiNote() const { return midiNote_; }
  int32_t level() const { return level_; }  // envelope level, 0..32767
  uint32_t phase() const { return phase_; }
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
  uint8_t midiNote_ = 0;
  uint32_t phase_ = 0;
  uint32_t phaseStep_ = 0;
  uint32_t modPhase_ = 0;
  uint32_t modPhaseStep_ = 0;
  uint32_t percussionPhase_ = 0;
  uint32_t percussionPhaseStep_ = 0;
  uint32_t unisonPhase_[2] = {0, 0};
  // FM pairs 2-3 phase accumulators
  uint32_t fm2CarrierPhase_ = 0;
  uint32_t fm2CarrierPhaseStep_ = 0;
  uint32_t fm2ModPhase_ = 0;
  uint32_t fm2ModPhaseStep_ = 0;
  uint32_t fm3CarrierPhase_ = 0;
  uint32_t fm3ModPhase_ = 0;
  uint32_t fm3ModPhaseStep_ = 0;
  int32_t level_ = 0;
  int32_t modLevel_ = 0;
  int32_t modDecayStep_ = 0;
  int32_t percussionLevel_ = 0;
  int32_t percussionDecayStep_ = 0;
  int32_t clickLevel_ = 0;
  int16_t lastModOutput_ = 0;
  int32_t filterLow_ = 0;
  int32_t filterBand_ = 0;
  uint32_t vibratoPhase_ = 0;    // per-voice vibrato LFO phase
  uint32_t vibratoStep_ = 0;     // precomputed vibrato phase increment
  int16_t stereoPan_ = 0;
  Stage stage_ = Stage::Off;
};
