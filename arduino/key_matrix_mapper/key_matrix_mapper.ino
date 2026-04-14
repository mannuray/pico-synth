#include <Arduino.h>

/*
  Pico key matrix mapper

  Edit ROW_PINS and COL_PINS to match the wires you connect from the toy
  keyboard matrix to the Pico. Open Serial Monitor at 115200 baud.

  Scan model:
  - Each row pin is driven LOW one at a time.
  - Column pins are inputs with internal pull-ups.
  - A pressed key is detected when a column reads LOW while its row is active.

  If your matrix is wired the opposite way, swap the physical connections so
  Pico outputs go to one side of the matrix and Pico inputs go to the other.
*/

static const uint8_t ROW_PINS[] = {
  0, 1, 2, 3, 4, 5, 14, 15, 16
};

static const uint8_t COL_PINS[] = {
  6, 7, 8, 9, 10, 11, 12, 13
};

static const uint8_t ROW_COUNT = sizeof(ROW_PINS) / sizeof(ROW_PINS[0]);
static const uint8_t COL_COUNT = sizeof(COL_PINS) / sizeof(COL_PINS[0]);

static const unsigned long SETTLE_US = 60;
static const unsigned long SCAN_DELAY_MS = 3;

static bool stableState[ROW_COUNT][COL_COUNT];
static bool lastReading[ROW_COUNT][COL_COUNT];
static uint8_t debounceCount[ROW_COUNT][COL_COUNT];

static const uint8_t DEBOUNCE_SCANS = 3;

void configurePins() {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    pinMode(ROW_PINS[row], OUTPUT);
    digitalWrite(ROW_PINS[row], HIGH);
  }

  for (uint8_t col = 0; col < COL_COUNT; col++) {
    pinMode(COL_PINS[col], INPUT_PULLUP);
  }
}

void printBanner() {
  Serial.println();
  Serial.println("=== Pico Key Matrix Mapper ===");
  Serial.print("Rows: ");
  Serial.println(ROW_COUNT);
  Serial.print("Cols: ");
  Serial.println(COL_COUNT);

  Serial.println("Row GPIOs:");
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    Serial.print("  R");
    Serial.print(row);
    Serial.print(" -> GP");
    Serial.println(ROW_PINS[row]);
  }

  Serial.println("Col GPIOs:");
  for (uint8_t col = 0; col < COL_COUNT; col++) {
    Serial.print("  C");
    Serial.print(col);
    Serial.print(" -> GP");
    Serial.println(COL_PINS[col]);
  }

  Serial.println();
  Serial.println("Press keys. Output format:");
  Serial.println("DOWN R<row> C<col>");
  Serial.println("UP   R<row> C<col>");
  Serial.println();
}

void reportChange(uint8_t row, uint8_t col, bool pressed) {
  Serial.print(pressed ? "DOWN " : "UP   ");
  Serial.print("R");
  Serial.print(row);
  Serial.print(" C");
  Serial.print(col);
  Serial.print("  [GP");
  Serial.print(ROW_PINS[row]);
  Serial.print(" x GP");
  Serial.print(COL_PINS[col]);
  Serial.println("]");
}

void scanMatrix() {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    for (uint8_t i = 0; i < ROW_COUNT; i++) {
      digitalWrite(ROW_PINS[i], HIGH);
    }

    digitalWrite(ROW_PINS[row], LOW);
    delayMicroseconds(SETTLE_US);

    for (uint8_t col = 0; col < COL_COUNT; col++) {
      const bool pressed = (digitalRead(COL_PINS[col]) == LOW);

      if (pressed == lastReading[row][col]) {
        if (debounceCount[row][col] < DEBOUNCE_SCANS) {
          debounceCount[row][col]++;
        }
      } else {
        debounceCount[row][col] = 0;
        lastReading[row][col] = pressed;
      }

      if (debounceCount[row][col] >= DEBOUNCE_SCANS && stableState[row][col] != pressed) {
        stableState[row][col] = pressed;
        reportChange(row, col, pressed);
      }
    }
  }

  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    digitalWrite(ROW_PINS[row], HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  configurePins();
  memset(stableState, 0, sizeof(stableState));
  memset(lastReading, 0, sizeof(lastReading));
  memset(debounceCount, 0, sizeof(debounceCount));
  printBanner();
}

void loop() {
  scanMatrix();
  delay(SCAN_DELAY_MS);
}
