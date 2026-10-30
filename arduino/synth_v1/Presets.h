#pragma once

#include "Config.h"
#include "SynthTypes.h"

namespace presets {
// Preset index = bank * config::kInstrumentsPerBank + slot; wraps at kPresetCount.
const Preset &at(uint8_t presetIndex);
}  // namespace presets
