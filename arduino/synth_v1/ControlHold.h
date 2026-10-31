#pragma once

#include "ControlType.h"

#include <Arduino.h>

// Tracks the held control button for auto-repeat (Tempo, Volume, Transpose)
// and for the long press on Panic (toggles the feedback sounds).
class ControlHold {
public:
  static constexpr uint32_t kRepeatDelayMs = 500;     // hold this long before repeating
  static constexpr uint32_t kRepeatIntervalMs = 100;  // then repeat this often
  static constexpr uint32_t kLongPressMs = 2000;

  void press(ControlType control, uint8_t value, uint32_t nowMs) {
    held_ = control;
    repeating_ = true;
    value_ = value;
    pressedAtMs_ = nowMs;
    nextRepeatMs_ = nowMs + kRepeatDelayMs;
    longPressDone_ = false;
  }

  void release(ControlType control) {
    if (control == held_) {
      held_ = ControlType::None;
    }
  }

  // Calls repeat(control, value) for each repeat that is due; repeating stops
  // when it returns false (e.g. the value hit its limit). Calls
  // longPress(control) once when a long-press control has been held long enough.
  template <typename Repeat, typename LongPress>
  void update(uint32_t nowMs, Repeat repeat, LongPress longPress) {
    if (held_ == ControlType::None) {
      return;
    }
    if (repeats(held_)) {
      while (repeating_ && static_cast<int32_t>(nowMs - nextRepeatMs_) >= 0) {
        repeating_ = repeat(held_, value_);
        nextRepeatMs_ += kRepeatIntervalMs;
      }
    }
    if (hasLongPress(held_) && !longPressDone_ && nowMs - pressedAtMs_ >= kLongPressMs) {
      longPressDone_ = true;
      longPress(held_);
    }
  }

private:
  ControlType held_ = ControlType::None;
  uint8_t value_ = 0;
  uint32_t pressedAtMs_ = 0;
  uint32_t nextRepeatMs_ = 0;
  bool longPressDone_ = false;
  bool repeating_ = true;

  static bool repeats(ControlType control) {
    switch (control) {
      case ControlType::TempoDown:
      case ControlType::TempoUp:
      case ControlType::VolumeDown:
      case ControlType::VolumeUp:
      case ControlType::TransposeDown:
      case ControlType::TransposeUp:
        return true;
      default:
        return false;
    }
  }

  static bool hasLongPress(ControlType control) {
    return control == ControlType::Panic;
  }
};
