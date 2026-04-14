#include "keyboard_matrix.h"

#include "board_config.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

static bool s_last_state[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];

void keyboard_matrix_init(void) {
    for (uint8_t row = 0; row < KEY_MATRIX_ROWS; ++row) {
        gpio_init(KEY_ROW_PINS[row]);
        gpio_set_dir(KEY_ROW_PINS[row], GPIO_OUT);
        gpio_put(KEY_ROW_PINS[row], 1);
    }

    for (uint8_t col = 0; col < KEY_MATRIX_COLS; ++col) {
        gpio_init(KEY_COL_PINS[col]);
        gpio_set_dir(KEY_COL_PINS[col], GPIO_IN);
        gpio_pull_up(KEY_COL_PINS[col]);
    }
}

void keyboard_matrix_scan(key_matrix_state_t *state) {
    for (uint8_t row = 0; row < KEY_MATRIX_ROWS; ++row) {
        for (uint8_t r = 0; r < KEY_MATRIX_ROWS; ++r) {
            gpio_put(KEY_ROW_PINS[r], 1);
        }

        gpio_put(KEY_ROW_PINS[row], 0);
        sleep_us(50);

        for (uint8_t col = 0; col < KEY_MATRIX_COLS; ++col) {
            const bool pressed = gpio_get(KEY_COL_PINS[col]) == 0;
            state->pressed[row][col] = pressed;
            state->changed[row][col] = (pressed != s_last_state[row][col]);
            s_last_state[row][col] = pressed;
        }
    }
}

uint8_t keyboard_matrix_note_number(uint8_t row, uint8_t col) {
    return (uint8_t)(48 + (row * KEY_MATRIX_COLS) + col);
}
