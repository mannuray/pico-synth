#pragma once

#include "Config.h"
#include "SynthTypes.h"

namespace presets {
// Preset N is tone button T(N % 8 + 1) in bank N / 8 + 1; wraps at kPresetCount.
const Preset &at(uint8_t presetIndex);
}  // namespace presets
