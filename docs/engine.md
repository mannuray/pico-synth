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
- 8-voice polyphony; when all are busy a new note takes the quietest releasing voice, else the oldest note
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

Used by EPiano (body pair + a 14:1 "tine" pair), Bells and RingMod:

```
[Mod1] → [Car1] ─┐
[Mod2] → [Car2] ─┼→ mix → out
[Mod3] → [Car3] ─┘
```

Each pair has an independent ratio, depth and output level. Ratios are fractional (`fmRatioX100`, so 140 = 1.4), which allows inharmonic spectra: Chowning's bell uses carrier:modulator 5:7. Depth is about 10x the modulation index. Pair 2 can be detuned for richness. Pair 3 typically uses a high ratio for a metallic attack that decays with the spectral envelope.

## Envelope

Exponential ADSR with Q30 levels and Q20 rates (so decays of 20+ seconds are possible without 64-bit math):

- **Attack**: exponential approach to peak (fast initial ramp, smooth arrival)
- **Decay**: exponential decay to the sustain level. With `decay2Ms` set, it is two-stage like a real piano or guitar string: a quick drop to -12 dB, then a much slower tail.
- **Sustain**: held at sustain level. A voice whose sustain is 0 ends by itself once it decays to silence.
- **Release**: exponential decay to silence

Rates are precomputed from milliseconds using `1 - exp(-4.6/samples)` for ~99% completion in the specified time.

Snap threshold: ~-54 dB, to avoid audible clicks at stage transitions.

### Spectral Modulation Envelope

Independent from the amplitude ADSR. Controls:
- FM modulation depth decay (brightness evolution on FM presets)
- Wavetable frame crossfading (timbre evolution on wavetable presets)
- Filter envelope amount

It starts at full, then decays over `modDecayMs` to `modSustain` (0 = darkest). Sustained instruments settle at a high `modSustain`, so they keep their harmonics instead of fading to a near-sine; struck and plucked ones settle lower. With `modDecayMs = 0` it stays at full brightness.

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
| Vibrato | 5 Hz pitch wobble, about +-20 cents (relative to each note) |
| Tremolo | 5 Hz volume wobble, about +-2.5 dB |
| Chorus | Two ~9 ms modulated delay taps, one per side, plus slight pitch wobble |
| Reverb | Freeverb-style: 4 damped combs + 2 allpasses per side |
| Delay | ~186 ms echo with 45% feedback |
| Bitcrusher | 4-bit resolution, 4x downsample |

Choosing a preset selects its default effect and switches it on, unless the preset has `effectEnabled = false` (the basses and JazzOrg).

Each effect is a class in `Effects.h` with two optional hooks: per-voice modulation (relative pitch change or gain, computed once per frame) and in-place processing of the stereo master mix. To add an effect, write the class, add an `EffectType` entry and register it in `EffectRack`.

### Built-in Per-Preset Vibrato

Solo and section instruments have their own vibrato, independent of the effect: rate in Hz (`vibratoRateX10`), depth in cents (`vibratoCents`), fading in over `vibratoDelayMs` the way players start vibrato after the attack. Real violin vibrato is about 5-7 Hz and 30-60 cents peak-to-peak; the presets use ±12-22 cents (24-44 peak-to-peak). The depth is a fraction of each note's pitch, so it sounds the same across the keyboard.

### Organ Simulation

Presets with `OrganTraits` (JazzOrg) get:
- **Key click**: broadband noise burst at note onset (~1 ms)
- **Hammond percussion**: a sine at the 2nd or 3rd harmonic (4' or 2 2/3'), exponential decay (~0.2 s fast, ~1 s slow on a real B-3). Single-trigger like the real thing: it only sounds when no other key is held.
- **Rotary speaker (Leslie)**: a short modulated delay for the horn's Doppler pitch wobble plus amplitude modulation, the two sides 180 degrees apart. It runs after the effect, on the whole mix. Real Leslie horn speeds: ~0.8 Hz "chorale", ~6.7 Hz "tremolo"; JazzOrg uses tremolo.

ChurchOrg is a pipe organ: no click, percussion or rotary, just slow pipe speech and reverb.

## Stereo

- Per-voice stereo panning: voices alternate across the field
- Stereo chorus and reverb preserve width
- Dithering (1-bit triangular) applied at output

## Latency

The I2S output queue is `kI2SBufferCount` x `kI2SBufferFrames` = 6 x 64 stereo frames, about 8.7 ms from key press to sound (it was 8 x 256, about 46 ms). The main loop has to refill a 64-frame buffer every ~1.45 ms; if the audio ever crackles, raise the buffer count in `Config.h`.

## Performance

The audio budget is 1.45 ms per 64-sample block. The firmware runs the RP2040 at 250 MHz (a mild, widely used overclock; `sketch.yaml`). Measured on the Pico (`-O2`), with all 8 voices sounding, drums playing and the preset's effect on:

| Presets | 250 MHz | 200 MHz (stock) |
|---|---|---|
| Basses, Pluck, WahClav, Chiptune, RingMod | 42-47% | 52-58% |
| JazzOrg, EPiano, Bells | 50-57% | 63-72% |
| GrandPno, NylonGtr, Violin, Cello, Flute, Clarinet, Trumpet, Harmonica, SynthLead | 61-63% | 77-79% |
| Unison presets: ChurchOrg, Accordion, Strings, Choir, SpacePad | 79-81% | 98-101% |
| Brass | 89% (94% worst block) | 112% |

(The original engine needed 100-235% for the same test.) This is the worst case; typical 3-5 note playing is well below it. What keeps it there:

- **Both cores**: voices 4-7 render on core 1 (`Core1Worker`) while core 0 renders voices 0-3, then core 0 mixes and runs the effects, drums and output.
- **Block rendering at a control rate**: each voice renders a whole block; filter cutoff, spectral envelope, vibrato and effect modulation update every 8 samples (0.18 ms), the amplitude envelope every sample.
- **RAM**: the per-sample functions (`AUDIO_HOT`), the sine table and the wavetables run from RAM rather than through the flash cache.
- **`-O2`** via `sketch.yaml` (the core defaults to `-Os`).
- Fixed-point throughout; no divisions per voice per sample.

A benchmark sketch that times every preset on the device is a good way to re-check after engine changes; the serial `cpu=` field shows the live load in standalone mode.

## Rhythm Engine

A drum machine playing PCM samples (generated by `tools/generate_drums.py`, stored in flash at 44.1 kHz) on six voices: kick, snare, hi-hat (closed and open share a voice so each chokes the other, like a real hat), percussion, toms and crash.

### Patterns

Patterns are written like a drum-machine grid in `RhythmEngine.cpp`: one string per drum, one character per 16th-note step.

```
  .kick  = "X.......X.x.....",     X accent   x normal   g ghost   . rest
  .snare = "....X.......X...",     toms: H/M/L = high/mid/low (lowercase = softer)
  .hat   = "X.x.X.x.X.x.X.x.",
```

- **Any length**: a pattern can be several bars (Latin's son clave spans two) and a bar can be 16 steps (4/4) or 12 (3/4, Waltz).
- **Velocity**: accent, normal and ghost notes (~9 dB under normal), so funk ghost notes and soft cross-stick ballads work.
- **Swing**: MPC-style, delaying the off-beat 16ths (50% straight, 66% triplet shuffle). Funk uses 56%.
- **Fills**: optional one-bar fill lanes replace the normal ones on the last bar of every 4-bar phrase; a crash marks the next downbeat.
- **Tempo**: each pattern has a usual tempo, recalled when it is selected (Tempo Up/Down still adjusts it).
- **Humanization**: each hit starts 0-1.4 ms late at random, without moving the beat grid.

| Pattern | Tempo | Groove |
|---|---|---|
| Pop | 100 | Kick on 1, the "and" of 2 and 3; backbeat; 8th hats. Snare fill. |
| Rock | 112 | Kick on 1, 3 and the "and" of 3; accented quarter hats. Snare-and-tom fill, crash. |
| Ballad | 72 | Cross-stick on 2 and 4, soft 8th hats. Tom fill. |
| Disco | 120 | Four-on-the-floor kick, clap backbeat, closed 16ths with open hats on the "and"s. Clap roll fill. |
| Funk | 96 | Syncopated kick, ghost notes around the backbeat, accented 16th hats, open hat on the last "and"; 56% swing. |
| Latin | 105 | Son clave 3-2 over two bars, tumbao-style kick, shaker-like hats. No fill (the clave never breaks). |
| Waltz | 90 | 3/4 "oom-pah-pah". |
| March | 116 | Bass drum on every beat, snare drags into accented 2 and 4. Roll fill. |

### Mix

Each drum has a mix level (kick and snare lead, cymbals sit lower), and one drum-bus level (`kDrumBusGain`) sets the whole kit against the instruments. The drums are mixed **after** the preset's effect and the organ's rotary speaker, so they stay dry and sound the same on every preset (no echoing drums on Pluck, no swirling drums on JazzOrg). They then share the instruments' soft clipper rather than a hard clamp.

At the default volume, playing 6-note chords along with Rock, Disco or March keeps the soft clipper's share under 0.1% of samples (a test enforces it); drum peaks reach about -2.5 dB, their average level sits 5-10 dB under a held note.

Choosing another pattern while playing switches at the next bar line, like a drum machine. A swung or humanized hit is queued, so the previous hit on that drum keeps ringing until the new one actually starts.

## Controls

`PerformanceController` turns button presses into state changes and commands; `ControlHold` adds hold-to-repeat (Tempo, Volume, Transpose: after 0.5 s, every 0.1 s) and the 2-second long press on Panic. Button presses are confirmed by `FeedbackSounds`: short sine blips (up, down, center, limit, blip, N blips) mixed dry after the effects and drums, scaled with the volume but kept audible near volume 0. In MIDI mode the engine renders only while a blip plays, with the music muted. See the README for the full button list.

## USB MIDI

Auto-detects USB host connection via `MidiUSB.connected()`:

- **Computer connected**: MIDI-only mode (speaker muted, sends MIDI)
- **Power only**: standalone synth mode (speaker active, no MIDI)

A change must hold for `kModeSwitchDebounceMs` (500 ms) before the mode switches. When the synth becomes active it is re-sent the full performance state; the computer is sent nothing on connect.

MIDI output includes:
- Note on/off
- Sustain pedal (CC64, values 127/0)
- Volume (CC7)
- Program change on preset select, using the preset's General MIDI program (`gmProgram`); no bank select, since GM Level 1 has no banks
- Octave, transpose, effect and rhythm settings as CC 20-25 and 111

Device name: "Synth Pico MIDI"

## Resource Usage

- Flash: 476 KB (22% of 2 MB); the drum kit is ~330 KB of it
- RAM: ~121 KB (46%) with the wavetables copied into RAM
- CPU: see Performance above

## Future Work

- **PCM sample playback**: real attack transients (1.9 MB flash available)
- **Velocity sensitivity**: map key velocity to amplitude and brightness
- **Preset persistence**: save settings to flash
- **Better speaker output**: headphone jack or better amplifier
