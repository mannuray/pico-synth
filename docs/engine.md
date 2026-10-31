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

- **Sample** — recorded instrument (see below); all 16 presets use it
- **Triangle** — simple phase-based
- **Saw** — PolyBLEP anti-aliased
- **Square** — PolyBLEP anti-aliased (both edges)
- **Sine** — 512-sample table with linear interpolation (built in RAM at startup)
- **FM** — 3-pair Algorithm 5 style (body + richness + tine/bell)

### Sampled Instruments

Each preset names a sample set (`Preset::sampleSet`): up to 6 key zones of FluidR3_GM recordings, each with a key range, root key, tuning and loop (`SampleSet.h`), cut by `tools/extract_sf2.py` and stored in flash at 22.05 kHz. A note picks the zone covering its key (after the preset octave) and plays it at `2^((note - root + tune/100) / 12)` times speed, a Q20.12 position stepping through the data with linear interpolation. Vibrato and the effects' pitch modulation scale that step, as they do an oscillator's. After the loop end it wraps to the loop start; one guard sample after the data repeats the loop start, so interpolation never reads past the zone.

The recordings carry the attack and timbre; the preset's envelope and filter shape the rest (a long sustained loop becomes a piano's two-stage decay). Pitch was checked against FluidSynth rendering the same SoundFont (within 5 cents). Two FluidR3 details: its Drawbar Organ is pitched two octaves low (16' foot), so the extractor retunes it to the key (`TUNE_CENTS`), which keeps the organ percussion and filter tracking on the sounding pitch; and its stereo samples' links are all broken (they point at sample 0, "Gun"), so the extractor pairs left and right by name.

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

Choosing a preset selects its default effect and switches it on, unless the preset has `effectEnabled = false` (the Organ).

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

| Presets | Average | Worst block |
|---|---|---|
| Organ (no master effect; rotary instead) | 47% | 53% |
| Violin, Trumpet, Bell | 55-56% | 61-62% |
| Piano, Banjo, Music Box, Guitar | 58% | 63% |

The effect and drums alone (no voices) take 25%. Sample playback streams from flash through the XIP cache; it costs about the same as the synthesized voices it replaced.

(The original synthesized engine needed 100-235% for the same test.) This is the worst case; typical 3-5 note playing is well below it. What keeps it there:

- **Both cores**: voices 4-7 render on core 1 (`Core1Worker`) while core 0 renders voices 0-3, then core 0 mixes and runs the effects, drums and output.
- **Block rendering at a control rate**: each voice renders a whole block; filter cutoff, spectral envelope, vibrato and effect modulation update every 8 samples (0.18 ms), the amplitude envelope every sample.
- **RAM**: the per-sample functions (`AUDIO_HOT`) and the sine table run from RAM rather than through the flash cache.
- **`-O2`** via `sketch.yaml` (the core defaults to `-Os`).
- Fixed-point throughout; no divisions per voice per sample.

A benchmark sketch that times every preset on the device is a good way to re-check after engine changes; the serial `cpu=` field shows the live load in standalone mode.

## Rhythm Engine

A drum machine playing PCM samples (the FluidR3_GM standard kit, cut by `tools/extract_sf2.py`, stored in flash at 44.1 kHz; `tools/generate_drums.py` makes the older synthesized kit) on six voices: kick, snare, hi-hat (closed and open share a voice so each chokes the other, like a real hat), percussion, toms and crash.

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

| Pattern | Tempo | Grid | Groove |
|---|---|---|---|
| Rock | 112 | 16ths | Kick on 1, 3 and the "and" of 3; accented quarter hats. Snare-and-tom fill, crash. |
| Slow Rock | 66 | triplets | 12/8 feel: all triplets on the hat, kick 1 and 3 with a pickup. Tom fill. |
| Twist | 156 | 16ths | Early-60s kick/snare backbeat, 8th hats. Snare fill. |
| Disco | 120 | 16ths | Four-on-the-floor kick, clap backbeat, closed 16ths with open hats on the "and"s. |
| March | 116 | 16ths | Bass drum on every beat, snare drags into accented 2 and 4. Roll fill. |
| Waltz | 90 | 16ths, 3/4 | "Oom-pah-pah". |
| Samba | 100 | 16ths | Surdo-style double kick, even shaker-like hat 16ths, cross-stick figure, 54% swing. |
| Blues | 96 | triplets | Shuffle: hat on each beat and its last triplet, kick 1 and 3, snare 2 and 4. Tom fill. |

`stepsPerBeat` sets the grid: 4 for 16ths, 3 for triplets (a shuffle's long-short feel). The Samba cross-stick figure and the Twist groove are approximations worth tuning by ear.

### Mix

Each drum has a mix level (kick and snare lead, cymbals sit lower), and one drum-bus level (`kDrumBusGain`) sets the whole kit against the instruments. The drums are mixed **after** the preset's effect and the organ's rotary speaker, so they stay dry and sound the same on every preset (no echoing drums on Pluck, no swirling drums on JazzOrg). They then share the instruments' soft clipper rather than a hard clamp.

At the default volume, playing 6-note chords along with Rock, Disco or March keeps the soft clipper's share under 0.1% of samples (a test enforces it). With the FluidR3 kit the patterns average about -24 to -25 dBFS with peaks of -4 to -6 dBFS: about 3 dB under a held organ note and 8 dB under a violin.

Choosing another pattern while playing switches at the next bar line, like a drum machine. A swung or humanized hit is queued, so the previous hit on that drum keeps ringing until the new one actually starts.

## Controls

`PerformanceController` turns button presses into state changes and commands; `ControlHold` adds hold-to-repeat (Tempo, Volume, Transpose: after 0.5 s, every 0.1 s) and the 2-second long press on Panic. Button presses are confirmed by `FeedbackSounds`: short sine blips (up, down, center, limit, blip, N blips) mixed dry after the effects and drums, scaled with the volume but kept audible near volume 0. In MIDI mode the engine renders only while a blip plays, with the music muted. See the README for the full button list. The bank button arms bank 1 or 2 for the tone buttons (`PerformanceState` keeps the armed bank apart from the playing preset, so the sound only changes on the next tone button); pressing the current rhythm's button again is Fill.

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

- Flash: 1,966 KB (96% of 2 MB); the instrument samples are ~1.5 MB of it, the drum kit ~0.4 MB
- RAM: ~62 KB (24%)
- CPU: see Performance above

## Future Work

- **Velocity sensitivity**: map key velocity to amplitude and brightness
- **Preset persistence**: save settings to flash
- **Better speaker output**: headphone jack or better amplifier
