#include "RotarySpeaker.h"
#include "Oscillator.h"

void RotarySpeaker::render(const OrganTraits &organ, const Preset &preset, const Voice &source,
                           int32_t mix, int16_t &outL, int16_t &outR) const {
  const int16_t rotaryL = osc::sine(phase_);
  const int16_t rotaryR = osc::sine(phase_ + 0x40000000u);
  const int32_t ampL = 224 + (rotaryL >> 10);
  const int32_t ampR = 224 + (rotaryR >> 10);
  const int32_t phaseOffset = rotaryL >> organ.rotaryDepthShift;
  const int32_t stereoOffset = rotaryR >> organ.rotaryDepthShift;
  const int32_t rotaryWet = organ.rotaryWet;
  const uint32_t basePhase = source.phase();
  const int32_t leftSample = osc::sample(preset.waveform, basePhase + static_cast<uint32_t>(phaseOffset), preset.wavetableIndex);
  const int32_t rightSample = osc::sample(preset.waveform, basePhase + static_cast<uint32_t>(stereoOffset), preset.wavetableIndex);
  const int32_t leftMix = (mix * ampL) >> 8;
  const int32_t rightMix = (mix * ampR) >> 8;
  outL = dsp::softClip((leftMix * (256 - rotaryWet) + leftSample * rotaryWet) >> 8);
  outR = dsp::softClip((rightMix * (256 - rotaryWet) + rightSample * rotaryWet) >> 8);
}
