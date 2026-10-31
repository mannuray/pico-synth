# Instrument Profiles

Sound-design intent for the 16 instruments (two banks of 8). The parameters live in `arduino/synth_v1/Presets.cpp`; the recordings come from the FluidR3_GM SoundFont (MIT licence), cut by `tools/extract_sf2.py`.

## Design Principles

- **Recorded, not modelled.** Each instrument plays real recordings of the same General MIDI program the keyboard sends over USB, so the Pico on its own and a computer running FluidSynth with FluidR3 sound alike (except the organ's octave, below).
- **Several zones per instrument.** Up to 6 recordings spread across the keyboard, so low and high notes aren't one sample stretched over five octaves.
- **The envelope finishes the note.** Recordings are cut short to fit in flash (0.5-1.6 s) and looped; the preset's envelope gives the decay its real length (a piano rings for ~7 s, a bell for ~8 s).
- **Level-matched.** Each preset's `gain` brings a 3-note chord's peak to roughly the same level (11-17k of 32767), below the soft clipper's knee even with the drums playing (tests enforce this). Sustained sounds (organ, violin, strings, flute, sax, clarinet) share one average level, about -18.5 dBFS for the chord, so none dominates.

## How the presets were checked

- **Pitch**: rendered by the engine and by FluidSynth from the same SoundFont, compared by spectral peak: within 5 cents where both play the same recording. A host test checks every preset's notes at 45, 57, 69 and 81: bank 1 within 10 cents, bank 2 within 20, since a few of its recordings are out of tune at their own root key (Vibraphone F#6 +16, Sax F#3 -12, Steel Drum F#5 -11 cents; FluidSynth plays them the same).
- **Coverage**: every key from MIDI 12 to 108 sounds on every preset; every zone's loop is inside its data and its seam is continuous.

## Bank 1

One per tone button, matching the labels printed on the keyboard (T1-T8). The GM program is what the keyboard sends as Program Change. All but the Organ start with reverb on.

| Button | Instrument | GM program | Zones | Envelope |
|---|---|---|---|---|
| T1 | Piano | 1 Acoustic Grand Piano | 6 | two-stage decay: 1.5 s, then ~7 s |
| T2 | Organ | 17 Drawbar Organ | 1 | held; key click, percussion, Leslie |
| T3 | Violin | 41 Violin | 4 | 40 ms bowed attack, held |
| T4 | Trumpet | 57 Trumpet | 1 | 10 ms attack, held |
| T5 | Banjo | 106 Banjo | 5 | two-stage decay: 0.6 s, then 1.8 s |
| T6 | Bell | 15 Tubular Bells | 1 | two-stage decay: 2 s, then 8 s; long release |
| T7 | Music Box | 11 Music Box | 4 | two-stage decay: 0.8 s, then 2.5 s |
| T8 | Guitar | 25 Acoustic Guitar (nylon) | 5 | two-stage decay: 1 s, then 3.5 s |

### Organ (T2)
FluidR3's drawbar organ is pitched two octaves low (a 16' stop), which a small speaker can't reproduce, so the extractor retunes it to sound at the key (`TUNE_CENTS` in `tools/extract_sf2.py`). The USB MIDI notes are unchanged, so FluidSynth on a computer plays the same notes two octaves lower. On top of the recording: a key click, single-trigger percussion on the 3rd harmonic (250 ms) and the Leslie rotary speaker (0.8 Hz chorale, 6.7 Hz tremolo). Pressing T2 again switches the Leslie speed. No extra effect by default.

## Bank 2

Press the bank button (Dog), then a tone button. Bank 2 instruments use 2-3 zones of 0.6-1 s each, as only ~400 KB of flash was left for them; zones are chosen near the keyboard's range.

| Button | Instrument | GM program | Zones | Envelope / effect |
|---|---|---|---|---|
| T1 | E.Piano | 5 Electric Piano 1 | 2 | two-stage decay: 1.2 s, then 5 s; chorus |
| T2 | Strings | 49 String Ensemble 1 | 2 | 120 ms swell, held |
| T3 | Flute | 74 Flute | 2 | 30 ms attack, held |
| T4 | Sax | 66 Alto Sax | 2 | 15 ms attack, held |
| T5 | Clarinet | 72 Clarinet | 3 | 20 ms attack, held |
| T6 | Harmonica | 23 Harmonica | 3 | 15 ms attack, held |
| T7 | Vibraphone | 12 Vibraphone | 3 | two-stage decay: 1.5 s, then 5 s; tremolo (its motor) |
| T8 | Steel Drum | 115 Steel Drums | 3 | two-stage decay: 0.6 s, then 2 s |

## Reference values

| Behaviour | Real-world value | Used here |
|---|---|---|
| Leslie horn speed | ~0.8 Hz chorale, ~6.7 Hz tremolo | 0.8 / 6.7 Hz |
| Hammond percussion | 2nd or 3rd harmonic; ~0.2 s fast, ~1 s slow | 3rd harmonic, 250 ms |
| Piano decay | two-stage: fast "prompt sound", slow "aftersound" | 1.5 s, then ~7 s |
