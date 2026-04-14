#pragma once

#include "AudioOutput.h"
#include "Config.h"

#include <I2S.h>

class I2SAudioOutput final : public AudioOutput {
public:
  bool begin(uint32_t sampleRate, uint8_t bitsPerSample) override;
  void writeStereo(int16_t left, int16_t right) override;

private:
  I2S i2s_{OUTPUT, config::kI2SBclkPin, config::kI2SDataPin};
};
