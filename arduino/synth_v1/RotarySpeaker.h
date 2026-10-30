#pragma once

#include "SynthTypes.h"

// Leslie-style rotary speaker for the organ presets. The rotating horn is
// modelled as a short modulated delay (Doppler pitch wobble) plus amplitude
// modulation, with the two output sides 180 degrees apart like two mics on
// opposite sides of the cabinet.
class RotarySpeaker {
public:
  // nullptr, or traits with no rotary speed, disables it.
  void configure(const OrganTraits *organ);
  bool enabled() const { return step_ != 0; }

  // Processes one stereo sample in place.
  void process(int32_t &left, int32_t &right);

private:
  static constexpr size_t kBufferSamples = 64;
  int16_t buffer_[kBufferSamples] = {};
  size_t index_ = 0;
  uint32_t phase_ = 0;
  uint32_t step_ = 0;
  int32_t depth_ = 0;  // 0..255

  int32_t readDelayed(int32_t delayQ8) const;
};
