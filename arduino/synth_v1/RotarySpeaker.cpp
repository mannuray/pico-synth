#include "RotarySpeaker.h"
#include "AudioHot.h"
#include "Oscillator.h"

namespace {
constexpr int32_t kBaseDelayQ8 = 24 << 8;  // ~0.5 ms, leaves room for the Doppler swing
}

void RotarySpeaker::configure(const OrganTraits *organ) {
  const bool on = organ != nullptr && organ->rotarySlowHzX10 > 0;
  slowStep_ = on ? osc::hzToPhaseStep(organ->rotarySlowHzX10 / 10.0f) : 0;
  fastStep_ = on ? osc::hzToPhaseStep(organ->rotaryFastHzX10 / 10.0f) : 0;
  depth_ = on ? organ->rotaryDepth : 0;
  step_ = fast_ ? fastStep_ : slowStep_;
}

void RotarySpeaker::setFast(bool fast) {
  fast_ = fast;
}

int32_t AUDIO_HOT(RotarySpeaker::readDelayed)(int32_t delayQ8) const {
  const size_t whole = static_cast<size_t>(delayQ8 >> 8);
  const int32_t frac = delayQ8 & 0xFF;
  const int32_t a = buffer_[(index_ + kBufferSamples - whole) & (kBufferSamples - 1)];
  const int32_t b = buffer_[(index_ + kBufferSamples - whole - 1) & (kBufferSamples - 1)];
  return a + (((b - a) * frac) >> 8);
}

void AUDIO_HOT(RotarySpeaker::process)(int32_t &left, int32_t &right) {
  buffer_[index_] = static_cast<int16_t>(constrain((left + right) >> 1, -32768, 32767));

  // Ramp toward the target speed: time constant ~0.75 s up, ~1.5 s down.
  const int32_t target = static_cast<int32_t>(fast_ ? fastStep_ : slowStep_);
  const int32_t diff = target - static_cast<int32_t>(step_);
  if (diff != 0) {
    const int32_t change = diff > 0 ? (diff >> 15) : -((-diff) >> 16);
    step_ = static_cast<uint32_t>(static_cast<int32_t>(step_) + (change != 0 ? change : diff));
  }
  phase_ += step_;
  const int32_t toward = osc::sine(phase_);                 // horn velocity toward the listener
  const int32_t facing = osc::sine(phase_ + 0x40000000u);   // horn pointing at the listener

  // Doppler: up to ~+-8 samples of delay swing at full depth.
  const int32_t swingQ8 = (toward * depth_) >> 12;
  const int32_t tapLeft = readDelayed(kBaseDelayQ8 + swingQ8);
  const int32_t tapRight = readDelayed(kBaseDelayQ8 - swingQ8);

  // Amplitude: up to +-50% at full depth.
  const int32_t am = (facing * depth_) >> 16;
  left = (tapLeft * (256 + am)) >> 8;
  right = (tapRight * (256 - am)) >> 8;

  index_ = (index_ + 1) & (kBufferSamples - 1);
}
