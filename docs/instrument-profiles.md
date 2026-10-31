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

## The eight instruments

One per tone button, matching the labels printed on the keyboard (T1-T8).

### Piano (T1)
Fast drop to -12 dB in about 0.5 s, then a long tail; the tone darkens as it rings. Reverb.

### Organ (T2)
Hammond B-3 style: instant attack, key click, single-trigger percussion on the 3rd harmonic, Leslie rotary speaker. Pressing T2 again switches the Leslie between slow (chorale) and fast (tremolo). No extra effect by default.

### Violin (T3)
Bowed attack; stays bright while held; vibrato fading in after 250 ms.

### Trumpet (T4)
Bright attack that stays brassy while held; light vibrato late in the note.

### Banjo (T5)
Bright, short pluck with a fast two-stage decay.

### Bell (T6)
Chowning FM bell (5:7 ratio), long inharmonic ring.

### Music Box (T7)
High (an octave up), FM tine with a quick metallic attack and a ringing decay.

### Guitar (T8)
Nylon-string pluck with a two-stage decay that darkens as it rings.
