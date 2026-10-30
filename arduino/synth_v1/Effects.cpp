#include "Effects.h"
#include "Dsp.h"

namespace {
int16_t saturate16(int32_t x) {
  return static_cast<int16_t>(constrain(x, -32768, 32767));
}

// Vibrato: +-765/65536 = +-1.17% = about +-20 cents. Chorus: about +-3 cents.
constexpr int32_t kVibratoPitchQ16 = 765;
constexpr int32_t kChorusPitchQ16 = 115;

constexpr size_t kChorusBaseDelay = 400;   // ~9 ms
constexpr size_t kChorusSweep = 88;        // +-1 ms

constexpr int32_t kReverbFeedback = 27525; // 0.84: room size
constexpr int32_t kReverbDamp = 8192;      // 0.25: high-frequency damping in the feedback path
constexpr int32_t kReverbWet = 9830;       // 0.30

constexpr int32_t kDelayFeedback = 14746;  // 0.45
constexpr int32_t kDelayMix = 13107;       // 0.40
}  // namespace

VoiceModulation VibratoEffect::voiceModulation(const EffectContext &ctx) const {
  VoiceModulation mod;
  mod.pitchFracQ16 = (ctx.lfo * kVibratoPitchQ16) >> 15;
  return mod;
}

VoiceModulation TremoloEffect::voiceModulation(const EffectContext &ctx) const {
  VoiceModulation mod;
  mod.gainQ8 = 205 + ((ctx.lfo * 51) >> 15);  // ~154..256, about +-2.5 dB
  return mod;
}

VoiceModulation ChorusEffect::voiceModulation(const EffectContext &ctx) const {
  VoiceModulation mod;
  mod.pitchFracQ16 = (ctx.lfo * kChorusPitchQ16) >> 15;
  return mod;
}

void ChorusEffect::onSelected() {
  memset(buffer_, 0, sizeof(buffer_));
  index_ = 0;
}

void ChorusEffect::process(int32_t &left, int32_t &right, const EffectContext &ctx) {
  const int32_t lfoLeft = dsp::interpolateTable(ctx.sineTable, ctx.lfoPhase);
  const int32_t lfoRight = dsp::interpolateTable(ctx.sineTable, ctx.lfoPhase + 0x40000000u);
  const size_t leftDelay = kChorusBaseDelay + (((lfoLeft + 32768) * static_cast<int32_t>(kChorusSweep)) >> 16);
  const size_t rightDelay = kChorusBaseDelay + (((lfoRight + 32768) * static_cast<int32_t>(kChorusSweep)) >> 16);
  const int32_t wetLeft = buffer_[(index_ + kBufferSamples - leftDelay) & (kBufferSamples - 1)];
  const int32_t wetRight = buffer_[(index_ + kBufferSamples - rightDelay) & (kBufferSamples - 1)];
  buffer_[index_] = saturate16((left + right) >> 1);
  index_ = (index_ + 1) & (kBufferSamples - 1);
  left = (left * 3 + wetLeft * 2) >> 2;
  right = (right * 3 + wetRight * 2) >> 2;
}

void ReverbEffect::initialize() {
  for (uint8_t side = 0; side < 2; ++side) {
    const uint16_t spread = side == 0 ? 0 : kStereoSpread;
    int16_t *comb = combMemory_[side];
    for (uint8_t i = 0; i < 4; ++i) {
      combs_[side][i] = Comb{comb, static_cast<uint16_t>(kCombLengths[i] + spread), 0, 0};
      comb += kCombLengths[i] + spread;
    }
    int16_t *allpass = allpassMemory_[side];
    for (uint8_t i = 0; i < 2; ++i) {
      allpasses_[side][i] = Allpass{allpass, static_cast<uint16_t>(kAllpassLengths[i] + spread), 0};
      allpass += kAllpassLengths[i] + spread;
    }
  }
  initialized_ = true;
}

void ReverbEffect::onSelected() {
  memset(combMemory_, 0, sizeof(combMemory_));
  memset(allpassMemory_, 0, sizeof(allpassMemory_));
  initialize();
}

int32_t ReverbEffect::processSide(uint8_t side, int32_t input) {
  int32_t out = 0;
  for (Comb &comb : combs_[side]) {
    const int32_t delayed = comb.buffer[comb.index];
    comb.filterState = (delayed * (32768 - kReverbDamp) + comb.filterState * kReverbDamp) >> 15;
    comb.buffer[comb.index] = saturate16(input + ((comb.filterState * kReverbFeedback) >> 15));
    if (++comb.index >= comb.length) comb.index = 0;
    out += delayed;
  }
  for (Allpass &allpass : allpasses_[side]) {
    const int32_t delayed = allpass.buffer[allpass.index];
    allpass.buffer[allpass.index] = saturate16(out + (delayed >> 1));
    out = delayed - out;
    if (++allpass.index >= allpass.length) allpass.index = 0;
  }
  return out;
}

void ReverbEffect::process(int32_t &left, int32_t &right, const EffectContext &) {
  if (!initialized_) {
    initialize();
  }
  const int32_t input = (left + right) >> 4;  // mono, scaled for the four summed combs
  const int32_t wetLeft = processSide(0, input);
  const int32_t wetRight = processSide(1, input);
  left += (wetLeft * kReverbWet) >> 15;
  right += (wetRight * kReverbWet) >> 15;
}

void DelayEffect::onSelected() {
  memset(buffer_, 0, sizeof(buffer_));
  index_ = 0;
}

void DelayEffect::process(int32_t &left, int32_t &right, const EffectContext &) {
  const int32_t delayed = buffer_[index_];
  const int32_t mono = (left + right) >> 1;
  buffer_[index_] = saturate16(mono + ((delayed * kDelayFeedback) >> 15));
  index_ = (index_ + 1) & (kBufferSamples - 1);
  const int32_t echo = (delayed * kDelayMix) >> 15;
  left += echo;
  right += echo;
}

void BitcrusherEffect::onSelected() {
  heldLeft_ = 0;
  heldRight_ = 0;
  counter_ = 0;
}

void BitcrusherEffect::process(int32_t &left, int32_t &right, const EffectContext &) {
  if (counter_++ >= 3) {
    // Keep the top 4 bits of a 16-bit sample (mask, since left-shifting negatives is undefined).
    heldLeft_ = left & ~0xFFF;
    heldRight_ = right & ~0xFFF;
    counter_ = 0;
  }
  left = heldLeft_;
  right = heldRight_;
}

void EffectRack::select(EffectType effect) {
  if (effect >= EffectType::Count) {
    return;
  }
  selected_ = effect;
  effects_[static_cast<uint8_t>(selected_)]->onSelected();
  updateActive();
}

void EffectRack::setEnabled(bool enabled) {
  enabled_ = enabled;
  updateActive();
}

void EffectRack::updateActive() {
  active_ = enabled_ ? effects_[static_cast<uint8_t>(selected_)] : &bypass_;
}
