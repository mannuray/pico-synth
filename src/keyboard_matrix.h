#ifndef KEYBOARD_MATRIX_H
#define KEYBOARD_MATRIX_H

#include "board_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool pressed[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];
    bool changed[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];
} key_matrix_state_t;

void keyboard_matrix_init(void);
void keyboard_matrix_scan(key_matrix_state_t *state);
uint8_t keyboard_matrix_note_number(uint8_t row, uint8_t col);

#endif
