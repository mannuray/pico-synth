#include <stdio.h>

#include "audio_i2s.h"
#include "board_config.h"
#include "keyboard_matrix.h"
#include "mic_adc.h"
#include "pico/stdlib.h"
#include "synth_engine.h"

int main(void) {
    stdio_init_all();
    sleep_ms(250);

    keyboard_matrix_init();
    mic_adc_init();
    synth_init(AUDIO_SAMPLE_RATE);
    audio_i2s_init(AUDIO_SAMPLE_RATE);

    key_matrix_state_t matrix_state = {0};
    int16_t audio_buffer[AUDIO_BLOCK_SAMPLES * 2] = {0};

    while (true) {
        keyboard_matrix_scan(&matrix_state);

        for (uint8_t row = 0; row < KEY_MATRIX_ROWS; ++row) {
            for (uint8_t col = 0; col < KEY_MATRIX_COLS; ++col) {
                if (!matrix_state.changed[row][col]) {
                    continue;
                }

                const uint8_t note = keyboard_matrix_note_number(row, col);
                if (matrix_state.pressed[row][col]) {
                    synth_note_on(note);
                } else {
                    synth_note_off(note);
                }
            }
        }

        const uint16_t mic_level = mic_adc_read_level();
        synth_render(audio_buffer, AUDIO_BLOCK_SAMPLES, mic_level);
        audio_i2s_write_stereo_blocking(audio_buffer, AUDIO_BLOCK_SAMPLES);
    }

    return 0;
}
