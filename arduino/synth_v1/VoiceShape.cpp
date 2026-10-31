#include "VoiceShape.h"
#include "Config.h"

#include <math.h>

VoiceShape computeVoiceShape(const Preset &preset) {
  const uint32_t attackMs = max<uint32_t>(1, preset.attackMs);
  const uint32_t decayMs = max<uint32_t>(1, preset.decayMs);
  const uint32_t decay2Ms = preset.decay2Ms == 0 ? decayMs : preset.decay2Ms;
  const uint32_t releaseMs = max<uint32_t>(1, preset.releaseMs);
  const int32_t sustain255 = preset.sustainLevel;

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
