#pragma once

#include "SynthTypes.h"

// Per-frame inputs shared by all effects.
struct EffectContext {
  uint32_t lfoPhase;        // shared LFO phase (advances only while the active effect uses it)
  int32_t lfo;              // triangle LFO value at lfoPhase
  uint8_t modulationDepth;  // active preset's modulation depth
  const int16_t *sineTable; // 512-sample sine table
};

// Modulation applied to every voice, computed once per frame.
struct VoiceModulation {
  int32_t pitchOffset = 0;  // added to each voice's phase step
  int32_t gainQ8 = 256;     // voice amplitude (256 = unity)
};

// An effect has two hooks: per-voice modulation (before the mix) and
// processing of the master mix. Each effect overrides only what it needs.
class AudioEffect {
public:
  virtual ~AudioEffect() = default;

  virtual bool usesLfo() const { return false; }
  virtual void onSelected() {}
  virtual VoiceModulation voiceModulation(const EffectContext &) const { return {}; }

  // Processes the post-volume mono mix. Returns true when the effect wrote its
  // own stereo output to left/right; false keeps the dry stereo mix.
  virtual bool process(int32_t &, int32_t &, int32_t &, const EffectContext &) { return false; }
};

class BypassEffect final : public AudioEffect {};

class VibratoEffect final : public AudioEffect {
public:
  bool usesLfo() const override { return true; }
  VoiceModulation voiceModulation(const EffectContext &ctx) const override;
};

class TremoloEffect final : public AudioEffect {
public:
  bool usesLfo() const override { return true; }
  VoiceModulation voiceModulation(const EffectContext &ctx) const override;
};

class ChorusEffect final : public AudioEffect {
public:
  bool usesLfo() const override { return true; }
  VoiceModulation voiceModulation(const EffectContext &ctx) const override;
  bool process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  static constexpr size_t kBufferSamples = 512;
  int16_t buffer_[kBufferSamples] = {};
  size_t index_ = 0;
};

class ReverbEffect final : public AudioEffect {
public:
  bool process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  static constexpr size_t kShortSamples = 389;
  static constexpr size_t kMediumSamples = 617;
  static constexpr size_t kLongSamples = 863;
  static constexpr size_t kXLargeSamples = 1103;
  int16_t bufferL1_[kShortSamples] = {};
  int16_t bufferL2_[kLongSamples] = {};
  int16_t bufferR1_[kMediumSamples] = {};
  int16_t bufferR2_[kXLargeSamples] = {};
  size_t indexL1_ = 0;
  size_t indexL2_ = 0;
  size_t indexR1_ = 0;
  size_t indexR2_ = 0;
};

class DelayEffect final : public AudioEffect {
public:
  bool process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  static constexpr size_t kBufferSamples = 1024;
  int16_t buffer_[kBufferSamples] = {};
  size_t index_ = 0;
};

class BitcrusherEffect final : public AudioEffect {
public:
  void onSelected() override;
  bool process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  int32_t held_ = 0;
  uint8_t counter_ = 0;
};

// Owns one instance of every effect and exposes the active one.
// Adding an effect: write the class, add an EffectType entry and a slot in effects_.
class EffectRack {
public:
  void select(EffectType effect);
  void setEnabled(bool enabled);
  AudioEffect &active() { return *active_; }

private:
  BypassEffect bypass_;
  VibratoEffect vibrato_;
  TremoloEffect tremolo_;
  ChorusEffect chorus_;
  ReverbEffect reverb_;
  DelayEffect delay_;
  BitcrusherEffect bitcrusher_;

  // Indexed by EffectType.
  AudioEffect *const effects_[static_cast<uint8_t>(EffectType::Count)] = {
      &vibrato_, &tremolo_, &chorus_, &reverb_, &delay_, &bitcrusher_};

  EffectType selected_ = EffectType::Vibrato;
  bool enabled_ = false;
  AudioEffect *active_ = &bypass_;

  void updateActive();
};
