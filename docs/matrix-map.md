# Matrix Map

This document records the toy keyboard matrix discovered with the Pico mapper sketch.

## GPIO Wiring

Rows:

- `R0 -> GP0`
- `R1 -> GP1`
- `R2 -> GP2`
- `R3 -> GP3`
- `R4 -> GP4`
- `R5 -> GP5`
- `R6 -> GP14`
- `R7 -> GP15`
- `R8 -> GP16`

Columns:

- `C0 -> GP6`
- `C1 -> GP7`
- `C2 -> GP8`
- `C3 -> GP9`
- `C4 -> GP10`
- `C5 -> GP11`
- `C6 -> GP12`
- `C7 -> GP13`

## Piano Keys

The user confirmed these are the exact `37` keyboard keys in left-to-right order, starting on `C`.

### Physical Left-To-Right Matrix Order

```text
R8 C4
R8 C5
R8 C6
R8 C7
R8 C0
R8 C1
R8 C2
R8 C3

R5 C4
R5 C5
R5 C6
R5 C7
R5 C0
R5 C1
R5 C2
R5 C3

R4 C4
R4 C5
R4 C6
R4 C7
R4 C0
R4 C1
R4 C2
R4 C3

R3 C4
R3 C5
R3 C6
R3 C7
R3 C0
R3 C1
R3 C2
R3 C3

R2 C4
R2 C5
R2 C6
R2 C7
R2 C0
```

### Note Names Left To Right

```text
C  C#  D  D#  E  F  F#  G
G# A   A# B   C  C# D  D#
E  F   F# G   G# A  A# B
C  C#  D  D#  E  F  F# G
G# A   A# B   C
```

### Matrix To Note Name

```text
R8 C4 = C
R8 C5 = C#
R8 C6 = D
R8 C7 = D#
R8 C0 = E
R8 C1 = F
R8 C2 = F#
R8 C3 = G

R5 C4 = G#
R5 C5 = A
R5 C6 = A#
R5 C7 = B
R5 C0 = C
R5 C1 = C#
R5 C2 = D
R5 C3 = D#

R4 C4 = E
R4 C5 = F
R4 C6 = F#
R4 C7 = G
R4 C0 = G#
R4 C1 = A
R4 C2 = A#
R4 C3 = B

R3 C4 = C
R3 C5 = C#
R3 C6 = D
R3 C7 = D#
R3 C0 = E
R3 C1 = F
R3 C2 = F#
R3 C3 = G

R2 C4 = G#
R2 C5 = A
R2 C6 = A#
R2 C7 = B
R2 C0 = C
```

## Control Keys

### Instrument Selection Row

The user confirmed the instrument-selection buttons occupy:

```text
R1 C0
R1 C1
R1 C2
R1 C3
R1 C4
R1 C5
R1 C6
R1 C7
```

Button names are not assigned yet; only matrix positions are known.

### Rhythm Selection Row

The user confirmed the rhythm-selection buttons occupy:

```text
R7 C0
R7 C1
R7 C2
R7 C3
R7 C4
R7 C5
R7 C6
R7 C7
```

Button names are not assigned yet; only matrix positions are known.

### Value / Tempo Controls

The user confirmed these four buttons are the `Volume +/-` and `Tempo +/-` controls:

```text
R6 C4 = Volume Up
R0 C4 = Volume Down
R6 C3 = Tempo Up
R0 C3 = Tempo Down
```

### Animal Sound Controls

These buttons were confirmed in the user-provided order `dog`, `frog`, `duck`, `bird`, `drum/animal switch`:

```text
R6 C5 = Dog
R6 C6 = Frog
R6 C7 = Duck
R6 C0 = Bird
R6 C1 = Drum/Animal Switch
```

### Stop / Demo / Recorder Controls

These buttons were confirmed in the user-provided order `Stop`, `Demo A`, `Demo B`, `Demo C`, `Rec`, `Play`:

```text
R0 C5 = Stop
R0 C6 = Demo A
R0 C7 = Demo B
R0 C0 = Demo C
R0 C1 = Rec
R0 C2 = Play
```

### Synth V1 Repurposed Functions

For Synth V1, the above controls are repurposed as:

```text
R6 C5 = Bank Down
R6 C6 = Bank Up
R6 C7 = Octave Down
R6 C0 = Octave Up
R6 C1 = Sustain

R0 C5 = Panic (hold 2 s: feedback sounds on/off)
R0 C6 = Effect Select
R0 C7 = Effect On/Off
R0 C0 = Rhythm Start/Stop
R0 C1 = Transpose Down
R0 C2 = Transpose Up
```

The four remaining buttons (no toy label recorded) are rhythm controls:

```text
R2 C1 = Fill
R2 C2 = Sync Start
R2 C3 = Intro/Ending
R6 C2 = Leslie fast/slow
```
