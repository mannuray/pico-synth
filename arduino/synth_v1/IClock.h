#pragma once

#include <Arduino.h>

class IClock {
public:
  virtual ~IClock() = default;
  virtual uint32_t nowMs() const = 0;
  virtual uint32_t nowUs() const = 0;
};
