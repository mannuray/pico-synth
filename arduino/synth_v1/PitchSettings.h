#pragma once

#include "Config.h"

#include <Arduino.h>

// Octave and transpose applied when a key becomes a MIDI note.
class PitchSettings {
public:
  void setOctaveShift(int8_t octaveShift);
  int8_t octaveShift() const;

  void setTranspose(int8_t transpose);
  int8_t transpose() const;

  uint8_t midiNoteForKey(uint8_t keyIndex) const;

private:
  int8_t octaveShift_ = 0;
  int8_t transpose_ = 0;
};
