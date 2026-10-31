#pragma once

#include "Config.h"
#include "SynthTypes.h"

namespace presets {
// Preset N is tone button T(N+1); wraps at kPresetCount.
const Preset &at(uint8_t presetIndex);
}  // namespace presets
