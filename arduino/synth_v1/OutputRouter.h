#pragma once

#include "IEventSink.h"

enum class OutputMode : uint8_t {
  SynthOnly = 0,
  MidiOnly,
  SynthAndMidi,
  Debug
};

const char *outputModeName(OutputMode mode);

// Fans events and commands out to the sinks enabled by the current mode.
class OutputRouter final : public IEventSink {
public:
  OutputRouter(IEventSink &synthSink, IEventSink &midiSink, IEventSink &debugSink, OutputMode initialMode);

  // Sinks that stop receiving output get AllNotesOff first so no notes hang.
  // Returns true if the mode changed.
  bool setMode(OutputMode mode);
  OutputMode mode() const { return mode_; }
  bool synthActive() const;

  void handleEvent(const KeyboardEvent &event) override;
  void handleCommand(const PerformanceCommand &command) override;

private:
  static constexpr uint8_t kSinkCount = 3;

  IEventSink *const sinks_[kSinkCount];
  OutputMode mode_;

  static uint8_t sinkMask(OutputMode mode);
};
