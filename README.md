# Pico Synth

A 37-key polyphonic synthesizer and USB MIDI controller built on the Raspberry Pi Pico (RP2040), replacing the original toy keyboard IC.

## Features

- **8-voice polyphony** at 44.1 kHz / 16-bit
- **24 presets** across 3 banks (acoustic, electronic, experimental)
- **Multi-frame wavetable synthesis** with 14 instrument spectral models (512 samples x 4 frames each, stored in flash)
- **3-pair FM synthesis** (Algorithm 5 style) for electric piano and bell sounds
- **PolyBLEP anti-aliased** sawtooth and square oscillators
- **Exponential ADSR envelopes** with per-voice spectral modulation decay
- **Per-voice resonant state-variable filter** with note-dependent brightness
- **Per-voice stereo panning** across the stereo field
- **Built-in per-preset vibrato** for solo instruments (violin, clarinet, flute)
- **Effects**: reverb, chorus, delay, tremolo, vibrato, bitcrusher
- **Per-preset default effects** (piano gets reverb, strings get chorus, etc.)
- **Organ simulation**: key click noise, Hammond percussion, rotary speaker
- **Rhythm engine** with 8 patterns and synthesized drum voices
- **USB MIDI** with auto-detection (computer = MIDI controller, power only = standalone synth)
- **Output dithering** for clean quiet tails

## Hardware

### Required

- Raspberry Pi Pico (RP2040)
- MAX98357A I2S amplifier/DAC
- Speaker
- 37-key toy keyboard (matrix-scanned)

### Wiring

| Signal | Pico Pin |
|--------|----------|
| I2S BCLK | GP18 |
| I2S LRCLK | GP19 |
| I2S DIN | GP20 |
| Matrix Rows | GP0-GP5, GP14-GP16 |
| Matrix Columns | GP6-GP13 |
| Mic ADC (optional) | GP27 / ADC1 |

See [docs/matrix-map.md](docs/matrix-map.md) for the full key matrix layout.

## Preset Banks

### Bank 1: Acoustic / Traditional

| Button | Preset | Synthesis |
|--------|--------|-----------|
| 1 | GrandPno | Multi-frame wavetable (piano harmonics + inharmonicity) |
| 2 | NylonGtr | Multi-frame wavetable (plucked guitar) |
| 3 | Violin | Multi-frame wavetable (bowed string + vibrato) |
| 4 | PanFlute | Multi-frame wavetable (flute + breath noise) |
| 5 | JazzOrg | Wavetable (drawbar organ + key click + rotary) |
| 6 | FullOrg | Wavetable (pipe organ + rotary + unison) |
| 7 | Accordion | Wavetable (reed harmonics + musette detuning) |
| 8 | Clarinet | Wavetable (odd harmonics + vibrato) |

### Bank 2: Synth / Electronic

| Button | Preset | Synthesis |
|--------|--------|-----------|
| 1 | SpacePad | Wavetable (choir + wide unison + slow evolve) |
| 2 | FunkBass | Saw (resonance 240, filter sweep per note) |
| 3 | SynthLead | Wavetable (pulse-saw hybrid + unison + vibrato) |
| 4 | BellTone | FM (ratio 5, gentle, long ring) |
| 5 | BitOrgan | Square (lo-fi chiptune, retro 8-bit) |
| 6 | WahClav | Saw (auto-wah, resonance 220, fast filter sweep) |
| 7 | Harmonica | Wavetable (blues harp + breath vibrato) |
| 8 | Cello | Wavetable (deep bowed string + vibrato) |

### Bank 3: Experimental / Wild

| Button | Preset | Synthesis |
|--------|--------|-----------|
| 1 | SubBass | Triangle (deep sub, felt more than heard) |
| 2 | Siren | Sine (extreme vibrato, wobbling pitch) |
| 3 | Trumpet | Wavetable (warm brass + vibrato) |
| 4 | Choir | Wavetable (vocal pad + unison) |
| 5 | Pluck | Saw (sharp percussive, 100ms decay) |
| 6 | Vox | Wavetable (haunting vocal, resonance formants) |
| 7 | Brass | Wavetable (punchy stab, fast brightness close) |
| 8 | RingMod | FM (ratio 8, metallic/industrial, feedback) |

## Controls

| Button | Function |
|--------|----------|
| Instrument 1-8 | Select preset in current bank |
| Bank Up/Down | Switch between 3 banks |
| Octave Up/Down | Shift keyboard +/- 2 octaves |
| Transpose Up/Down | Shift +/- 12 semitones |
| Volume Up/Down | Master volume |
| Tempo Up/Down | Rhythm/metronome BPM |
| Sustain | Hold pedal (sends MIDI CC64) |
| Effect Select | Cycle through effects |
| Effect On/Off | Toggle current effect |
| Panic | All notes off |
| Rhythm Start/Stop | Toggle rhythm engine |
| Rhythm 1-8 | Select rhythm pattern |

## USB MIDI

The keyboard auto-detects its connection:

| Connection | Mode | Behavior |
|-----------|------|----------|
| Computer (USB data) | MIDI Only | Speaker muted, sends MIDI notes + CC |
| Power bank / charger | Synth Only | Speaker active, no MIDI |

The device appears as **"Synth Pico MIDI"** in DAWs and MIDI monitors. Sends note on/off, sustain (CC64), volume (CC7), and program changes.

## Build

### Prerequisites

- [arduino-cli](https://arduino.github.io/arduino-cli/)
- [arduino-pico core](https://github.com/earlephilhower/arduino-pico) v5.5.1+

### Install the board

```bash
arduino-cli core update-index --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core install rp2040:rp2040 --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```

### Compile and upload

```bash
arduino-cli compile -b rp2040:rp2040:rpipico arduino/synth_v1
arduino-cli upload -b rp2040:rp2040:rpipico -p /dev/cu.usbmodem2101 arduino/synth_v1
```

## Wavetable Generator

The `tools/extract_wavetable.py` script generates multi-frame wavetable data from spectral models:

```bash
python3 tools/extract_wavetable.py --generate-all --table-size 512 --frames 4
```

This outputs `arduino/synth_v1/WaveTableData.h` with 14 instrument tables (56 KB in flash). Each instrument has 4 frames with progressive harmonic rolloff, noise components, inharmonicity, and body formants.

The script can also extract wavetables from WAV recordings:

```bash
python3 tools/extract_wavetable.py recording.wav --name Piano --table-size 512 --frames 4
```

## Architecture

```
synth_v1.ino          Entry point, wires up components
AppController         Main loop, matrix scanning, mode management
SynthEngine           Polyphonic synth: oscillators, envelopes, filters, effects
RhythmEngine          Drum pattern sequencer with synthesized voices
MatrixScanner         Keyboard matrix scanning with debounce
KeyboardState         Note tracking, octave/transpose management
PerformanceState      Bank, instrument, effect, volume state
I2SAudioOutput        I2S stereo output via PIO
MidiEventSink         USB MIDI output (notes, CC, program change)
SynthEventSink        Routes events to the synth engine
SerialDebugSink       Serial monitor debug output
WaveTableData.h       Pre-generated wavetable data (14 instruments x 4 frames)
```

## Resource Usage

| Resource | Used | Available | % |
|----------|------|-----------|---|
| Flash | 137 KB | 2,048 KB | 6% |
| RAM | 21 KB | 264 KB | 7% |
| CPU (idle) | ~11% | 100% | |
| CPU (8 voices + reverb) | ~19% | 100% | |

## Documentation

- [docs/engine.md](docs/engine.md) - Synth engine architecture, effects, rhythm, MIDI, performance
- [docs/instrument-profiles.md](docs/instrument-profiles.md) - Sound design details for all 24 presets
- [docs/matrix-map.md](docs/matrix-map.md) - Physical key matrix layout and button mapping

## License

This project is open source.
