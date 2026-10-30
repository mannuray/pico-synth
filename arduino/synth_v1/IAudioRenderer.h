#pragma once

#include <Arduino.h>

class IAudioRenderer {
public:
  virtual ~IAudioRenderer() = default;
  virtual void renderFrames(size_t frameCount) = 0;
  virtual uint32_t lastProcessingUs() const = 0;
};
