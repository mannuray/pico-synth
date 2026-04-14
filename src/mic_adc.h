#ifndef MIC_ADC_H
#define MIC_ADC_H

#include <stdint.h>

void mic_adc_init(void);
uint16_t mic_adc_read_raw(void);
uint16_t mic_adc_read_level(void);

#endif
