#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdbool.h>
#include <stdint.h>

void audio_i2s_init(uint32_t sample_rate);
bool audio_i2s_write_stereo_blocking(const int16_t *samples, uint32_t frame_count);

#endif
