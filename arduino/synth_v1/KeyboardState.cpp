#include "KeyboardState.h"

void KeyboardState::setOctaveShift(int8_t octaveShift) {
  octaveShift_ = constrain(octaveShift, config::kMinOctaveShift, config::kMaxOctaveShift);
}

int8_t KeyboardState::octaveShift() const {
  return octaveShift_;
}

void KeyboardState::setTranspose(int8_t transpose) {
  transpose_ = constrain(transpose, config::kMinTranspose, config::kMaxTranspose);
}

int8_t KeyboardState::transpose() const {
  return transpose_;
}

uint8_t KeyboardState::midiForKeyIndex(uint8_t keyIndex) const {
  const int note = config::kBaseMidiNote + keyIndex + (octaveShift_ * 12) + transpose_;
  return static_cast<uint8_t>(constrain(note, 0, 127));
}

void KeyboardState::pressKey(uint8_t keyIndex) {
  keyHeld_[keyIndex] = true;
}

void KeyboardState::releaseKey(uint8_t keyIndex) {
  keyHeld_[keyIndex] = false;
}

bool KeyboardState::isHeld(uint8_t keyIndex) const {
  return keyHeld_[keyIndex];
}

void KeyboardState::setActiveMidiNote(uint8_t keyIndex, uint8_t midiNote) {
  activeMidiNotes_[keyIndex] = midiNote;
}

uint8_t KeyboardState::activeMidiNote(uint8_t keyIndex) const {
  return activeMidiNotes_[keyIndex];
}

void KeyboardState::setKeyActive(uint8_t keyIndex, bool active) {
  keyActive_[keyIndex] = active;
}

bool KeyboardState::isKeyActive(uint8_t keyIndex) const {
  return keyActive_[keyIndex];
}

void KeyboardState::resetTransientState() {
  memset(keyHeld_, 0, sizeof(keyHeld_));
  memset(keyActive_, 0, sizeof(keyActive_));
  memset(activeMidiNotes_, 0, sizeof(activeMidiNotes_));
}
