#pragma once

#include <Arduino.h>

class IStatusIndicator {
public:
  virtual ~IStatusIndicator() = default;
  // Brief flash, e.g. on a note-on.
  virtual void flash(uint32_t nowMs) = 0;
  virtual void update(uint32_t nowMs) = 0;
};
