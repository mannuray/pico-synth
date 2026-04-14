#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

void synth_init(uint32_t sample_rate);
void synth_note_on(uint8_t midi_note);
void synth_note_off(uint8_t midi_note);
void synth_render(int16_t *stereo_frames, uint32_t frame_count, uint16_t mic_level);
bool synth_any_voice_active(void);

#endif
