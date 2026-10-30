#pragma once

#include <Arduino.h>

class ILevelInput {
public:
  virtual ~ILevelInput() = default;
  virtual uint16_t readLevel() = 0;
};
