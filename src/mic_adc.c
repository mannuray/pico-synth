#include "mic_adc.h"

#include "board_config.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

static uint32_t s_smoothed_level = 0;

void mic_adc_init(void) {
    adc_init();
    adc_gpio_init(MIC_ADC_GPIO);
    adc_select_input(MIC_ADC_INPUT);
}

uint16_t mic_adc_read_raw(void) {
    adc_select_input(MIC_ADC_INPUT);
    return adc_read();
}

uint16_t mic_adc_read_level(void) {
    const uint16_t raw = mic_adc_read_raw();
    const uint16_t centered = (raw > 2048u) ? (raw - 2048u) : (2048u - raw);

    s_smoothed_level += centered;
    s_smoothed_level -= (s_smoothed_level >> MIC_LEVEL_SMOOTH_SHIFT);

    return (uint16_t)(s_smoothed_level >> MIC_LEVEL_SMOOTH_SHIFT);
}
