#include "I2SAudioOutput.h"

bool I2SAudioOutput::begin(uint32_t sampleRate, uint8_t bitsPerSample) {
  i2s_.setBitsPerSample(bitsPerSample);
  i2s_.setBuffers(config::kI2SBufferCount, config::kI2SBufferFrames);  // one 32-bit word per stereo frame
  return i2s_.begin(sampleRate);
}

void I2SAudioOutput::writeStereo(int16_t left, int16_t right) {
  i2s_.write16(left, right);
}
