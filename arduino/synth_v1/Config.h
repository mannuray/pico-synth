#pragma once

#include <Arduino.h>

namespace config {
constexpr uint8_t kRowPins[] = {0, 1, 2, 3, 4, 5, 14, 15, 16};
constexpr uint8_t kColPins[] = {6, 7, 8, 9, 10, 11, 12, 13};

constexpr size_t kRowCount = sizeof(kRowPins) / sizeof(kRowPins[0]);
constexpr size_t kColCount = sizeof(kColPins) / sizeof(kColPins[0]);

constexpr pin_size_t kI2SBclkPin = 18;
constexpr pin_size_t kI2SDataPin = 20;
constexpr pin_size_t kMicAdcPin = 27;

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kSampleRate = 44100;
constexpr uint8_t kBitsPerSample = 16;
constexpr uint8_t kPolyphony = 8;

constexpr uint32_t kMatrixSettleUs = 50;
constexpr uint8_t kDebounceScans = 3;
constexpr uint32_t kScanIntervalMs = 3;
constexpr uint32_t kStatusIntervalMs = 1000;
constexpr size_t kAudioBlockFrames = 64;
// I2S output queue: 6 buffers x 64 stereo frames = 384 frames, ~8.7 ms of
// key-to-sound latency (was 8 x 256 = ~46 ms). Raise the count if the audio
// crackles (the main loop must refill a buffer every ~1.45 ms).
constexpr size_t kI2SBufferCount = 6;
constexpr size_t kI2SBufferFrames = 64;
// USB connect/disconnect must hold this long before the output mode switches.
constexpr uint32_t kModeSwitchDebounceMs = 500;

constexpr int8_t kMinOctaveShift = -2;
constexpr int8_t kMaxOctaveShift = 2;
constexpr int8_t kMinTranspose = -12;
constexpr int8_t kMaxTranspose = 12;

constexpr uint8_t kBankCount = 3;
constexpr uint8_t kInstrumentsPerBank = 8;
constexpr uint8_t kPresetCount = kBankCount * kInstrumentsPerBank;

constexpr uint8_t kBaseMidiNote = 48; // C3 on the leftmost key
constexpr uint8_t kTempoMinBpm = 40;
constexpr uint8_t kTempoMaxBpm = 240;
constexpr uint8_t kTempoDefaultBpm = 120;

constexpr int16_t kDefaultMasterVolume = 24576;
constexpr int16_t kMinMasterVolume = 0;
constexpr int16_t kMaxMasterVolume = 32767;
}
