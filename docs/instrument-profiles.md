# Instrument Profiles

Sound-design intent for the 24 presets. The parameters live in `arduino/synth_v1/Presets.cpp`.

## Design Principles

- **Realistic over gimmicky.** Each acoustic preset aims at measured behaviour of the real instrument (vibrato rate and depth, decay shape, organ speeds), not just a recognisable label.
- **Keep the harmonics.** The spectral envelope starts bright and settles at `modSustain`. Sustained instruments settle high, so a held violin or trumpet stays a violin or trumpet; struck and plucked sounds settle lower, as they darken in real life.
- **Vibrato like a player.** Depth is in cents (`vibratoCents` is the ± swing; peak-to-peak is twice that) and fades in after the attack (`vibratoDelayMs`).
- **Level-matched.** Each preset's `gain` puts its loudest held moment at the same level (within 0.2 dB). Struck and plucked presets (piano, guitar, pluck) sit 2.5-6 dB lower on that measure so their sharp attacks stay below the soft clipper, even in chords; their peaks are as loud as the rest.
- **Distinct slots.** No two presets share both a sound source and a role.

## How the presets were checked

A host render (see `tests/host`) plays each preset at C3, C4 and C5 and measures loudness, harmonic content, decay times and vibrato depth/rate. Typical results after tuning:

| Measure | Before the rework | After |
|---|---|---|
| Loudness spread across presets | 16 dB | 0.2 dB (sustained); plucked/struck deliberately lower, see above |
| Energy in the fundamental, held violin / trumpet | 85% / 96% (nearly a sine) | 45% / 36% |
| Violin vibrato | under 1 cent (inaudible) | ±22 cents (44 peak-to-peak) at 6 Hz |
| Piano, time to fall 20 dB | 0.57 s | ~2 s |
| Pitch vs. MIDI note | one octave high | matches (A4 = 440 Hz) |

## Reference values

| Instrument behaviour | Real-world value | Used here |
|---|---|---|
| Violin vibrato | 5-7 Hz, 30-60 cents peak-to-peak (pedagogy suggests ~50) | 6 Hz, ±22 cents, 250 ms fade-in |
| Leslie horn speed | ~0.8 Hz chorale, ~6.7 Hz tremolo | JazzOrg 6.7 Hz |
| Hammond percussion | 2nd or 3rd harmonic; ~0.2 s fast, ~1 s slow | 3rd harmonic, 250 ms |
| Chowning FM bell | carrier:modulator 5:7, index up to 10, exponential decays | ratio 1.4, index ~8 |
| Piano decay | two-stage: fast "prompt sound", slow "aftersound" | 0.5 s to -12 dB, then ~7 s |

## Bank 1: Keys & Folk

### GrandPno (wavetable 0)
Fast drop to -12 dB in about 0.5 s, then a ~7 s tail. The tone darkens over 2.5 s but keeps some harmonics. Reverb.

### EPiano (FM)
Rhodes-style: a 1:1 FM body plus a 14:1 "tine" pair whose bite fades with the spectral envelope. Two-stage decay. Chorus.

### JazzOrg (wavetable 7)
Hammond B-3 style: instant attack, full sustain, key click, single-trigger percussion on the 3rd harmonic, fast Leslie. No extra effect by default; the Leslie is the effect.

### ChurchOrg (wavetable 12)
Pipe organ: 60 ms pipe speech, slight detune between ranks via unison, long release into reverb. No click, percussion or Leslie.

### Accordion (wavetable 11)
Reed wavetable with musette detuning (unison spread 9, about 15 cents). Steady bellows tone, no spectral decay.

### Harmonica (wavetable 5)
Blues harp: breathy onset, settles slightly darker, ±20-cent hand vibrato after 300 ms.

### NylonGtr (wavetable 2)
Pluck with a two-stage decay (0.25 s drop, ~3.5 s tail) that darkens as it rings.

### Bells (FM)
Chowning's bell: modulator at 1.4x the carrier, index ~8 at the strike, decaying with the tone. Long ring and release.

## Bank 2: Orchestral

### Violin (wavetable 1)
80 ms bowed attack; stays bright while held; 6 Hz, ±22-cent vibrato fading in after 250 ms.

### Cello (wavetable 6)
An octave below the key (cello range starts at C2). Slower 100 ms bow attack, 5.5 Hz, ±20-cent vibrato.

### Strings (wavetable 9)
Ensemble: soft-string wavetable with unison detune, 250 ms attack, long release, light section vibrato.

### Flute (wavetable 3)
Near-sinusoidal like a real flute, breathy attack, gentle ±15-cent vibrato.

### Clarinet (wavetable 10)
Odd-harmonic reed, stays reedy while held. No vibrato, as in classical clarinet playing.

### Trumpet (wavetable 4)
Bright 30 ms attack that stays brassy while held; ±15-cent vibrato late in the note.

### Brass (wavetable 4)
Section version: unison, slower 70 ms swell, a little darker than the solo trumpet.

### Choir (wavetable 8)
Vocal pad: unison, 250 ms attack, light vibrato.

## Bank 3: Synth

### SynthLead (saw)
Detuned three-saw unison, resonant filter that settles after the attack, delayed vibrato, delay effect.

### SpacePad (wavetable 13)
Very slow attack (0.9 s), wide unison, a 3 s downward filter sweep, chorus.

### FunkBass (saw, octave down)
High resonance with a fast spectral envelope: each note "quacks", then settles. Dry.

### SubBass (sine, octave down)
Pure low fundamental. Dry.

### Pluck (saw)
Short resonant pluck with a quick two-stage decay, into the delay.

### WahClav (saw)
Funky clav with a fast resonant filter sweep on every note. Dry.

### Chiptune (square)
Open square wave with the delayed vibrato typical of 8-bit game music, through the bitcrusher.

### RingMod (FM)
Non-integer ratio (3.53) for an inharmonic, metallic tone; moderate index so it's harsh but not noise. Delay.
