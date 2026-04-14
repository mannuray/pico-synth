#include "audio_i2s.h"

#include "board_config.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "i2s_out.pio.h"
#include "pico/stdlib.h"

static PIO s_pio = pio0;
static uint s_sm = 0;
static uint s_offset = 0;

void audio_i2s_init(uint32_t sample_rate) {
    gpio_init(I2S_BCLK_PIN);
    gpio_init(I2S_LRCLK_PIN);
    gpio_init(I2S_DATA_PIN);

    gpio_set_function(I2S_BCLK_PIN, GPIO_FUNC_PIO0);
    gpio_set_function(I2S_LRCLK_PIN, GPIO_FUNC_PIO0);
    gpio_set_function(I2S_DATA_PIN, GPIO_FUNC_PIO0);

    s_offset = pio_add_program(s_pio, &i2s_out_program);
    pio_sm_config config = i2s_out_program_get_default_config(s_offset);

    sm_config_set_out_pins(&config, I2S_DATA_PIN, 1);
    sm_config_set_sideset_pins(&config, I2S_BCLK_PIN);
    sm_config_set_out_shift(&config, false, false, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);

    const float pio_clock_hz = (float)clock_get_hz(clk_sys);
    const float bit_clock_hz = (float)sample_rate * 32.0f * 2.0f;
    const float divider = pio_clock_hz / (bit_clock_hz * 2.0f);
    sm_config_set_clkdiv(&config, divider);

    pio_gpio_init(s_pio, I2S_BCLK_PIN);
    pio_gpio_init(s_pio, I2S_LRCLK_PIN);
    pio_gpio_init(s_pio, I2S_DATA_PIN);

    pio_sm_set_consecutive_pindirs(s_pio, s_sm, I2S_BCLK_PIN, 1, true);
    pio_sm_set_consecutive_pindirs(s_pio, s_sm, I2S_LRCLK_PIN, 1, true);
    pio_sm_set_consecutive_pindirs(s_pio, s_sm, I2S_DATA_PIN, 1, true);

    pio_sm_init(s_pio, s_sm, s_offset, &config);
    pio_sm_set_enabled(s_pio, s_sm, true);
}

bool audio_i2s_write_stereo_blocking(const int16_t *samples, uint32_t frame_count) {
    for (uint32_t i = 0; i < frame_count; ++i) {
        const uint16_t left = (uint16_t)samples[(i * 2)];
        const uint16_t right = (uint16_t)samples[(i * 2) + 1];
        const uint32_t packed = ((uint32_t)left << 16) | right;
        pio_sm_put_blocking(s_pio, s_sm, packed);
    }

    return true;
}
