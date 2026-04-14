#pragma once

#include "Config.h"

#include <Arduino.h>

class KeyboardState {
public:
  void setOctaveShift(int8_t octaveShift);
  int8_t octaveShift() const;

  void setTranspose(int8_t transpose);
  int8_t transpose() const;

  uint8_t midiForKeyIndex(uint8_t keyIndex) const;

  void pressKey(uint8_t keyIndex);
  void releaseKey(uint8_t keyIndex);
  bool isHeld(uint8_t keyIndex) const;

  void setActiveMidiNote(uint8_t keyIndex, uint8_t midiNote);
  uint8_t activeMidiNote(uint8_t keyIndex) const;

  void setKeyActive(uint8_t keyIndex, bool active);
  bool isKeyActive(uint8_t keyIndex) const;

  void resetTransientState();

private:
  static constexpr uint8_t kKeyCount = 37;

  int8_t octaveShift_ = 0;
  int8_t transpose_ = 0;
  bool keyHeld_[kKeyCount] = {};
  bool keyActive_[kKeyCount] = {};
  uint8_t activeMidiNotes_[kKeyCount] = {};
};
