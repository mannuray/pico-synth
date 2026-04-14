#include "synth_engine.h"

#include "board_config.h"

typedef struct {
    bool active;
    uint8_t midi_note;
    float phase;
    float phase_step;
    float level;
    bool releasing;
} voice_t;

static voice_t s_voices[SYNTH_MAX_VOICES];
static uint32_t s_sample_rate = AUDIO_SAMPLE_RATE;

static float midi_note_to_frequency(uint8_t note) {
    const int semitones_from_a4 = (int)note - 69;
    float frequency = 440.0f;

    if (semitones_from_a4 > 0) {
        for (int i = 0; i < semitones_from_a4; ++i) {
            frequency *= 1.05946309436f;
        }
    } else {
        for (int i = 0; i < -semitones_from_a4; ++i) {
            frequency /= 1.05946309436f;
        }
    }

    return frequency;
}

static voice_t *allocate_voice(uint8_t midi_note) {
    for (uint32_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (!s_voices[i].active) {
            return &s_voices[i];
        }
    }

    return &s_voices[0];
}

void synth_init(uint32_t sample_rate) {
    s_sample_rate = sample_rate;

    for (uint32_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        s_voices[i].active = false;
        s_voices[i].midi_note = 0;
        s_voices[i].phase = 0.0f;
        s_voices[i].phase_step = 0.0f;
        s_voices[i].level = 0.0f;
        s_voices[i].releasing = false;
    }
}

void synth_note_on(uint8_t midi_note) {
    voice_t *voice = allocate_voice(midi_note);
    const float frequency = midi_note_to_frequency(midi_note);

    voice->active = true;
    voice->midi_note = midi_note;
    voice->phase = 0.0f;
    voice->phase_step = frequency / (float)s_sample_rate;
    voice->level = 0.0f;
    voice->releasing = false;
}

void synth_note_off(uint8_t midi_note) {
    for (uint32_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (s_voices[i].active && s_voices[i].midi_note == midi_note) {
            s_voices[i].releasing = true;
        }
    }
}

bool synth_any_voice_active(void) {
    for (uint32_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
        if (s_voices[i].active) {
            return true;
        }
    }

    return false;
}

void synth_render(int16_t *stereo_frames, uint32_t frame_count, uint16_t mic_level) {
    const float mic_mod = 1.0f + ((float)mic_level / 4096.0f) * 0.15f;

    for (uint32_t frame = 0; frame < frame_count; ++frame) {
        float mix = 0.0f;

        for (uint32_t i = 0; i < SYNTH_MAX_VOICES; ++i) {
            voice_t *voice = &s_voices[i];
            if (!voice->active) {
                continue;
            }

            if (voice->releasing) {
                voice->level -= 0.0025f;
                if (voice->level <= 0.0f) {
                    voice->active = false;
                    voice->level = 0.0f;
                    continue;
                }
            } else if (voice->level < 0.2f) {
                voice->level += 0.0035f;
            }

            const float sample = (voice->phase < 0.5f) ? 1.0f : -1.0f;
            mix += sample * voice->level;

            voice->phase += voice->phase_step * mic_mod;
            if (voice->phase >= 1.0f) {
                voice->phase -= 1.0f;
            }
        }

        if (mix > 1.0f) {
            mix = 1.0f;
        } else if (mix < -1.0f) {
            mix = -1.0f;
        }

        const int16_t pcm = (int16_t)(mix * 16000.0f);
        stereo_frames[frame * 2] = pcm;
        stereo_frames[(frame * 2) + 1] = pcm;
    }
}
