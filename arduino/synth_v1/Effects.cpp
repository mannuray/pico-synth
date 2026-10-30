#include "Effects.h"
#include "Dsp.h"

VoiceModulation VibratoEffect::voiceModulation(const EffectContext &ctx) const {
  VoiceModulation mod;
  mod.pitchOffset = (ctx.lfo * ctx.modulationDepth) >> 5;
  return mod;
}

VoiceModulation TremoloEffect::voiceModulation(const EffectContext &ctx) const {
  VoiceModulation mod;
  mod.gainQ8 = 180 + (ctx.lfo >> 9);
  return mod;
}

VoiceModulation ChorusEffect::voiceModulation(const EffectContext &ctx) const {
  VoiceModulation mod;
  mod.pitchOffset = (ctx.lfo * ctx.modulationDepth) >> 6;
  return mod;
}

bool ChorusEffect::process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &ctx) {
  const int16_t chorusLfoLeft = dsp::interpolateTable(ctx.sineTable, ctx.lfoPhase);
  const int16_t chorusLfoRight = dsp::interpolateTable(ctx.sineTable, ctx.lfoPhase + 0x40000000u);
  const size_t baseDelay = 132;
  const size_t sweep = 28 + (ctx.modulationDepth / 2);
  const size_t leftDelay = baseDelay + ((static_cast<int32_t>(chorusLfoLeft) + 32768) * static_cast<int32_t>(sweep) >> 16);
  const size_t rightDelay = baseDelay + ((static_cast<int32_t>(chorusLfoRight) + 32768) * static_cast<int32_t>(sweep) >> 16);
  const size_t leftTap = (index_ + kBufferSamples - (leftDelay & (kBufferSamples - 1))) & (kBufferSamples - 1);
  const size_t rightTap = (index_ + kBufferSamples - (rightDelay & (kBufferSamples - 1))) & (kBufferSamples - 1);
  const int32_t wetLeft = buffer_[leftTap];
  const int32_t wetRight = buffer_[rightTap];
  buffer_[index_] = dsp::softClip(mono);
  index_ = (index_ + 1) & (kBufferSamples - 1);
  left = (mono * 3 + wetLeft * 2) >> 2;
  right = (mono * 3 + wetRight * 2) >> 2;
  return true;
}

bool ReverbEffect::process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &) {
  const int32_t dry = mono;
  const int32_t tapL1 = bufferL1_[indexL1_];
  const int32_t tapL2 = bufferL2_[indexL2_];
  const int32_t tapR1 = bufferR1_[indexR1_];
  const int32_t tapR2 = bufferR2_[indexR2_];
  const int32_t wetLeft = tapL1 + (tapL2 >> 1) + (tapR1 >> 2);
  const int32_t wetRight = tapR1 + (tapR2 >> 1) + (tapL1 >> 2);

  bufferL1_[indexL1_] = dsp::softClip((dry + (tapL2 >> 1) + (tapR1 >> 2)) >> 1);
  bufferL2_[indexL2_] = dsp::softClip((dry + (tapL1 >> 1) + (tapR2 >> 3)) >> 1);
  bufferR1_[indexR1_] = dsp::softClip((dry + (tapR2 >> 1) + (tapL1 >> 2)) >> 1);
  bufferR2_[indexR2_] = dsp::softClip((dry + (tapR1 >> 1) + (tapL2 >> 3)) >> 1);

  if (++indexL1_ >= kShortSamples) indexL1_ = 0;
  if (++indexL2_ >= kLongSamples) indexL2_ = 0;
  if (++indexR1_ >= kMediumSamples) indexR1_ = 0;
  if (++indexR2_ >= kXLargeSamples) indexR2_ = 0;

  left = (dry * 3 + wetLeft) >> 2;
  right = (dry * 3 + wetRight) >> 2;
  return true;
}

bool DelayEffect::process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &) {
  const int16_t delayed = buffer_[index_];
  buffer_[index_] = static_cast<int16_t>((mono + (delayed >> 1)) >> 1);
  mono += delayed >> 1;
  index_ = (index_ + 1) & (kBufferSamples - 1);
  left = mono;
  right = mono;
  return true;
}

void BitcrusherEffect::onSelected() {
  held_ = 0;
  counter_ = 0;
}

bool BitcrusherEffect::process(int32_t &mono, int32_t &left, int32_t &right, const EffectContext &) {
  if (counter_++ >= 3) {
    held_ = (mono >> 12) << 12;
    counter_ = 0;
  }
  mono = held_;
  left = mono;
  right = mono;
  return true;
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
