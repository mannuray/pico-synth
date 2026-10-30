# Synth V1

Main firmware for the Pico Synth. See the [project README](../../README.md) for full documentation.

## Quick Build

```bash
arduino-cli compile -b rp2040:rp2040:rpipico arduino/synth_v1
arduino-cli upload -b rp2040:rp2040:rpipico -p /dev/cu.usbmodem2101 arduino/synth_v1
```

## File Overview

| File | Purpose |
|------|---------|
| synth_v1.ino | Entry point; creates and wires all components |
| AppController.h/cpp | Main loop: scan/render/status scheduling, USB mode detection |
| PerformanceController.h/cpp | Turns key and button presses into state changes and output commands |
| PerformanceState.h/cpp | Bank, preset, effect, sustain, tempo, volume, envelope offsets (single source of truth) |
| PitchSettings.h/cpp | Octave and transpose; key index to MIDI note |
| KeyTracker.h/cpp | Which keys are held or still sounding (sustain) |
| OutputRouter.h/cpp | Sends events/commands to the sinks enabled by the output mode |
| IEventSink.h | Output sink interface |
| SynthEventSink.h/cpp | Applies events/commands to the synth engine |
| MidiEventSink.h/cpp | USB MIDI output (notes, CC, program change) |
| SerialDebugSink.h/cpp | Serial monitor debug output |
| KeyboardEvent.h, PerformanceCommand.h | Note events and typed performance commands |
| SynthEngine.h/cpp | Voice allocation, mixing, effect/rotary routing, audio output |
| Voice.h/cpp | One note: oscillators (plain/unison/FM), envelope, filter, organ percussion and click |
| VoiceShape.h/cpp | Per-preset envelope and filter constants, with the ADSR offsets applied |
| Oscillator.h/cpp | Waveforms and wavetable lookup (the only file that includes WaveTableData.h) |
| RotarySpeaker.h/cpp | Leslie-style rotary speaker for the organ presets |
| Effects.h/cpp | Master effects (one class each) and the effect rack |
| Dsp.h | Shared fixed-point helpers (table lookup, triangle, soft clip, noise) |
| SynthTypes.h | Waveform, EffectType, Preset types |
| Presets.h/cpp | Preset table |
| RhythmEngine.h/cpp, RhythmPatterns.h | Drum pattern sequencer and pattern table |
| WaveTableData.h | Pre-generated wavetable data (14 instruments, 4 frames each) |
| Config.h | Hardware pins, sample rate, polyphony, bank configuration |
| MatrixMap.h | Key matrix to note/control mapping |
| MatrixScanner.h/cpp | Matrix scanning with debounce (implements IKeyMatrix) |
| MicInput.h/cpp | Mic level input (implements ILevelInput) |
| I2SAudioOutput.h/cpp | I2S stereo audio output via PIO |
| ArduinoPlatform.h/cpp | Clock, USB host detection and status LED for Arduino |
| IClock.h, IHostLink.h, IStatusIndicator.h, IKeyMatrix.h, ILevelInput.h, IAudioRenderer.h | Hardware interfaces used by AppController |
| CpuMeter.h, StatusReporter.h/cpp | CPU load measurement and periodic status line |
