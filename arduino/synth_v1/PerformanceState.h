#pragma once

#include "Config.h"
#include "SynthTypes.h"

#include <Arduino.h>

// Single source of truth for performance settings. Output sinks only mirror it.
class PerformanceState {
public:
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

  // Rhythm armed to start with the first key played.
  void setSyncStartArmed(bool armed);
  bool syncStartArmed() const;

  // Organ rotary speaker: fast (tremolo) or slow (chorale).
  void setRotaryFast(bool fast);
  bool rotaryFast() const;

private:
  uint8_t instrumentSlot_ = 0;
  EffectType effect_ = EffectType::Vibrato;
  bool effectEnabled_ = false;
  bool sustainEnabled_ = false;
  uint8_t tempoBpm_ = config::kTempoDefaultBpm;
  bool rhythmPlaying_ = false;
  uint8_t rhythmPattern_ = 0;
  int16_t masterVolume_ = config::kDefaultMasterVolume;
  bool syncStartArmed_ = false;
  bool rotaryFast_ = false;
};
