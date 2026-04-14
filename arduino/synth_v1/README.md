# Synth V1

Main firmware for the Pico Synth. See the [project README](../../README.md) for full documentation.

## Quick Build

```bash
arduino-cli compile -b rp2040:rp2040:rpipico arduino/synth_v1
arduino-cli upload -b rp2040:rp2040:rpipico -p /dev/cu.usbmodem2101 arduino/synth_v1
```

## File Overview

| File | Purpose |
|------|---------|
| synth_v1.ino | Entry point |
| SynthEngine.h/cpp | Polyphonic synth engine (oscillators, envelopes, filters, effects) |
| WaveTableData.h | Pre-generated wavetable data (14 instruments, 4 frames each) |
| AppController.h/cpp | Main loop, matrix scanning, USB mode management |
| RhythmEngine.h/cpp | Drum pattern sequencer |
| Config.h | Hardware pins, sample rate, polyphony, bank configuration |
| MatrixMap.h | Key matrix to note/control mapping |
| MidiEventSink.h/cpp | USB MIDI output |
| SynthEventSink.h/cpp | Routes keyboard events to synth engine |
| I2SAudioOutput.h/cpp | I2S stereo audio output via PIO |
| KeyboardState.h/cpp | Note tracking, octave, transpose |
| PerformanceState.h/cpp | Bank, instrument, effect, volume state |
| MatrixScanner.h/cpp | Matrix scanning with debounce |
