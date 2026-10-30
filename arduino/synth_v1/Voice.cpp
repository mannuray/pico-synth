#include "Voice.h"
#include "Oscillator.h"

namespace {
constexpr int32_t kClickDecayPerSample = 136;          // ~1 ms key-click burst at 44.1 kHz
constexpr int32_t kEnvelopeSnap = 64 << 15;            // ~-54 dB: treat as arrived
constexpr int32_t kDecayKnee = kEnvelopeFull / 4;      // -12 dB: where decay2 takes over
constexpr float kCentsToRatio = 0.000577623f;          // ln(2) / 1200

uint32_t ratioStep(uint32_t phaseStep, uint16_t ratioX100) {
  return static_cast<uint32_t>(min<uint64_t>(0xFFFFFFFFull, static_cast<uint64_t>(phaseStep) * ratioX100 / 100));
}

// Phase offset for FM: depth ~= 10x the modulation index. Wraps modulo 2^32 on purpose.
uint32_t fmPhaseOffset(int32_t modulator, int32_t modLevel15, uint8_t depth) {
  const int32_t scaledDepth = (modLevel15 * depth) >> 8;
  return static_cast<uint32_t>(modulator * scaledDepth) * 16u;
}
}  // namespace

void Voice::start(uint8_t keyNote, const Preset &preset, int16_t stereoPan, bool percussion, dsp::NoiseSource &noise) {
  *this = Voice{};
  active_ = true;
  keyNote_ = keyNote;
  stereoPan_ = stereoPan;

  const uint8_t pitchNote = static_cast<uint8_t>(constrain(keyNote + preset.octave * 12, 0, 127));
  // Random start phase: prevents phasing artifacts when retriggering the same note.
  phase_ = noise.next();
  phaseStep_ = osc::noteToPhaseStep(pitchNote);

  if (preset.unison) {
    unisonPhase_[0] = noise.next();
    unisonPhase_[1] = noise.next();
  }

  modPhaseStep_ = ratioStep(phaseStep_, preset.fmRatioX100);
  if (preset.fm2Level > 0) {
    fm2CarrierPhaseStep_ = phaseStep_ + ((phaseStep_ >> 10) * preset.detune);
    fm2ModPhaseStep_ = ratioStep(phaseStep_, preset.fm2RatioX100);
  }
  if (preset.fm3Level > 0) {
    fm3ModPhaseStep_ = ratioStep(phaseStep_, preset.fm3RatioX100);
  }

  // Spectral envelope: starts bright, settles at modSustain over modDecayMs.
  modLevel_ = kEnvelopeFull;
  if (preset.modDecayMs > 0) {
    modSustain_ = static_cast<int32_t>(preset.modSustain) << 22;
    modDecayRate_ = msToEnvRate(preset.modDecayMs);
  }

  if (preset.organ != nullptr) {
    const OrganTraits &organ = *preset.organ;
    if (percussion && organ.percussionHarmonic > 0) {
      percussionPhaseStep_ = phaseStep_ * organ.percussionHarmonic;
      percussionLevel_ = kEnvelopeFull;
      percussionDecayRate_ = msToEnvRate(organ.percussionDecayMs);
      percussionPeak_ = organ.percussionLevel;
    }
    clickLevel_ = organ.clickLevel;
  }

  if (preset.vibratoCents > 0) {
    vibratoStep_ = osc::hzToPhaseStep(preset.vibratoRateX10 / 10.0f);
    vibratoDepthStep_ = static_cast<uint32_t>(phaseStep_ * (preset.vibratoCents * kCentsToRatio));
    vibratoPhase_ = noise.next();  // voices don't vibrate in sync
    if (preset.vibratoDelayMs > 0) {
      vibratoFadeStep_ = max<int32_t>(1, 32767 / static_cast<int32_t>(preset.vibratoDelayMs * (config::kSampleRate / 1000)));
    } else {
      vibratoFade_ = 32767;
    }
  }

  stage_ = Stage::Attack;
}

int32_t Voice::render(const Preset &preset, const VoiceShape &shape, const VoiceModulation &modulation, dsp::NoiseSource &noise) {
  if (!active_) {
    return 0;
  }
  advanceEnvelope(shape);
  if (!active_) {
    return 0;
  }

  const int32_t modLevel15 = advanceSpectralEnvelope();
  int32_t sample = oscillate(preset, currentPhaseStep(modulation), modLevel15);
  sample += organSounds(noise);
  if (modulation.gainQ8 != 256) {
    sample = (sample * modulation.gainQ8) >> 8;
  }

  const int32_t filtered = filter_.process(sample, filterCutoff(preset, shape, modLevel15), shape.resonanceDamping);
  const int32_t out = (filtered * (level_ >> 15)) >> 15;
  return (out * preset.gain) >> 7;
}

// Moves the spectral envelope toward its sustain level; returns it as Q15.
int32_t Voice::advanceSpectralEnvelope() {
  if (modDecayRate_ > 0 && modLevel_ > modSustain_) {
    modLevel_ -= max<int32_t>(1, scaleByRate(modLevel_ - modSustain_, modDecayRate_));
  }
  return modLevel_ >> 15;
}

// Organ key click (noise burst at note onset) and percussion (a decaying sine
// at the 2nd or 3rd harmonic). Zero for non-organ presets.
int32_t Voice::organSounds(dsp::NoiseSource &noise) {
  int32_t sample = 0;
  if (clickLevel_ > 0) {
    const int16_t noiseVal = static_cast<int16_t>(noise.next() >> 16);
    sample += (static_cast<int32_t>(noiseVal) * clickLevel_) >> 15;
    clickLevel_ = max<int32_t>(0, clickLevel_ - kClickDecayPerSample);
  }
  if (percussionLevel_ > kEnvelopeSnap) {
    percussionPhase_ += percussionPhaseStep_;
    const int32_t envelope = ((percussionLevel_ >> 15) * percussionPeak_) >> 15;
    sample += (osc::sine(percussionPhase_) * envelope) >> 15;
    percussionLevel_ -= scaleByRate(percussionLevel_, percussionDecayRate_);
  }
  return sample;
}

// The cutoff follows the amplitude envelope, the spectral envelope and the
// note (+1 per 4 semitones above C3).
int32_t Voice::filterCutoff(const Preset &preset, const VoiceShape &shape, int32_t modLevel15) const {
  const int32_t envAmount = ((level_ >> 15) * shape.filterEnvDepth) >> 15;
  const int32_t spectralEnv = (modLevel15 * preset.modulationDepth) >> 15;
  const int32_t noteBrightness = (static_cast<int32_t>(keyNote_) - 48) >> 2;
  return constrain(shape.baseCutoff + (envAmount >> 1) + spectralEnv + noteBrightness, 10, 230);
}

void Voice::advanceEnvelope(const VoiceShape &shape) {
  switch (stage_) {
    case Stage::Attack:
      level_ += max<int32_t>(1, scaleByRate(kEnvelopeFull - level_, shape.attackRate));
      if (level_ >= kEnvelopeFull - kEnvelopeSnap) {
        level_ = kEnvelopeFull;
        stage_ = (shape.sustainLevel < kEnvelopeFull) ? Stage::Decay : Stage::Sustain;
      }
      break;
    case Stage::Decay: {
      // Two-stage decay: a quick initial drop, then the slower decay2 tail below the knee.
      const uint32_t rate = level_ > kDecayKnee ? shape.decayRate : shape.decay2Rate;
      level_ -= max<int32_t>(1, scaleByRate(level_ - shape.sustainLevel, rate));
      if (level_ <= shape.sustainLevel + kEnvelopeSnap) {
        if (shape.sustainLevel == 0) {
          reset();  // decayed to silence while held (piano, guitar)
          return;
        }
        level_ = shape.sustainLevel;
        stage_ = Stage::Sustain;
      }
      break;
    }
    case Stage::Sustain:
      level_ = shape.sustainLevel;
      break;
    case Stage::Release:
      level_ -= max<int32_t>(1, scaleByRate(level_, shape.releaseRate));
      if (level_ <= kEnvelopeSnap) {
        reset();
      }
      break;
    case Stage::Off:
      reset();
      break;
  }
}

uint32_t Voice::currentPhaseStep(const VoiceModulation &modulation) {
  uint32_t phaseStep = phaseStep_;

  // Built-in vibrato, depth in cents, fading in over the vibrato delay.
  if (vibratoStep_ > 0) {
    vibratoPhase_ += vibratoStep_;
    if (vibratoFade_ < 32767) {
      vibratoFade_ = min<int32_t>(32767, vibratoFade_ + vibratoFadeStep_);
    }
    const int32_t wobble = (osc::sine(vibratoPhase_) * vibratoFade_) >> 15;
    // wobble * depth / 32768, split so each product fits in 32 bits.
    phaseStep += static_cast<uint32_t>((wobble >> 4) * static_cast<int32_t>(vibratoDepthStep_ >> 11));
  }

  // Effect pitch modulation (vibrato/chorus effects), relative to the note.
  if (modulation.pitchFracQ16 != 0) {
    phaseStep += static_cast<uint32_t>((static_cast<int32_t>(phaseStep_ >> 12) * modulation.pitchFracQ16) >> 4);
  }
  return phaseStep;
}

int32_t Voice::oscillate(const Preset &preset, uint32_t phaseStep, int32_t modLevel15) {
  // Wavetables move from the bright first frame toward the dark last frame
  // as the spectral envelope falls; with no spectral decay they stay bright.
  const int32_t framePos = modDecayRate_ > 0 ? (32767 - modLevel15) : 0;

  if (preset.waveform == Waveform::FM) {
    // Pair 1 (body): carrier at pitch, modulator at fmRatioX100 / 100, optional feedback.
    const int32_t feedback = (static_cast<int32_t>(lastModOutput_) * preset.fmFeedback) >> 5;
    modPhase_ += modPhaseStep_ + static_cast<uint32_t>(feedback);
    const int32_t modulator = osc::sine(modPhase_);
    lastModOutput_ = static_cast<int16_t>(modulator);
    phase_ += phaseStep;
    int32_t sample = osc::sine(phase_ + fmPhaseOffset(modulator, modLevel15, preset.fmDepth));

    // Pair 2 (richness): slightly detuned carrier, its own modulator.
    if (preset.fm2Level > 0) {
      fm2ModPhase_ += fm2ModPhaseStep_;
      const int32_t mod2 = osc::sine(fm2ModPhase_);
      fm2CarrierPhase_ += fm2CarrierPhaseStep_;
      sample += (osc::sine(fm2CarrierPhase_ + fmPhaseOffset(mod2, modLevel15, preset.fm2Depth)) * preset.fm2Level) >> 8;
    }

    // Pair 3 (e.g. an electric-piano tine): carrier at pitch, high-ratio modulator.
    if (preset.fm3Level > 0) {
      fm3ModPhase_ += fm3ModPhaseStep_;
      const int32_t mod3 = osc::sine(fm3ModPhase_);
      fm3CarrierPhase_ += phaseStep;
      sample += (osc::sine(fm3CarrierPhase_ + fmPhaseOffset(mod3, modLevel15, preset.fm3Depth)) * preset.fm3Level) >> 8;
    }
    return sample;
  }

  phase_ += phaseStep;
  if (preset.unison) {
    // Three oscillators: at pitch, detuned up, detuned down.
    const uint32_t detuneStep = (phaseStep >> 10) * preset.detune;
    unisonPhase_[0] += phaseStep + detuneStep;
    unisonPhase_[1] += phaseStep - detuneStep;
    int32_t sample = osc::sample(preset.waveform, phase_, preset.wavetableIndex, phaseStep, framePos);
    sample += osc::sample(preset.waveform, unisonPhase_[0], preset.wavetableIndex, phaseStep + detuneStep, framePos);
    sample += osc::sample(preset.waveform, unisonPhase_[1], preset.wavetableIndex, phaseStep - detuneStep, framePos);
    return sample / 3;
  }
  return osc::sample(preset.waveform, phase_, preset.wavetableIndex, phaseStep, framePos);
}
