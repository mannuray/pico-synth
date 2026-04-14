# Key Matrix Mapper

This sketch is for the first bring-up step: identify which key closes which matrix row/column pair.

## Use

1. Open [key_matrix_mapper.ino](/Users/metallonudo/code/synth/arduino/key_matrix_mapper/key_matrix_mapper.ino).
2. Edit `ROW_PINS[]` to the Pico GPIOs wired to one side of the keyboard matrix.
3. Edit `COL_PINS[]` to the Pico GPIOs wired to the other side of the keyboard matrix.
4. Upload to Raspberry Pi Pico from Arduino IDE.
5. Open Serial Monitor at `115200`.
6. Press keys and note the reported `R# C#` pairs.

## Expected Output

```text
DOWN R2 C1  [GP4 x GP11]
UP   R2 C1  [GP4 x GP11]
```

That means the pressed key connects matrix row `2` to matrix column `1`.

## Important Notes

- The sketch assumes active-low scanning:
  - rows are driven `HIGH` when idle
  - one row at a time is driven `LOW`
  - columns use `INPUT_PULLUP`
  - pressed key reads as `LOW`
- If you get no events, the matrix side assignment may be reversed.
- If reversed, swap the wires so `ROW_PINS[]` are on the driven side and `COL_PINS[]` are on the sensed side.
- Start with the likely keybed matrix size we discussed, for example `8` rows and `4` columns.

## Recommended Pico Pins

Safe GPIOs for matrix scanning on a Pico:

- `GP2` to `GP15`
- avoid `GP0` and `GP1` at first if you want cleaner serial/debug workflows
- avoid ADC pins if you later want one for mic input

## Next Step

Once you tell me which Pico GPIO goes to which keyboard line and share the `R# C#` results for a few keys, I can convert that into the actual synth key map and fold it into the firmware.
