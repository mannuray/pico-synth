# Instrument Profiles

Sound-design intent for the 8 instruments. The parameters live in `arduino/synth_v1/Presets.cpp`; the recordings come from the FluidR3_GM SoundFont (MIT licence), cut by `tools/extract_sf2.py`.

## Design Principles

- **Recorded, not modelled.** Each instrument plays real recordings of the same General MIDI program the keyboard sends over USB, so the Pico on its own and a computer running FluidSynth with FluidR3 sound alike (except the organ's octave, below).
- **Several zones per instrument.** Up to 6 recordings spread across the keyboard, so low and high notes aren't one sample stretched over five octaves.
- **The envelope finishes the note.** Recordings are cut short to fit in flash (0.5-1.6 s) and looped; the preset's envelope gives the decay its real length (a piano rings for ~7 s, a bell for ~8 s).
- **Level-matched.** Each preset's `gain` brings a 3-note chord's peak to roughly the same level (13-17k of 32767), below the soft clipper's knee even with the drums playing (tests enforce this). Sustained sounds (organ, violin) sit a little lower so their average level doesn't dominate.

## How the presets were checked

- **Pitch**: rendered by the engine and by FluidSynth from the same SoundFont, compared by spectral peak: within 5 cents. A host test checks every preset's notes at 45, 57, 69 and 81 within 10 cents, with no louder component an octave or two below.
- **Coverage**: every key from MIDI 12 to 108 sounds on every preset; every zone's loop is inside its data and its seam is continuous.

## The eight instruments

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

## Reference values

| Behaviour | Real-world value | Used here |
|---|---|---|
| Leslie horn speed | ~0.8 Hz chorale, ~6.7 Hz tremolo | 0.8 / 6.7 Hz |
| Hammond percussion | 2nd or 3rd harmonic; ~0.2 s fast, ~1 s slow | 3rd harmonic, 250 ms |
| Piano decay | two-stage: fast "prompt sound", slow "aftersound" | 1.5 s, then ~7 s |
