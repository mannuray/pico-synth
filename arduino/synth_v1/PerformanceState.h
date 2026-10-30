#pragma once

#include "Config.h"
#include "SynthTypes.h"

#include <Arduino.h>

enum class EnvelopeParam : uint8_t {
  Attack = 0,
  Decay,
  Sustain,
  Release,
  Count
};

// Single source of truth for performance settings. Output sinks only mirror it.
class PerformanceState {
public:
  void setBank(uint8_t bank);
  uint8_t bank() const;

  void setInstrumentSlot(uint8_t slot);
  uint8_t instrumentSlot() const;

  uint8_t activePresetIndex() const;

  void setEffect(EffectType effect);
  EffectType effect() const;

  void setEffectEnabled(bool enabled);
  bool effectEnabled() const;

  void setSustainEnabled(bool enabled);
  bool sustainEnabled() const;

  void setTempoBpm(int bpm);
  uint8_t tempoBpm() const;

  void setRhythmPlaying(bool enabled);
  bool rhythmPlaying() const;

  void setRhythmPattern(uint8_t pattern);
  uint8_t rhythmPattern() const;

  void setMasterVolume(int volume);
  int16_t masterVolume() const;

  void setEnvelopeOffset(EnvelopeParam param, int8_t offset);
  int8_t envelopeOffset(EnvelopeParam param) const;
  void resetEnvelopeOffsets();

private:
  uint8_t bank_ = 0;
  uint8_t instrumentSlot_ = 0;
  EffectType effect_ = EffectType::Vibrato;
  bool effectEnabled_ = false;
  bool sustainEnabled_ = false;
  uint8_t tempoBpm_ = config::kMetronomeDefaultBpm;
  bool rhythmPlaying_ = false;
  uint8_t rhythmPattern_ = 0;
  int16_t masterVolume_ = config::kDefaultMasterVolume;
  int8_t envelopeOffsets_[static_cast<uint8_t>(EnvelopeParam::Count)] = {};
};
