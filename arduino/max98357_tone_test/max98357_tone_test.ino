#include <Arduino.h>
#include <I2S.h>

namespace {
constexpr pin_size_t kBclkPin = 18;
constexpr pin_size_t kDataPin = 20;
constexpr uint32_t kSampleRate = 22050;
constexpr uint8_t kBitsPerSample = 16;
constexpr int16_t kAmplitude = 7000;
constexpr float kFrequencyHz = 440.0f;

I2S i2s(OUTPUT, kBclkPin, kDataPin);
uint32_t phase = 0;

uint32_t phaseStepForHz(float hz) {
  return static_cast<uint32_t>((hz * 4294967296.0) / kSampleRate);
}
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("max98357_tone_test");
  Serial.println("BCLK=GP18 LRCLK=GP19 DIN=GP20");

  i2s.setBitsPerSample(kBitsPerSample);
  i2s.setBuffers(8, 128);

  if (!i2s.begin(kSampleRate)) {
    Serial.println("I2S init failed");
    while (true) {
      delay(100);
    }
  }
}

void loop() {
  static const uint32_t step = phaseStepForHz(kFrequencyHz);
  phase += step;

  const int16_t sample = (phase & 0x80000000u) ? kAmplitude : -kAmplitude;
  i2s.write16(sample, sample);
}
