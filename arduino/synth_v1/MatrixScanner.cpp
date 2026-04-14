#include "MatrixScanner.h"

void MatrixScanner::begin() {
  for (size_t row = 0; row < config::kRowCount; ++row) {
    pinMode(config::kRowPins[row], OUTPUT);
    digitalWrite(config::kRowPins[row], HIGH);
  }

  for (size_t col = 0; col < config::kColCount; ++col) {
    pinMode(config::kColPins[col], INPUT_PULLUP);
  }
}

bool MatrixScanner::poll(Event &event) {
  if (eventIndex_ >= pendingCount_) {
    pendingCount_ = 0;
    eventIndex_ = 0;
    scanOnce();
  }

  if (eventIndex_ < pendingCount_) {
    event = pending_[eventIndex_++];
    return true;
  }

  return false;
}

void MatrixScanner::queueEvent(uint8_t row, uint8_t col, bool pressed) {
  if (pendingCount_ < (config::kRowCount * config::kColCount)) {
    pending_[pendingCount_++] = Event{row, col, pressed};
  }
}

void MatrixScanner::scanOnce() {
  for (size_t row = 0; row < config::kRowCount; ++row) {
    for (size_t i = 0; i < config::kRowCount; ++i) {
      digitalWrite(config::kRowPins[i], HIGH);
    }

    digitalWrite(config::kRowPins[row], LOW);
    delayMicroseconds(config::kMatrixSettleUs);

    for (size_t col = 0; col < config::kColCount; ++col) {
      const bool pressed = (digitalRead(config::kColPins[col]) == LOW);

      if (pressed == reading_[row][col]) {
        if (debounce_[row][col] < config::kDebounceScans) {
          debounce_[row][col]++;
        }
      } else {
        reading_[row][col] = pressed;
        debounce_[row][col] = 0;
      }

      if (debounce_[row][col] >= config::kDebounceScans && stable_[row][col] != pressed) {
        stable_[row][col] = pressed;
        queueEvent(static_cast<uint8_t>(row), static_cast<uint8_t>(col), pressed);
      }
    }
  }

  for (size_t row = 0; row < config::kRowCount; ++row) {
    digitalWrite(config::kRowPins[row], HIGH);
  }
}
