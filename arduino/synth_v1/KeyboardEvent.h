#pragma once

#include "MatrixMap.h"

#include <Arduino.h>

enum class KeyboardEventType : uint8_t {
  NoteOn = 0,
  NoteOff,
  Control
};

struct KeyboardEvent {
  KeyboardEventType type;
  uint8_t note = 0;
  ControlType control = ControlType::None;
  uint8_t value = 0;
};

constexpr uint8_t kControlValueOff = 0;
constexpr uint8_t kControlValueOn = 1;
