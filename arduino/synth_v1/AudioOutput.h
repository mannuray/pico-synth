#pragma once

#include <Arduino.h>

class AudioOutput {
public:
  virtual ~AudioOutput() = default;

  virtual bool begin(uint32_t sampleRate, uint8_t bitsPerSample) = 0;
  virtual void writeStereo(int16_t left, int16_t right) = 0;
};
