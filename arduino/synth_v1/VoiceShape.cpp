#include "VoiceShape.h"
#include "Config.h"

#include <math.h>

namespace {
uint32_t applyOffset(uint32_t base, int8_t offset) {
  if (offset == 0) return base;
  if (offset > 0) return min<uint32_t>(20000, base << offset);
  return max<uint32_t>(1, base >> (-offset));
}
}  // namespace

VoiceShape computeVoiceShape(const Preset &preset, const EnvelopeOffsets &offsets) {
  const uint32_t attackMs = applyOffset(max<uint32_t>(1, preset.attackMs), offsets.attack);
  const uint32_t decayMs = applyOffset(max<uint32_t>(1, preset.decayMs), offsets.decay);
  const uint32_t decay2Ms = preset.decay2Ms == 0 ? decayMs : applyOffset(preset.decay2Ms, offsets.decay);
  const uint32_t releaseMs = applyOffset(max<uint32_t>(1, preset.releaseMs), offsets.release);
  const int32_t sustain255 = (offsets.sustain == 0) ? static_cast<int32_t>(preset.sustainLevel) :
      constrain(static_cast<int32_t>(preset.sustainLevel) + (offsets.sustain * 32), 0, 255);

  VoiceShape shape;
  shape.attackRate = msToEnvRate(attackMs);
  shape.decayRate = msToEnvRate(decayMs);
  shape.decay2Rate = msToEnvRate(decay2Ms);
  shape.releaseRate = msToEnvRate(releaseMs);
  shape.sustainLevel = sustain255 == 255 ? kEnvelopeFull : sustain255 << 22;
  shape.filterBase = static_cast<uint8_t>(constrain(12 + (preset.brightness / 4), 12, 180));
  shape.filterEnvDepth = static_cast<uint8_t>(constrain(16 + (preset.brightness / 2), 16, 180));
  shape.baseCutoff = constrain(static_cast<int32_t>(shape.filterBase) + (preset.brightness / 5), 10, 200);
  shape.resonanceDamping = constrain(180 - (preset.resonance / 2), 28, 180);
  return shape;
}

uint32_t msToEnvRate(uint32_t ms) {
  const float samples = max(1.0f, static_cast<float>(ms) * config::kSampleRate / 1000.0f);
  const float rate = 1048576.0f * (1.0f - expf(-4.6f / samples));
  return static_cast<uint32_t>(constrain(rate, 1.0f, 1048576.0f));
}
