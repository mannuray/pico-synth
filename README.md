# Pico Synth

A 37-key polyphonic synthesizer and USB MIDI controller built on the Raspberry Pi Pico (RP2040), replacing the original toy keyboard IC.

## Features

- **8-voice polyphony** at 44.1 kHz / 16-bit
- **8 instruments and 8 rhythms**, matching the labels printed on the keyboard, level-matched
- **Sampled instruments and drums** from the FluidR3_GM SoundFont (MIT licence): multi-zone, looped, 1.5 MB in flash
- **Synthesis voices still in the engine**: multi-frame wavetables, 3-pair FM, PolyBLEP oscillators
- **PolyBLEP anti-aliased** sawtooth and square oscillators
- **Exponential ADSR envelopes** with two-stage piano/guitar decay and a per-voice spectral envelope
- **Per-voice resonant state-variable filter** with note-dependent brightness
- **Per-voice stereo panning** across the stereo field
- **Built-in per-preset vibrato** in cents, fading in after the attack
- **Effects**: reverb, chorus, delay, tremolo, vibrato, bitcrusher
- **Per-preset default effects** (acoustic instruments get reverb, basses play dry, etc.)
- **Organ simulation**: key click, single-trigger Hammond percussion, Leslie rotary speaker
- **Rhythm engine**: 8 patterns (Rock, Slow Rock, Twist, Disco, March, Waltz, Samba, Blues) with accents, ghost notes, swing and triplet shuffles, fills and crashes, on a 12-piece drum kit
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

## Instruments

| Button | Preset | GM program sent over MIDI |
|--------|--------|---------------------------|
| T1 | Piano | 1 Acoustic Grand Piano |
| T2 | Organ (press again: Leslie slow/fast) | 17 Drawbar Organ |
| T3 | Violin | 41 Violin |
| T4 | Trumpet | 57 Trumpet |
| T5 | Banjo | 106 Banjo |
| T6 | Bell | 15 Tubular Bells |
| T7 | Music Box | 11 Music Box |
| T8 | Guitar | 25 Acoustic Guitar (nylon) |

## Rhythms

| Button | Pattern | Tempo | Feel |
|--------|---------|-------|------|
| R1 | Rock | 112 | 8th hats, kick on 1, 3 and the "and" of 3 |
| R2 | Slow Rock | 66 | 12/8 triplet feel |
| R3 | Twist | 156 | Early-60s backbeat |
| R4 | Disco | 120 | Four-on-the-floor, open hats on the "and"s |
| R5 | March | 116 | Bass drum on every beat, snare drags |
| R6 | Waltz | 90 | 3/4 "oom-pah-pah" |
| R7 | Samba | 100 | Surdo-style double kick, shaker 16ths, light swing |
| R8 | Blues | 96 | Triplet shuffle |

## Controls

The front-panel buttons are the toy keyboard's original ones, repurposed (toy label in brackets; see [docs/matrix-map.md](docs/matrix-map.md) for wiring).

| Button | Function |
|--------|----------|
| T1-T8 | Select instrument (loads its default effect). Pressing T2 (Organ) again switches the Leslie slow/fast |
| R1-R8 | Select rhythm; while stopped it also recalls the pattern's tempo, while playing your tempo is kept and the change happens at the next bar |
| Octave Down/Up (Duck/Bird) | Shift keyboard +/- 2 octaves |
| Transpose Down/Up (Rec/Play) | Shift +/- 12 semitones |
| Volume -/+ | 3 dB per press, down to silence |
| Tempo -/+ | 5 BPM per press (40-240) |
| Sustain (Drum/Animal) | Latching sustain on/off (MIDI CC64) |
| Effect Select (Demo A) | Cycle Vibrato, Tremolo, Chorus, Reverb, Delay, Bitcrusher; also switches the effect on |
| Effect On/Off (Demo B) | Toggle the effect |
| Start/Stop (Demo C) | Start/stop the drums |
| Fill (Dog) | Playing: fill on the next bar. Stopped: arm/disarm Sync Start (the rhythm starts with the first key you play) |
| Intro/Ending (Frog) | Stopped: start with an intro fill. Playing: end with a fill and a crash |
| Panic (Stop) | All notes off and sustain off; the rhythm keeps playing. **Hold 2 s** to switch the feedback sounds on/off |

**Hold to repeat:** Tempo, Volume and Transpose repeat while held (after 0.5 s, 10 per second).

### Feedback sounds

The keyboard has no display, so button presses are confirmed with short, quiet sounds from the speaker (also in MIDI mode; never sent over MIDI):

| Sound | Meaning |
|---|---|
| Rising two notes | Up / switched on |
| Falling two notes | Down / switched off |
| Double blip | Back to the default (octave 0, transpose 0) |
| Low "bonk" | Can't go further (limit) |
| Single blip | Changed (volume: standalone, it plays at the new level; effect type) |

Hold Panic for 2 seconds to switch them off or on (they are on again after power-up). Holding a repeating button at its limit gives one "bonk" and stops repeating.

## USB MIDI

The keyboard auto-detects its connection:

| Connection | Mode | Behavior |
|-----------|------|----------|
| Computer (USB data) | MIDI Only | Speaker muted, sends MIDI notes + CC |
| Power bank / charger | Synth Only | Speaker active, no MIDI |

The device appears as **"Synth Pico MIDI"** in DAWs and MIDI monitors. Sends note on/off, sustain (CC64), volume (CC7), and program changes.

Choosing a preset sends its **General MIDI program number**, so a GM instrument on the computer switches to the matching sound (Piano -> Acoustic Grand Piano, Violin -> Violin, and so on; see `gmProgram` in `Presets.cpp`). No bank select is sent: GM Level 1 has no banks.

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
arduino-cli compile -b rp2040:rp2040:rpipico:opt=Optimize2,freq=250 arduino/synth_v1
arduino-cli upload -b rp2040:rp2040:rpipico:opt=Optimize2,freq=250 -p /dev/cu.usbmodem2101 arduino/synth_v1
```

### Host tests

The engine and control logic also build on a desktop compiler, with no Pico needed:

```bash
tests/host/run.sh
```

This renders every preset with every effect and compares the result with `tests/host/golden_expected.txt`, then runs the engine, control and MIDI-output tests. If you change the sound on purpose, accept the new output with `tests/host/run.sh --update`.

## Sample Extractor

The instruments and the drum kit are cut from the FluidR3_GM SoundFont (`fluid-soundfont-gm` on Debian/Ubuntu, `/usr/share/sounds/sf2/FluidR3_GM.sf2`):

```bash
python3 tools/extract_sf2.py /usr/share/sounds/sf2/FluidR3_GM.sf2
```

It writes `arduino/synth_v1/SampleData.h` (8 instruments, up to 6 key zones each, mono 22.05 kHz, ~1.1 MB) and `DrumSamples.h` (12 drums, 44.1 kHz, ~0.4 MB). The `INSTRUMENTS` and `DRUMS` tables at the top of the script pick the GM programs, zone counts and lengths. Recordings are cut to fit; a sustained sound keeps its own loop when it fits, otherwise the last few whole pitch periods are looped with a cross-fade, and the preset's envelope shapes the rest. FluidR3's licence is in `third_party/FluidR3_GM/LICENSE`. (`tools/generate_drums.py`, the older synthesized kit, is kept as a fallback.)

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
PerformanceState      Instrument, effect, sustain, tempo, volume, rhythm state
PitchSettings         Octave/transpose, key -> MIDI note
KeyTracker            Held and sustained keys
OutputRouter          Sends events/commands to the sinks for the current mode
SynthEventSink        Applies events/commands to the synth engine
MidiEventSink         USB MIDI output (notes, CC, program change)
SynthEngine           Voice allocation, mixing, effect/rotary routing, output
Voice                 One note: oscillators, envelope, filter
Oscillator            Waveforms and wavetables
Samples               Sampled instruments: key zone lookup (the only file that includes SampleData.h)
RotarySpeaker         Leslie-style rotary for the organ presets
Effects               Master effects, one class each, plus the effect rack
Presets               Preset table
RhythmEngine          Drum pattern sequencer with sampled voices
MatrixScanner         Keyboard matrix scanning with debounce
I2SAudioOutput        I2S stereo output via PIO
ArduinoPlatform       Clock, USB host detection, status LED
SampleData.h          Instrument samples cut from FluidR3_GM by tools/extract_sf2.py
DrumSamples.h         Drum samples, from the same tool
WaveTableData.h       Pre-generated wavetable data (14 instruments x 4 frames)
```

## Resource Usage

| Resource | Used | Available | % |
|----------|------|-----------|---|
| Flash | 1,628 KB | 2,044 KB | 79% (1.5 MB of samples) |
| RAM | 121 KB | 256 KB | 46% |
| CPU, 8 voices + drums + effect (worst case, 250 MHz) | 47-58% average, 63% worst block | 100% | see docs/engine.md |

## Documentation

- [docs/engine.md](docs/engine.md) - Synth engine architecture, effects, rhythm, MIDI, performance
- [docs/instrument-profiles.md](docs/instrument-profiles.md) - Sound design details for the 8 instruments
- [docs/matrix-map.md](docs/matrix-map.md) - Physical key matrix layout and button mapping

## License

This project is open source. The instrument and drum samples come from FluidR3_GM by Frank Wen, under the MIT licence (`third_party/FluidR3_GM/LICENSE`).
