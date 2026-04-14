#include "MicInput.h"

void MicInput::begin() {
  analogReadResolution(12);
  pinMode(config::kMicAdcPin, INPUT);
}

uint16_t MicInput::readLevel() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    sum += analogRead(config::kMicAdcPin);
  }

  const uint16_t raw = static_cast<uint16_t>(sum / 8u);
  const uint16_t centered = (raw > 2048u) ? (raw - 2048u) : (2048u - raw);

  smoothedLevel_ += centered;
  smoothedLevel_ -= (smoothedLevel_ >> 4);
  return static_cast<uint16_t>(smoothedLevel_ >> 4);
}
