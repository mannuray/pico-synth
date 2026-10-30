#pragma once

#include "IClock.h"
#include "IHostLink.h"
#include "IStatusIndicator.h"

class ArduinoClock final : public IClock {
public:
  uint32_t nowMs() const override { return millis(); }
  uint32_t nowUs() const override { return micros(); }
};

class UsbMidiHostLink final : public IHostLink {
public:
  bool isConnected() override;
};

// Heartbeat blink on the built-in LED, with a short flash on demand.
class BuiltinStatusLed final : public IStatusIndicator {
public:
  void begin(uint32_t nowMs);
  void flash(uint32_t nowMs) override;
  void update(uint32_t nowMs) override;

private:
  uint32_t heartbeatStartMs_ = 0;
  uint32_t flashUntilMs_ = 0;
};
