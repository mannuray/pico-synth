#include "PitchSettings.h"

void PitchSettings::setOctaveShift(int8_t octaveShift) {
  octaveShift_ = constrain(octaveShift, config::kMinOctaveShift, config::kMaxOctaveShift);
}

int8_t PitchSettings::octaveShift() const {
  return octaveShift_;
}

void PitchSettings::setTranspose(int8_t transpose) {
  transpose_ = constrain(transpose, config::kMinTranspose, config::kMaxTranspose);
}

int8_t PitchSettings::transpose() const {
  return transpose_;
}

uint8_t PitchSettings::midiNoteForKey(uint8_t keyIndex) const {
  const int note = config::kBaseMidiNote + keyIndex + (octaveShift_ * 12) + transpose_;
  return static_cast<uint8_t>(constrain(note, 0, 127));
}
