#pragma once

#include "IEventSink.h"
#include "KeyboardState.h"
#include "MatrixMap.h"
#include "MatrixScanner.h"
#include "MicInput.h"
#include "PerformanceState.h"
#include "SynthEngine.h"

class AppController {
public:
  enum class OutputMode : uint8_t {
    SynthOnly = 0,
    MidiOnly,
    SynthAndMidi,
    Debug
  };

  AppController(MatrixScanner &matrixScanner,
                MicInput &micInput,
                SynthEngine &synthEngine,
                IEventSink &synthSink,
                IEventSink &midiSink,
                IEventSink &debugSink);

  void begin();
  void tick();
  void printStatus() const;

private:
  MatrixScanner &matrixScanner_;
  MicInput &micInput_;
  SynthEngine &synth_;
  IEventSink &synthSink_;
  IEventSink &midiSink_;
  IEventSink &debugSink_;
  KeyboardState keyboardState_;
  PerformanceState performanceState_;
  uint16_t micLevel_ = 0;
  OutputMode outputMode_ = OutputMode::SynthAndMidi;
  uint32_t lastScanMs_ = 0;
  uint32_t lastStatusMs_ = 0;
  uint32_t lastLedMs_ = 0;
  uint32_t ledFlashUntilMs_ = 0;
  uint32_t lastMeasurementUs_ = 0;
  uint32_t totalProcessingUs_ = 0;
  uint16_t cpuLoad_ = 0;
  int8_t attackOffset_ = 0;
  int8_t decayOffset_ = 0;
  int8_t sustainOffset_ = 0;
  int8_t releaseOffset_ = 0;

  void updatePreset();
  void releaseSustainedNotes();
  void processMatrix();
  void handleNoteEvent(const MatrixBinding &binding, bool pressed);
  void handleControlPress(const MatrixBinding &binding);
  void updateStatusLed(uint32_t now);
  void dispatchControlCommand(ControlType control, uint8_t value);
  void dispatchEvent(const KeyboardEvent &event);
  void dispatchCommand(const PerformanceCommand &command);
  static const char *modeName(OutputMode mode);
  static const char *effectName(EffectType effect);
};
