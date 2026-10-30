#include "KeyTracker.h"

void KeyTracker::press(uint8_t keyIndex, uint8_t midiNote) {
  held_[keyIndex] = true;
  sounding_[keyIndex] = true;
  notes_[keyIndex] = midiNote;
}

void KeyTracker::release(uint8_t keyIndex) {
  held_[keyIndex] = false;
}

bool KeyTracker::isHeld(uint8_t keyIndex) const {
  return held_[keyIndex];
}

bool KeyTracker::isSounding(uint8_t keyIndex) const {
  return sounding_[keyIndex];
}

uint8_t KeyTracker::soundingNote(uint8_t keyIndex) const {
  return notes_[keyIndex];
}

void KeyTracker::stopSounding(uint8_t keyIndex) {
  sounding_[keyIndex] = false;
}

void KeyTracker::reset() {
  memset(held_, 0, sizeof(held_));
  memset(sounding_, 0, sizeof(sounding_));
  memset(notes_, 0, sizeof(notes_));
}
