# Audio Bring-Up

This sketch is the first audio validation step for the Pico retrofit:

- output synthesized audio to a `MAX98357A` over I2S
- read a mic or preamp signal on the Pico ADC
- use the ADC level to modulate the sound so both subsystems can be verified quickly

## Wiring

`MAX98357A`:

- `GP18 -> BCLK`
- `GP19 -> LRC/LRCLK`
- `GP20 -> DIN`
- `GND -> GND`
- `VIN -> 5V` or the supply your board supports

Mic / external analog audio input:

- `GP26 / ADC0 <- mic preamp output`
- `GND -> GND`

Do not connect a raw microphone capsule directly to `GP26`. Use a biased mic preamp or another analog source limited to `0V..3.3V`.

## Behavior

- Plays a steady synth tone on the speaker.
- Mic level changes the output volume and adds mild pitch vibrato.
- Serial monitor prints the smoothed ADC level once per second.

## Serial

- Baud: `115200`

## Notes

- This sketch does not use the keyboard matrix yet.
- It is a bring-up tool to confirm I2S audio and ADC input before integrating the full synth.
