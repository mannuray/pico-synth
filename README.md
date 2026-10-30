# Pico Synth

A 37-key polyphonic synthesizer and USB MIDI controller built on the Raspberry Pi Pico (RP2040), replacing the original toy keyboard IC.

## Features

- **8-voice polyphony** at 44.1 kHz / 16-bit
- **24 presets** across 3 banks (keys & folk, orchestral, synth), level-matched
- **Multi-frame wavetable synthesis** with 14 instrument spectral models (512 samples x 4 frames each, stored in flash)
- **3-pair FM synthesis** (Algorithm 5 style) for electric piano and bell sounds
- **PolyBLEP anti-aliased** sawtooth and square oscillators
- **Exponential ADSR envelopes** with two-stage piano/guitar decay and a per-voice spectral envelope
- **Per-voice resonant state-variable filter** with note-dependent brightness
- **Per-voice stereo panning** across the stereo field
- **Built-in per-preset vibrato** in cents, fading in after the attack
- **Effects**: reverb, chorus, delay, tremolo, vibrato, bitcrusher
- **Per-preset default effects** (acoustic instruments get reverb, basses play dry, etc.)
- **Organ simulation**: key click, single-trigger Hammond percussion, Leslie rotary speaker
- **Rhythm engine**: 8 patterns (Pop, Rock, Ballad, Disco, Funk, Latin, Waltz, March) with accents, ghost notes, swing, fills and crashes, on a 12-piece drum kit
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

Presets are listed in slot order within each bank (which physical button selects which slot is still to be confirmed). Cello and the two basses play an octave below the key.

### Bank 1: Keys & Folk

| Slot | Preset | Sound |
|------|--------|-------|
| 1 | GrandPno | Piano wavetable, two-stage decay (fast drop, ~7 s tail), darkens as it rings |
| 2 | EPiano | FM electric piano: 1:1 body plus a 14:1 "tine", chorus |
| 3 | JazzOrg | Hammond-style: drawbar wavetable, key click, percussion, fast Leslie |
| 4 | ChurchOrg | Pipe organ: slow pipe speech, slight detune between ranks, reverb |
| 5 | Accordion | Reed wavetable with musette detuning |
| 6 | Harmonica | Blues harp with delayed hand vibrato |
| 7 | NylonGtr | Plucked nylon guitar, two-stage decay |
| 8 | Bells | Chowning FM bell (5:7 ratio), long inharmonic ring |

### Bank 2: Orchestral

| Slot | Preset | Sound |
|------|--------|-------|
| 1 | Violin | Bowed attack, ±22-cent vibrato fading in |
| 2 | Cello | An octave down, warm, vibrato fading in |
| 3 | Strings | Section: soft-string wavetable, unison, slow attack |
| 4 | Flute | Breathy, gentle vibrato |
| 5 | Clarinet | Odd-harmonic reed, no vibrato (classical style) |
| 6 | Trumpet | Bright brass, stays brassy while held |
| 7 | Brass | Section: unison, slower swell |
| 8 | Choir | Vocal pad, unison, slow attack |

### Bank 3: Synth

| Slot | Preset | Sound |
|------|--------|-------|
| 1 | SynthLead | Detuned unison saw, resonant filter, delay |
| 2 | SpacePad | Wide unison pad, very slow attack and filter sweep, chorus |
| 3 | FunkBass | Resonant saw "quack", an octave down, dry |
| 4 | SubBass | Pure sine an octave down, dry |
| 5 | Pluck | Short resonant saw pluck with delay |
| 6 | WahClav | Funky clav with a fast resonant filter sweep, dry |
| 7 | Chiptune | Square wave with 8-bit-style vibrato, bitcrusher |
| 8 | RingMod | Inharmonic FM (ratio 3.53), metallic, delay |

## Controls

| Button | Function |
|--------|----------|
| Instrument 1-8 | Select preset in current bank |
| Bank Up/Down | Switch between 3 banks |
| Octave Up/Down | Shift keyboard +/- 2 octaves |
| Transpose Up/Down | Shift +/- 12 semitones |
| Volume Up/Down | Master volume |
| Tempo Up/Down | Rhythm BPM |
| Sustain | Hold pedal (sends MIDI CC64) |
| Effect Select | Cycle through effects |
| Effect On/Off | Toggle current effect |
| Panic | All notes off |
| Rhythm Start/Stop | Toggle rhythm engine |
| Rhythm 1-8 | Select rhythm pattern (also recalls its usual tempo) |

## USB MIDI

The keyboard auto-detects its connection:

| Connection | Mode | Behavior |
|-----------|------|----------|
| Computer (USB data) | MIDI Only | Speaker muted, sends MIDI notes + CC |
| Power bank / charger | Synth Only | Speaker active, no MIDI |

The device appears as **"Synth Pico MIDI"** in DAWs and MIDI monitors. Sends note on/off, sustain (CC64), volume (CC7), and program changes.

Choosing a preset sends its **General MIDI program number**, so a GM instrument on the computer switches to the matching sound (GrandPno -> Acoustic Grand Piano, Violin -> Violin, and so on; see `gmProgram` in `Presets.cpp`). No bank select is sent: GM Level 1 has no banks.

The mode only switches once the connection has been stable for half a second, so a flaky cable doesn't cut notes off.

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

### Host tests

The engine and control logic also build on a desktop compiler, with no Pico needed:

```bash
tests/host/run.sh
```

This renders every preset with every effect and compares the result with `tests/host/golden_expected.txt`, then runs the engine, control and MIDI-output tests. If you change the sound on purpose, accept the new output with `tests/host/run.sh --update`.

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
AppController         Main loop: scheduling, USB mode detection
PerformanceController Key/button presses -> state changes -> output commands
PerformanceState      Bank, preset, effect, sustain, tempo, volume state
PitchSettings         Octave/transpose, key -> MIDI note
KeyTracker            Held and sustained keys
OutputRouter          Sends events/commands to the sinks for the current mode
SynthEventSink        Applies events/commands to the synth engine
MidiEventSink         USB MIDI output (notes, CC, program change)
SynthEngine           Voice allocation, mixing, effect/rotary routing, output
Voice                 One note: oscillators, envelope, filter
Oscillator            Waveforms and wavetables
RotarySpeaker         Leslie-style rotary for the organ presets
Effects               Master effects, one class each, plus the effect rack
Presets               Preset table
RhythmEngine          Drum pattern sequencer with sampled voices
MatrixScanner         Keyboard matrix scanning with debounce
I2SAudioOutput        I2S stereo output via PIO
ArduinoPlatform       Clock, USB host detection, status LED
WaveTableData.h       Pre-generated wavetable data (14 instruments x 4 frames)
```

## Resource Usage

| Resource | Used | Available | % |
|----------|------|-----------|---|
| Flash | 476 KB | 2,044 KB | 22% |
| RAM | 54 KB | 256 KB | 20% |
| CPU (8 voices + reverb) | not yet re-measured | 100% | see `cpu=` in the serial status line |

## Documentation

- [docs/engine.md](docs/engine.md) - Synth engine architecture, effects, rhythm, MIDI, performance
- [docs/instrument-profiles.md](docs/instrument-profiles.md) - Sound design details for all 24 presets
- [docs/matrix-map.md](docs/matrix-map.md) - Physical key matrix layout and button mapping

## License

This project is open source.
