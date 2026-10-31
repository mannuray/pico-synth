#pragma once

// AUDIO_HOT(name) runs a per-sample audio function from RAM on the RP2040.
// Code in flash runs through a 16 KB cache, which the streaming drum samples
// keep evicting; RAM code doesn't miss. Elsewhere (host tests) it does nothing.
#if defined(ARDUINO_ARCH_RP2040)
#include <pico.h>
#define AUDIO_HOT(name) __not_in_flash_func(name)
#else
#define AUDIO_HOT(name) name
#endif
