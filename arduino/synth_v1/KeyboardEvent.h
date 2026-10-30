#pragma once

#include <Arduino.h>

enum class KeyboardEventType : uint8_t {
  NoteOn = 0,
  NoteOff
};

struct KeyboardEvent {
  KeyboardEventType type;
  uint8_t note = 0;
};
