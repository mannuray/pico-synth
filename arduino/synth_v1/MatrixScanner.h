#pragma once

#include "Config.h"
#include "IKeyMatrix.h"

class MatrixScanner final : public IKeyMatrix {
public:
  void begin();
  bool poll(KeyMatrixEvent &event) override;

private:
  bool stable_[config::kRowCount][config::kColCount] = {};
  bool reading_[config::kRowCount][config::kColCount] = {};
  uint8_t debounce_[config::kRowCount][config::kColCount] = {};
  uint8_t eventIndex_ = 0;
  KeyMatrixEvent pending_[config::kRowCount * config::kColCount] = {};
  uint8_t pendingCount_ = 0;

  void queueEvent(uint8_t row, uint8_t col, bool pressed);
  void scanOnce();
};
