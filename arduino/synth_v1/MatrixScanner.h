#pragma once

#include "Config.h"

class MatrixScanner {
public:
  struct Event {
    uint8_t row;
    uint8_t col;
    bool pressed;
  };

  void begin();
  bool poll(Event &event);

private:
  bool stable_[config::kRowCount][config::kColCount] = {};
  bool reading_[config::kRowCount][config::kColCount] = {};
  uint8_t debounce_[config::kRowCount][config::kColCount] = {};
  uint8_t eventIndex_ = 0;
  Event pending_[config::kRowCount * config::kColCount] = {};
  uint8_t pendingCount_ = 0;

  void queueEvent(uint8_t row, uint8_t col, bool pressed);
  void scanOnce();
};
