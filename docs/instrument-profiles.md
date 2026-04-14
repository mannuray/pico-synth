# Instrument Profiles

This document describes the sound design intent for each preset across all 3 banks.

## Design Principles

- each preset must occupy a clearly different musical slot
- different synthesis methods (wavetable, FM, saw, square, sine, triangle) create the most distinct sounds
- multi-frame wavetable scanning gives time-varying timbre that makes instruments sound alive
- the resonant filter shapes character — high resonance for nasal/funky sounds, low for warm/natural
- built-in vibrato on solo instruments (violin, clarinet, flute) without needing the global effect

## Bank 1: Acoustic / Traditional

### GrandPno (WT0: GrandPno)
- warm acoustic piano with inharmonicity and hammer noise in the wavetable
- fast attack, 900ms decay to silence, no sustain
- 600ms spectral decay: bright strike mellows into warm body
- default effect: reverb

### NylonGtr (WT2: NylonGtr)
- plucked nylon guitar with finger noise in the wavetable
- very fast attack, 480ms decay, no sustain — dies out like a real pluck
- 250ms spectral decay: bright pluck goes dark quickly
- high resonance (130) for woody body peak
- default effect: reverb

### Violin (WT1: Violin)
- solo bowed string with 2nd harmonic strongest (real violin trait)
- body formants at 300/1200/2700 Hz baked into wavetable
- 18ms attack, sustained, built-in vibrato (rate 120, depth 24)
- 600ms spectral decay for bow attack evolution
- default effect: reverb

### PanFlute (WT3: Flute)
- breathy flute with strong breath noise in the wavetable
- slow 45ms attack (breath onset), sustained
- built-in vibrato (rate 60, depth 14)
- low resonance (26) for pure airy character
- default effect: reverb

### JazzOrg (WT7: Organ)
- Hammond-style jazz organ with 6-harmonic drawbar model
- instant attack, full sustain, fast release
- key click noise burst at note onset
- Hammond percussion (sine at 4x fundamental)
- rotary speaker simulation (2.4 Hz)
- static timbre (no spectral decay — organs don't change brightness)

### FullOrg (WT12: PipeOrgan)
- full pipe organ with 8-harmonic registration
- wider unison detune (24) for ensemble width
- brighter than JazzOrg (brightness 248)
- rotary speaker simulation (4.8 Hz, more wet)

### Accordion (WT11: AccordionReed)
- reed accordion with all-harmonic sawtooth-like wavetable + reed noise
- musette detuning via unison (detune 14)
- high resonance (110) for nasal reed character
- 500ms spectral decay for subtle brightness evolution

### Clarinet (WT10: Reed)
- odd-harmonic reed spectrum (1, 3, 5, 7, 9)
- 22ms attack (reed onset), sustained
- built-in vibrato (rate 80, depth 18)
- 500ms spectral decay

## Bank 2: Synth / Electronic

### SpacePad (WT8: Choir)
- huge evolving sci-fi pad
- very slow attack (300ms), wide unison (detune 30)
- 1200ms spectral decay for long timbre evolution
- default effect: chorus

### FunkBass (Saw)
- squelchy resonant bass with PolyBLEP anti-aliased sawtooth
- resonance 240 — the filter sweep IS the sound
- 120ms spectral decay: each note quacks then settles
- default effect: delay

### SynthLead (WT13: SoftLead)
- fat analog-style lead with pulse-saw hybrid wavetable
- unison (detune 16) + built-in vibrato (rate 80, depth 16)
- default effect: chorus

### BellTone (FM ratio 5)
- clean glass bell with gentle FM synthesis
- 1400ms decay, very low sustain — long ring
- low FM depth (36) so it's gentle, not harsh
- default effect: reverb

### BitOrgan (Square)
- lo-fi chiptune organ, raw square wave
- instant attack, full sustain — retro 8-bit feel
- very low brightness (60), minimal resonance
- default effect: delay

### WahClav (Saw)
- funky auto-wah clavinet
- resonance 220 with 150ms spectral decay = rapid filter sweep per note
- default effect: delay

### Harmonica (WT5: Harmonica)
- blues harp with odd-heavy harmonics and reed/breath noise
- built-in vibrato (rate 100, depth 22)
- default effect: reverb

### Cello (WT6: Cello)
- deep bowed string with body formants
- slow attack (30ms), built-in vibrato (rate 90, depth 28)
- 700ms spectral decay
- default effect: reverb

## Bank 3: Experimental / Wild

### SubBass (Triangle)
- deep sub bass, felt more than heard
- very low brightness (30), zero resonance
- no spectral modulation — pure fundamental

### Siren (Sine)
- wobbling pitch siren with extreme vibrato (rate 255, depth 120)
- full sustain, pure sine — the vibrato IS the instrument
- default effect: delay

### Trumpet (WT4: Trumpet)
- warm brass with strong mid harmonics and mute resonance formants
- built-in vibrato (rate 70, depth 12)
- 300ms spectral decay for attack brightness
- default effect: reverb

### Choir (WT8: Choir)
- vocal pad with formant wavetable
- slow attack (200ms), unison (detune 14)
- built-in vibrato (rate 50, depth 12)
- default effect: reverb

### Pluck (Saw)
- sharp percussive synth pluck
- 100ms decay, no sustain — dies almost instantly
- high resonance (200) for a bright snap
- default effect: delay

### Vox (WT8: Choir)
- haunting vocal synth
- high resonance (160) creates vowel-like formant shapes
- unison (detune 12) + built-in vibrato
- 800ms spectral decay for slow timbre evolution
- default effect: reverb

### Brass (WT4: Trumpet)
- punchy synth brass stab
- bright attack (200) that closes in 200ms — snap then settle
- default effect: reverb

### RingMod (FM ratio 8)
- metallic industrial sound
- FM ratio 8 with feedback 20 — intentionally inharmonic
- no spectral decay — stays harsh
- default effect: delay
