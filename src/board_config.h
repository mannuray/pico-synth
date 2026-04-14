#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "hardware/adc.h"

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_BLOCK_SAMPLES 128
#define SYNTH_MAX_VOICES 8

#define KEY_MATRIX_ROWS 4
#define KEY_MATRIX_COLS 8

static const uint8_t KEY_ROW_PINS[KEY_MATRIX_ROWS] = {2, 3, 4, 5};
static const uint8_t KEY_COL_PINS[KEY_MATRIX_COLS] = {6, 7, 8, 9, 10, 11, 12, 13};

#define I2S_BCLK_PIN 18
#define I2S_LRCLK_PIN 19
#define I2S_DATA_PIN 20

#define MIC_ADC_GPIO 26
#define MIC_ADC_INPUT 0

#define MIC_LEVEL_SMOOTH_SHIFT 5

#endif
