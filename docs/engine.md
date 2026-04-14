# Synth Engine

Architecture reference for the Arduino synth_v1 engine.

## Audio Path

Signal flow per sample:

1. render each active voice (oscillator + envelope + filter)
2. per-voice stereo panning
3. sum voices with polyphony scaling
4. mix rhythm engine output
5. apply selected master effect (reverb/chorus/delay/etc.)
6. apply master volume
7. apply dithering
8. soft clip and write stereo frames over I2S to MAX98357A

Specifications:

- 44,100 Hz sample rate
- 16-bit output
- 8-voice polyphony
- 64-sample audio block size

## Oscillator Layer

Supported waveform generators:

- **Triangle** — simple phase-based
- **Saw** — PolyBLEP anti-aliased
- **Square** — PolyBLEP anti-aliased (both edges)
- **Sine** — 512-sample table with linear interpolation (stored in flash)
- **Wavetable** — multi-frame crossfading (4 frames, 512 samples each, stored in flash)
- **FM** — 3-pair Algorithm 5 style (body + richness + tine/bell)

### Multi-Frame Wavetable System

14 instrument spectral models generated offline by `tools/extract_wavetable.py`:

| Index | Instrument | Characteristics |
|-------|-----------|-----------------|
| 0 | GrandPno | Piano inharmonicity, hammer noise, 8 harmonics |
| 1 | Violin | 2nd harmonic strongest, bow noise, body formants |
| 2 | NylonGtr | Pluck noise, string inharmonicity |
| 3 | Flute | Weak upper harmonics, strong breath noise |
| 4 | Trumpet | Strong mid harmonics, mute resonance formants |
| 5 | Harmonica | Odd-heavy harmonics, reed/breath noise |
| 6 | Cello | Rich harmonics, body formants |
| 7 | Organ | 6-harmonic drawbar model |
| 8 | Choir | Vocal formants (ah vowel), breath noise |
| 9 | SoftStr | String ensemble harmonics |
| 10 | Reed | Odd harmonics only (clarinet-like) |
| 11 | AccordionReed | All harmonics, 1/n sawtooth rolloff, reed noise |
| 12 | PipeOrgan | Full 8-harmonic registration |
| 13 | SoftLead | Pulse-saw hybrid |

Each table has 4 frames with progressive harmonic rolloff:
- Frame 0 (attack): all harmonics at full strength
- Frame 1: upper harmonics at 70% per octave
- Frame 2: upper harmonics at 42% per octave
- Frame 3 (tail): upper harmonics at 22% per octave

The spectral modulation envelope (`modLevel`) drives crossfading between frames, giving time-varying timbre without relying solely on the filter.

Total wavetable data: 56 KB in flash, zero RAM.

### 3-Pair FM Synthesis

For presets that need it (currently none active, but the engine supports it):

```
[Mod1] → [Car1] ─┐
[Mod2] → [Car2] ─┼→ mix → out
[Mod3] → [Car3] ─┘
```

Each pair has independent ratio, depth, and output level. Pair 2 can be detuned for richness. Pair 3 typically uses a high ratio for metallic attack that decays with the spectral envelope.

## Envelope

Exponential ADSR with Q15 fixed-point rates:

- **Attack**: exponential approach to peak (fast initial ramp, smooth arrival)
- **Decay**: exponential decay to sustain level
- **Sustain**: held at sustain level
- **Release**: exponential decay to silence

Rates are precomputed from milliseconds using `1 - exp(-4.6/samples)` for ~99% completion in the specified time.

Snap threshold: 64 (~-54 dB) to avoid audible clicks at stage transitions.

### Spectral Modulation Envelope

Independent from the amplitude ADSR. Controls:
- FM modulation depth decay (brightness evolution on FM presets)
- Wavetable frame crossfading (timbre evolution on wavetable presets)
- Filter envelope amount

Also exponential, with per-preset `modDecayMs` controlling the rate.

## Filter

Per-voice resonant state-variable low-pass:
- Base cutoff from preset brightness
- Envelope amount from voice level
- Spectral envelope amount from modLevel
- Note-dependent brightness (+1 per 4 semitones above C3)
- Precomputed resonance damping (avoids per-sample division)

## Effects

Available master effects (one active at a time):

| Effect | Implementation |
|--------|---------------|
| Vibrato | LFO pitch modulation |
| Tremolo | LFO amplitude modulation |
| Chorus | Stereo modulated delay taps, offset LFO phases |
| Reverb | 4-tap stereo feedback delay network |
| Delay | Mono feedback delay (1024 samples) |
| Bitcrusher | 4-bit resolution, 4x downsample |

Per-preset default effect is auto-enabled when switching presets.

### Built-in Per-Preset Vibrato

Solo instruments (violin, clarinet, flute) have vibrato that runs independently of the global effect toggle. Precomputed phase step per voice (no float in hot path).

### Organ Simulation

For JazzOrg and FullOrg presets:
- **Key click**: broadband noise burst at note onset (~1ms linear decay)
- **Hammond percussion**: sine tone at 4x fundamental frequency, fast decay
- **Rotary speaker**: stereo amplitude + phase modulation (2.4 Hz jazz / 4.8 Hz full)

## Stereo

- Per-voice stereo panning: voices alternate across the field
- Stereo chorus and reverb preserve width
- Dithering (1-bit triangular) applied at output

## Performance Optimizations

- All wavetable + sine data in flash (zero RAM)
- Precomputed phase steps for LFO, metronome, rotary (no float in inner loop)
- All divisions replaced with bit shifts in the hot path
- Precomputed filter constants (baseCutoff, resonanceDamping) in recomputeEnvelope
- Inactive voices skipped before function call
- Conditional subtraction for reverb index wrapping (avoids modulo on non-power-of-2 sizes)
- Bitwise AND for power-of-2 buffer wrapping (delay, chorus)

## Rhythm Engine

16-step pattern sequencer with 4 synthesized drum voices:

- **Kick**: low oscillator with fast downward pitch envelope
- **Snare**: tonal body + noise burst
- **Hat**: high-passed short noise
- **Perc**: click/blip synthesized hit

8 rhythm patterns selectable via front-panel buttons. Tempo shared with metronome. Humanization jitter (0-31 samples) for natural feel.

Patterns are stored as compact bitmasks (8 bytes per pattern). Mixed into the output after voice rendering.

## USB MIDI

Auto-detects USB host connection via `MidiUSB.connected()`:

- **Computer connected**: MIDI-only mode (speaker muted, sends MIDI)
- **Power only**: standalone synth mode (speaker active, no MIDI)

MIDI output includes:
- Note on/off
- Sustain pedal (CC64, values 127/0)
- Volume (CC7)
- Program change on instrument select

Device name: "Synth Pico MIDI"

## Resource Usage

- Flash: 137 KB (6% of 2 MB)
- RAM: 21 KB (7% of 264 KB)
- CPU: 11% idle, ~19% peak (8 voices + organ rotary)

## Future Work

- **PCM sample playback**: real attack transients (1.9 MB flash available)
- **Velocity sensitivity**: map key velocity to amplitude and brightness
- **Preset persistence**: save settings to flash
- **Better speaker output**: headphone jack or better amplifier
