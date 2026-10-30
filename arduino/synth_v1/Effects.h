#pragma once

#include "EffectTypes.h"
#include "SynthTypes.h"

// An effect has two hooks: per-voice modulation (before the mix) and
// in-place processing of the stereo master mix. Each effect overrides only what it needs.
class AudioEffect {
public:
  virtual ~AudioEffect() = default;

  virtual bool usesLfo() const { return false; }
  virtual void onSelected() {}
  virtual VoiceModulation voiceModulation(const EffectContext &) const { return {}; }
  virtual void process(int32_t &, int32_t &, const EffectContext &) {}
};

class BypassEffect final : public AudioEffect {};

// Pitch wobble, about +-20 cents at the shared 5 Hz LFO.
class VibratoEffect final : public AudioEffect {
public:
  bool usesLfo() const override { return true; }
  VoiceModulation voiceModulation(const EffectContext &ctx) const override;
};

// Volume wobble at the shared LFO.
class TremoloEffect final : public AudioEffect {
public:
  bool usesLfo() const override { return true; }
  VoiceModulation voiceModulation(const EffectContext &ctx) const override;
};

// Two modulated delay taps (one per side) blended with the dry signal.
class ChorusEffect final : public AudioEffect {
public:
  bool usesLfo() const override { return true; }
  void onSelected() override;
  VoiceModulation voiceModulation(const EffectContext &ctx) const override;
  void process(int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  static constexpr size_t kBufferSamples = 512;
  int16_t buffer_[kBufferSamples] = {};
  size_t index_ = 0;
};

// Freeverb-style room: four damped feedback combs and two allpass diffusers
// per side, the right side slightly longer for stereo width.
class ReverbEffect final : public AudioEffect {
public:
  void onSelected() override;
  void process(int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  struct Comb {
    int16_t *buffer;
    uint16_t length;
    uint16_t index;
    int32_t filterState;
  };
  struct Allpass {
    int16_t *buffer;
    uint16_t length;
    uint16_t index;
  };

  static constexpr uint16_t kCombLengths[4] = {1116, 1188, 1277, 1356};
  static constexpr uint16_t kAllpassLengths[2] = {556, 441};
  static constexpr uint16_t kStereoSpread = 23;
  static constexpr size_t kCombSamples = 1116 + 1188 + 1277 + 1356 + 4 * kStereoSpread;
  static constexpr size_t kAllpassSamples = 556 + 441 + 2 * kStereoSpread;

  int16_t combMemory_[2][kCombSamples] = {};
  int16_t allpassMemory_[2][kAllpassSamples] = {};
  Comb combs_[2][4] = {};
  Allpass allpasses_[2][2] = {};
  bool initialized_ = false;

  void initialize();
  int32_t processSide(uint8_t side, int32_t input);
};

// Echo at ~186 ms with feedback, mixed equally into both sides.
class DelayEffect final : public AudioEffect {
public:
  void onSelected() override;
  void process(int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  static constexpr size_t kBufferSamples = 8192;
  int16_t buffer_[kBufferSamples] = {};
  size_t index_ = 0;
};

// Lo-fi: holds every 4th sample and keeps 4 bits of resolution.
class BitcrusherEffect final : public AudioEffect {
public:
  void onSelected() override;
  void process(int32_t &left, int32_t &right, const EffectContext &ctx) override;

private:
  int32_t heldLeft_ = 0;
  int32_t heldRight_ = 0;
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
