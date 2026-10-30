#pragma once

#include <Arduino.h>

// Tracks which keys are physically held and which are still sounding
// (a released key keeps sounding while sustain is on). Remembers the note
// each key started, so later octave/transpose changes release the right note.
class KeyTracker {
public:
  static constexpr uint8_t kKeyCount = 37;

  void press(uint8_t keyIndex, uint8_t midiNote);
  void release(uint8_t keyIndex);
  bool isHeld(uint8_t keyIndex) const;

  bool isSounding(uint8_t keyIndex) const;
  uint8_t soundingNote(uint8_t keyIndex) const;
  void stopSounding(uint8_t keyIndex);

  void reset();

private:
  bool held_[kKeyCount] = {};
  bool sounding_[kKeyCount] = {};
  uint8_t notes_[kKeyCount] = {};
};
