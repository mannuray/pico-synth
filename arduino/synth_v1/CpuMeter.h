#pragma once

#include <Arduino.h>

// Share of wall-clock time spent rendering audio, as a percentage.
class CpuMeter {
public:
  void reset(uint32_t nowUs) {
    windowStartUs_ = nowUs;
    busyUs_ = 0;
  }

  void addBusyUs(uint32_t us) { busyUs_ += us; }

  // Closes the current measurement window and starts a new one.
  void update(uint32_t nowUs) {
    const uint32_t elapsedUs = nowUs - windowStartUs_;
    if (elapsedUs > 0) {
      loadPercent_ = static_cast<uint16_t>((static_cast<uint64_t>(busyUs_) * 100u) / elapsedUs);
    }
    reset(nowUs);
  }

  uint16_t loadPercent() const { return loadPercent_; }

private:
  uint32_t windowStartUs_ = 0;
  uint32_t busyUs_ = 0;
  uint16_t loadPercent_ = 0;
};
