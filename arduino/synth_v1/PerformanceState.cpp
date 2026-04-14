#include "PerformanceState.h"

void PerformanceState::setBank(uint8_t bank) {
  bank_ = min<uint8_t>(config::kBankCount - 1, bank);
}

uint8_t PerformanceState::bank() const {
  return bank_;
}

void PerformanceState::setInstrumentSlot(uint8_t slot) {
  instrumentSlot_ = min<uint8_t>(config::kInstrumentsPerBank - 1, slot);
}

uint8_t PerformanceState::instrumentSlot() const {
  return instrumentSlot_;
}

uint8_t PerformanceState::activePresetIndex() const {
  return bank_ * config::kInstrumentsPerBank + instrumentSlot_;
}

void PerformanceState::setEffect(EffectType effect) {
  effect_ = effect;
}

EffectType PerformanceState::effect() const {
  return effect_;
}

void PerformanceState::setEffectEnabled(bool enabled) {
  effectEnabled_ = enabled;
}

bool PerformanceState::effectEnabled() const {
  return effectEnabled_;
}

void PerformanceState::setMetronomeEnabled(bool enabled) {
  metronomeEnabled_ = enabled;
}

bool PerformanceState::metronomeEnabled() const {
  return metronomeEnabled_;
}

void PerformanceState::setMetronomeBpm(int bpm) {
  metronomeBpm_ = static_cast<uint8_t>(
      constrain(bpm, static_cast<int>(config::kMetronomeMinBpm), static_cast<int>(config::kMetronomeMaxBpm)));
}

uint8_t PerformanceState::metronomeBpm() const {
  return metronomeBpm_;
}

void PerformanceState::setRhythmPlaying(bool enabled) {
  rhythmPlaying_ = enabled;
}

bool PerformanceState::rhythmPlaying() const {
  return rhythmPlaying_;
}

void PerformanceState::setRhythmPattern(uint8_t pattern) {
  rhythmPattern_ = pattern % 8;
}

uint8_t PerformanceState::rhythmPattern() const {
  return rhythmPattern_;
}

void PerformanceState::setMasterVolume(int volume) {
  masterVolume_ = static_cast<int16_t>(
      constrain(volume, static_cast<int>(config::kMinMasterVolume), static_cast<int>(config::kMaxMasterVolume)));
}

int16_t PerformanceState::masterVolume() const {
  return masterVolume_;
}
