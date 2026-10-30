#include "ArduinoPlatform.h"

#include <MIDIUSB.h>

namespace {
constexpr uint32_t kLedHeartbeatMs = 1000;
constexpr uint32_t kLedOnWindowMs = 90;
constexpr uint32_t kLedNoteFlashMs = 45;
}  // namespace

bool UsbMidiHostLink::isConnected() {
  return MidiUSB.connected();
}

void BuiltinStatusLed::begin(uint32_t nowMs) {
  heartbeatStartMs_ = nowMs;
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

void BuiltinStatusLed::flash(uint32_t nowMs) {
  flashUntilMs_ = nowMs + kLedNoteFlashMs;
}

void BuiltinStatusLed::update(uint32_t nowMs) {
#ifdef LED_BUILTIN
  const bool flashActive = nowMs < flashUntilMs_;
  const bool heartbeatOn = ((nowMs - heartbeatStartMs_) % kLedHeartbeatMs) < kLedOnWindowMs;
  digitalWrite(LED_BUILTIN, (flashActive || heartbeatOn) ? HIGH : LOW);
#else
  (void)nowMs;
#endif
}
