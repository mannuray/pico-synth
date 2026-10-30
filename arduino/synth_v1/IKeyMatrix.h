#pragma once

#include <Arduino.h>

struct KeyMatrixEvent {
  uint8_t row;
  uint8_t col;
  bool pressed;
};

class IKeyMatrix {
public:
  virtual ~IKeyMatrix() = default;
  // Returns true and fills `event` while debounced key changes are pending.
  virtual bool poll(KeyMatrixEvent &event) = 0;
};
