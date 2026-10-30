#pragma once

#include "Config.h"
#include "ILevelInput.h"

class MicInput final : public ILevelInput {
public:
  void begin();
  uint16_t readLevel() override;

private:
  uint32_t smoothedLevel_ = 0;
};
