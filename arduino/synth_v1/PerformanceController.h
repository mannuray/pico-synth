#pragma once

#include "ControlType.h"
#include "IEventSink.h"
#include "KeyTracker.h"
#include "PerformanceState.h"
#include "PitchSettings.h"

// Turns key and button presses into state changes, and forwards those
// changes to the output as note events and performance commands.
class PerformanceController {
public:
  PerformanceController(IEventSink &output, Print &log);

  // Loads the first preset's defaults into the state (sends nothing).
  void begin();

  // Returns true when the press started a new note.
  bool handleKey(uint8_t keyIndex, bool pressed);
  void handleControl(ControlType control, uint8_t value);

  // Sends the full state, e.g. when the synth becomes an output.
  void syncOutputs();

  const PerformanceState &performance() const { return performance_; }
  const PitchSettings &pitch() const { return pitch_; }

private:
  IEventSink &output_;
  Print &log_;
  KeyTracker keys_;
  PitchSettings pitch_;
  PerformanceState performance_;

  // Key handling
  bool pressKey(uint8_t keyIndex);
  void releaseKey(uint8_t keyIndex);
  void releaseSustainedNotes();

  // One handler per button action
  void selectInstrument(uint8_t slot);
  void stepBank(int8_t direction);
  void stepOctave(int8_t direction);
  void stepTranspose(int8_t direction);
  void stepVolume(int8_t direction);
  void stepTempo(int8_t direction);
  void toggleSustain();
  void toggleEffect();
  void nextEffect();
  void toggleRhythm();
  void selectRhythm(uint8_t pattern);
  void cycleEnvelopeOffset(EnvelopeParam param);
  void panic();

  // Presets
  void loadPresetDefaults();
  void applyActivePreset();
  void sendPresetState();

  // Output
  void send(CommandType type, int16_t value);
  void sendQuiet(CommandType type, int16_t value);
  void sendNote(KeyboardEventType type, uint8_t note);
  void logNote(const char *label, uint8_t keyIndex, uint8_t midiNote);
  void logPreset();
  static CommandType envelopeCommand(EnvelopeParam param);
};
