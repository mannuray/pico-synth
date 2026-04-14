#pragma once

#include "Config.h"
#include "SynthEngine.h"

#include <Arduino.h>

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

  void setMetronomeEnabled(bool enabled);
  bool metronomeEnabled() const;

  void setMetronomeBpm(int bpm);
  uint8_t metronomeBpm() const;

  void setRhythmPlaying(bool enabled);
  bool rhythmPlaying() const;

  void setRhythmPattern(uint8_t pattern);
  uint8_t rhythmPattern() const;

  void setMasterVolume(int volume);
  int16_t masterVolume() const;

private:
  uint8_t bank_ = 0;
  uint8_t instrumentSlot_ = 0;
  EffectType effect_ = EffectType::Vibrato;
  bool effectEnabled_ = false;
  bool metronomeEnabled_ = false;
  uint8_t metronomeBpm_ = config::kMetronomeDefaultBpm;
  bool rhythmPlaying_ = false;
  uint8_t rhythmPattern_ = 0;
  int16_t masterVolume_ = config::kDefaultMasterVolume;
};
