#pragma once

#include "MatrixMap.h"

#include <Arduino.h>

struct PerformanceCommand {
  ControlType control = ControlType::None;
  uint8_t value = 0;
};
