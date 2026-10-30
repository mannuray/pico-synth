#pragma once

#include "SynthTypes.h"
#include "Voice.h"

// Leslie-style rotary speaker for the organ presets: a rotor spinning at the
// preset's speed swings volume left/right and adds a Doppler-like phase wobble.
class RotarySpeaker {
public:
  // Call once per sample while an organ preset is active.
  void advance(const OrganTraits &organ) { phase_ += organ.rotaryStep; }

  // Turns the mono mix into stereo, blending in `source` re-sampled at the
  // rotor's phase offsets.
  void render(const OrganTraits &organ, const Preset &preset, const Voice &source,
              int32_t mix, int16_t &outL, int16_t &outR) const;

private:
  uint32_t phase_ = 0;
};
