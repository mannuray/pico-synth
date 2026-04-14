#include <Arduino.h>
#include <I2S.h>

namespace {
constexpr pin_size_t PIN_I2S_BCLK = 18;
constexpr pin_size_t PIN_I2S_DOUT = 20;
constexpr pin_size_t PIN_MIC_ADC = 26;

constexpr int SAMPLE_RATE = 22050;
constexpr int BITS_PER_SAMPLE = 16;
constexpr int SERIAL_BAUD = 115200;

constexpr int32_t BASE_AMPLITUDE = 2800;
constexpr uint32_t MIC_REPORT_INTERVAL_MS = 1000;

I2S i2s(OUTPUT, PIN_I2S_BCLK, PIN_I2S_DOUT);

uint32_t phaseA = 0;
uint32_t phaseB = 0;
uint32_t phaseC = 0;

uint32_t phaseStepA = 0;
uint32_t phaseStepB = 0;
uint32_t phaseStepC = 0;

uint32_t smoothedMic = 0;
uint32_t lastReportMs = 0;

uint32_t phaseStepForHz(float hz) {
  return static_cast<uint32_t>((hz * 4294967296.0) / SAMPLE_RATE);
}

int16_t triangleFromPhase(uint32_t phase) {
  const uint16_t top = static_cast<uint16_t>(phase >> 16);
  int32_t value = (top < 32768u) ? top : (65535u - top);
  value = (value * 2) - 32767;
  return static_cast<int16_t>(value);
}

void initAudio() {
  i2s.setBCLK(PIN_I2S_BCLK);
  i2s.setDATA(PIN_I2S_DOUT);
  i2s.setBitsPerSample(BITS_PER_SAMPLE);
  i2s.setBuffers(8, 128);

  if (!i2s.begin(SAMPLE_RATE)) {
    Serial.println("I2S init failed");
    while (true) {
      delay(100);
    }
  }

  phaseStepA = phaseStepForHz(261.63f); // C4
  phaseStepB = phaseStepForHz(329.63f); // E4
  phaseStepC = phaseStepForHz(392.00f); // G4
}

uint16_t readMicLevel() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    sum += analogRead(PIN_MIC_ADC);
  }

  const uint16_t raw = static_cast<uint16_t>(sum / 8u);
  const uint16_t centered = (raw > 2048u) ? (raw - 2048u) : (2048u - raw);

  smoothedMic += centered;
  smoothedMic -= (smoothedMic >> 4);
  return static_cast<uint16_t>(smoothedMic >> 4);
}

void writeAudioFrame(uint16_t micLevel) {
  const int32_t volume = BASE_AMPLITUDE + static_cast<int32_t>(micLevel) * 6;
  const uint32_t vibrato = static_cast<uint32_t>(micLevel) << 5;

  phaseA += phaseStepA + vibrato;
  phaseB += phaseStepB + (vibrato >> 1);
  phaseC += phaseStepC + (vibrato >> 2);

  int32_t mix = 0;
  mix += triangleFromPhase(phaseA);
  mix += triangleFromPhase(phaseB);
  mix += triangleFromPhase(phaseC);
  mix /= 3;

  mix = (mix * volume) / 32768;

  if (mix > 32767) {
    mix = 32767;
  } else if (mix < -32768) {
    mix = -32768;
  }

  const int16_t sample = static_cast<int16_t>(mix);
  i2s.write16(sample, sample);
}
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1200);

  analogReadResolution(12);
  pinMode(PIN_MIC_ADC, INPUT);

  Serial.println();
  Serial.println("audio_bringup");
  Serial.println("I2S: BCLK=GP18 LRCLK=GP19 DOUT=GP20");
  Serial.println("ADC: GP26 / ADC0");

  initAudio();
  lastReportMs = millis();
}

void loop() {
  const uint16_t micLevel = readMicLevel();
  writeAudioFrame(micLevel);

  if (millis() - lastReportMs >= MIC_REPORT_INTERVAL_MS) {
    Serial.print("mic_level=");
    Serial.println(micLevel);
    lastReportMs = millis();
  }
}
