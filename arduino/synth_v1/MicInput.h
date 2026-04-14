#pragma once

#include "Config.h"

class MicInput {
public:
  void begin();
  uint16_t readLevel();

private:
  uint32_t smoothedLevel_ = 0;
};
