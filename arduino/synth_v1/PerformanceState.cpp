#include "PerformanceState.h"
#include "RhythmPatterns.h"

void PerformanceState::setInstrumentBank(uint8_t bank) {
  instrumentBank_ = min<uint8_t>(config::kBankCount - 1, bank);
}

uint8_t PerformanceState::instrumentBank() const {
  return instrumentBank_;
}

void PerformanceState::setInstrumentSlot(uint8_t slot) {
  activePreset_ = instrumentBank_ * config::kSlotsPerBank + min<uint8_t>(config::kSlotsPerBank - 1, slot);
}

uint8_t PerformanceState::instrumentSlot() const {
  return activePreset_ % config::kSlotsPerBank;
}

uint8_t PerformanceState::activePresetIndex() const {
  return activePreset_;
}

void PerformanceState::setEffect(EffectType effect) {
  if (effect < EffectType::Count) {
    effect_ = effect;
  }
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

void PerformanceState::setSustainEnabled(bool enabled) {
  sustainEnabled_ = enabled;
}

bool PerformanceState::sustainEnabled() const {
  return sustainEnabled_;
}

void PerformanceState::setTempoBpm(int bpm) {
  tempoBpm_ = static_cast<uint8_t>(
      constrain(bpm, static_cast<int>(config::kTempoMinBpm), static_cast<int>(config::kTempoMaxBpm)));
}

uint8_t PerformanceState::tempoBpm() const {
  return tempoBpm_;
}

void PerformanceState::setRhythmPlaying(bool enabled) {
  rhythmPlaying_ = enabled;
}

bool PerformanceState::rhythmPlaying() const {
  return rhythmPlaying_;
}

void PerformanceState::setRhythmPattern(uint8_t pattern) {
  rhythmPattern_ = pattern % kRhythmPatternCount;
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

void PerformanceState::setSyncStartArmed(bool armed) {
  syncStartArmed_ = armed;
}

bool PerformanceState::syncStartArmed() const {
  return syncStartArmed_;
}

void PerformanceState::setRotaryFast(bool fast) {
  rotaryFast_ = fast;
}

bool PerformanceState::rotaryFast() const {
  return rotaryFast_;
}
