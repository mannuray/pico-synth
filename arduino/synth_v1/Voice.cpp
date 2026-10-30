#include "Voice.h"
#include "Oscillator.h"

namespace {
constexpr int32_t kOrganPercussionStartLevel = 22000;
constexpr uint32_t kOrganPercussionDecayDivisor = 35u;
constexpr int32_t kOrganClickStartLevel = 6000;
constexpr int32_t kOrganClickDecayPerSample = 136;  // ~1ms decay at 44.1kHz
constexpr int32_t kEnvelopeSnapThreshold = 64;
constexpr int32_t kReleaseFloor = 64;
}  // namespace

void Voice::start(uint8_t midiNote, const Preset &preset, int16_t stereoPan, dsp::NoiseSource &noise) {
  active_ = true;
  midiNote_ = midiNote;
  // Random start phase: prevents phasing artifacts when retriggering the same note.
  phase_ = noise.next();
  phaseStep_ = osc::noteToPhaseStep(midiNote);
  modPhase_ = 0;
  modPhaseStep_ = static_cast<uint32_t>(
      min<uint64_t>(0xFFFFFFFFull,
                    static_cast<uint64_t>(phaseStep_) *
                        max<uint8_t>(1, preset.fmRatio)));
  percussionPhase_ = 0;
  percussionPhaseStep_ = phaseStep_ * 4u;

  // FM pairs 2-3 (Algorithm 5 style).
  if (preset.fm2Level > 0) {
    fm2CarrierPhase_ = 0;
    // Pair 2 carrier slightly detuned for richness (reuses detune field).
    fm2CarrierPhaseStep_ = phaseStep_ +
        ((phaseStep_ >> 10) * preset.detune);
    fm2ModPhase_ = 0;
    fm2ModPhaseStep_ = static_cast<uint32_t>(
        min<uint64_t>(0xFFFFFFFFull,
                      static_cast<uint64_t>(phaseStep_) *
                          max<uint8_t>(1, preset.fm2Ratio)));
  }
  if (preset.fm3Level > 0) {
    fm3CarrierPhase_ = 0;
    fm3ModPhase_ = 0;
    fm3ModPhaseStep_ = static_cast<uint32_t>(
        min<uint64_t>(0xFFFFFFFFull,
                      static_cast<uint64_t>(phaseStep_) *
                          max<uint8_t>(1, preset.fm3Ratio)));
  }

  if (preset.unison) {
    unisonPhase_[0] = 0;
    unisonPhase_[1] = 0;
  }

  // Independent spectral envelope: used by FM depth and non-FM brightness movement.
  // Uses exponential decay for natural brightness evolution.
  if (preset.modDecayMs == 0 || preset.modulationDepth == 0) {
    modLevel_ = 0;
    modDecayStep_ = 0;
  } else {
    modLevel_ = 32767;
    modDecayStep_ = msToEnvRate(preset.modDecayMs);
  }
  lastModOutput_ = 0;

  if (preset.organ != nullptr) {
    percussionLevel_ = kOrganPercussionStartLevel;
    const uint32_t percussionDecaySamples = max<uint32_t>(1, config::kSampleRate / kOrganPercussionDecayDivisor);
    percussionDecayStep_ = max<int32_t>(1, percussionLevel_ / static_cast<int32_t>(percussionDecaySamples));
    clickLevel_ = kOrganClickStartLevel;
  } else {
    percussionLevel_ = 0;
    percussionDecayStep_ = 0;
    clickLevel_ = 0;
  }

  level_ = 0;
  filterLow_ = 0;
  filterBand_ = 0;

  stereoPan_ = stereoPan;

  // Per-voice vibrato: precompute step, random start phase so voices don't vibrate in sync.
  vibratoStep_ = (preset.vibratoDepth > 0) ?
      osc::hzToPhaseStep(2.0f + preset.vibratoRate * 0.024f) : 0;
  vibratoPhase_ = noise.next();

  stage_ = Stage::Attack;
}

int32_t Voice::render(const Preset &preset, const VoiceShape &shape, const VoiceModulation &modulation, dsp::NoiseSource &noise) {
  if (!active_) {
    return 0;
  }

  // Exponential envelope: fast initial movement, natural tail.
  switch (stage_) {
    case Stage::Attack: {
      const int32_t distance = 32767 - level_;
      level_ += max<int32_t>(1, (distance * shape.attackRate) >> 15);
      if (level_ >= 32767 - kEnvelopeSnapThreshold) {
        level_ = 32767;
        stage_ = (shape.sustainLevel < 32767) ? Stage::Decay : Stage::Sustain;
      }
      break;
    }
    case Stage::Decay: {
      const int32_t distance = level_ - shape.sustainLevel;
      level_ -= max<int32_t>(1, (distance * shape.decayRate) >> 15);
      if (level_ <= shape.sustainLevel + kEnvelopeSnapThreshold) {
        level_ = shape.sustainLevel;
        stage_ = Stage::Sustain;
      }
      break;
    }
    case Stage::Sustain:
      level_ = shape.sustainLevel;
      break;
    case Stage::Release: {
      level_ -= max<int32_t>(1, (level_ * shape.releaseRate) >> 15);
      if (level_ <= kReleaseFloor) {
        *this = Voice{};
        return 0;
      }
      break;
    }
    case Stage::Off:
      *this = Voice{};
      return 0;
  }

  // Exponential spectral modulation decay (brightness evolution).
  if (modDecayStep_ > 0) {
    modLevel_ -= max<int32_t>(1, (modLevel_ * modDecayStep_) >> 15);
    if (modLevel_ < kReleaseFloor) {
      modLevel_ = 0;
    }
  }
  if (percussionDecayStep_ > 0) {
    percussionLevel_ -= percussionDecayStep_;
    if (percussionLevel_ < 0) {
      percussionLevel_ = 0;
    }
  }
  // Key click: broadband noise burst for organ attack realism.
  if (clickLevel_ > 0) {
    clickLevel_ -= kOrganClickDecayPerSample;
    if (clickLevel_ < 0) {
      clickLevel_ = 0;
    }
  }

  uint32_t phaseStep = phaseStep_;

  // Built-in per-preset vibrato (independent of global effect toggle).
  if (vibratoStep_ > 0) {
    vibratoPhase_ += vibratoStep_;
    const int32_t vib = osc::sample(Waveform::Sine, vibratoPhase_);
    phaseStep += static_cast<uint32_t>((vib * preset.vibratoDepth) >> 6);
  }

  phaseStep += static_cast<uint32_t>(modulation.pitchOffset);

  // Multi-frame wavetable position: modLevel 32767=bright → framePos 0,
  // modLevel 0=dark → framePos 32767. Drives frame crossfading in sampleWave.
  const int32_t framePos = (preset.modDecayMs > 0) ? (32767 - modLevel_) : 0;

  int32_t sample = 0;
  if (preset.waveform == Waveform::FM) {
    // Pair 1 (body): carrier at base frequency, modulator at fmRatio.
    const int32_t fbMod = (static_cast<int32_t>(lastModOutput_) * preset.fmFeedback) >> 5;
    modPhase_ += (modPhaseStep_ + fbMod);
    int32_t modulator = osc::sample(Waveform::Sine, modPhase_);
    lastModOutput_ = modulator;
    const int32_t depth1 = (static_cast<int64_t>(modLevel_) * preset.fmDepth) >> 8;
    const uint32_t phaseMod1 = static_cast<uint32_t>((static_cast<int64_t>(modulator) * depth1) << 7);
    phase_ += phaseStep;
    sample = osc::sample(Waveform::Sine, phase_ + phaseMod1);

    // Pair 2 (richness): slightly detuned carrier, independent modulator.
    if (preset.fm2Level > 0) {
      fm2ModPhase_ += fm2ModPhaseStep_;
      const int32_t mod2 = osc::sample(Waveform::Sine, fm2ModPhase_);
      const int32_t depth2 = (static_cast<int64_t>(modLevel_) * preset.fm2Depth) >> 8;
      const uint32_t phaseMod2 = static_cast<uint32_t>((static_cast<int64_t>(mod2) * depth2) << 7);
      fm2CarrierPhase_ += fm2CarrierPhaseStep_;
      sample += (osc::sample(Waveform::Sine, fm2CarrierPhase_ + phaseMod2) *
                 static_cast<int32_t>(preset.fm2Level)) >> 8;
    }

    // Pair 3 (tine/bell): carrier at base freq, high-ratio modulator for metallic attack.
    if (preset.fm3Level > 0) {
      fm3ModPhase_ += fm3ModPhaseStep_;
      const int32_t mod3 = osc::sample(Waveform::Sine, fm3ModPhase_);
      const int32_t depth3 = (static_cast<int64_t>(modLevel_) * preset.fm3Depth) >> 8;
      const uint32_t phaseMod3 = static_cast<uint32_t>((static_cast<int64_t>(mod3) * depth3) << 7);
      fm3CarrierPhase_ += phaseStep;
      sample += (osc::sample(Waveform::Sine, fm3CarrierPhase_ + phaseMod3) *
                 static_cast<int32_t>(preset.fm3Level)) >> 8;
    }
  } else if (preset.unison) {
    // Unison: sum three oscillators (base, slightly detuned up, slightly detuned down)
    phase_ += phaseStep;
    const uint32_t detuneStep = (phaseStep >> 10) * preset.detune;
    unisonPhase_[0] += (phaseStep + detuneStep);
    unisonPhase_[1] += (phaseStep - detuneStep);

    sample = osc::sample(preset.waveform, phase_, preset.wavetableIndex, phaseStep, framePos);
    sample += osc::sample(preset.waveform, unisonPhase_[0], preset.wavetableIndex, phaseStep + detuneStep, framePos);
    sample += osc::sample(preset.waveform, unisonPhase_[1], preset.wavetableIndex, phaseStep - detuneStep, framePos);
    sample /= 3;
  } else {
    phase_ += phaseStep;
    sample = osc::sample(preset.waveform, phase_, preset.wavetableIndex, phaseStep, framePos);
  }

  // Organ key click: inject filtered noise burst at note onset.
  if (clickLevel_ > 0) {
      const int16_t noiseVal = static_cast<int16_t>(noise.next() >> 16);
    sample += (static_cast<int32_t>(noiseVal) * clickLevel_) >> 15;
  }

  if (preset.organ != nullptr && percussionLevel_ > 0) {
    percussionPhase_ += percussionPhaseStep_;
    const int32_t percussion = (static_cast<int32_t>(osc::sample(Waveform::Sine, percussionPhase_)) * percussionLevel_) >> 15;
    sample += percussion;
  }

  if (modulation.gainQ8 != 256) {
    sample = (sample * modulation.gainQ8) >> 8;
  }

  const int32_t envAmount = (level_ * shape.filterEnvDepth) >> 15;
  const int32_t spectralEnv = (static_cast<int64_t>(modLevel_) * preset.modulationDepth) >> 15;
  // Note-dependent brightness: higher notes get brighter filter cutoff (+1 per 4 semitones above C3).
  const int32_t noteBrightness = (static_cast<int32_t>(midiNote_) - 48) >> 2;
  const int32_t filterCutoff = constrain(shape.baseCutoff + (envAmount >> 1) + spectralEnv + noteBrightness, 10, 188);
  filterLow_ += (filterCutoff * filterBand_) >> 8;
  const int32_t filterHigh = sample - filterLow_ - ((filterBand_ * shape.resonanceDamping) >> 8);
  filterBand_ += (filterCutoff * filterHigh) >> 8;
  sample = filterLow_;

  return (sample * level_) >> 15;
}
