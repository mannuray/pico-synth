#pragma once

#include "IEventSink.h"

enum class OutputMode : uint8_t {
  SynthOnly = 0,  // no computer connected: play through the speaker
  MidiOnly        // computer connected: send USB MIDI, speaker silent
};

const char *outputModeName(OutputMode mode);

// Sends events and commands to the sink for the current mode.
class OutputRouter final : public IEventSink {
public:
  OutputRouter(IEventSink &synthSink, IEventSink &midiSink, OutputMode initialMode);

  // The sink being switched away from gets AllNotesOff first so no notes hang.
  // Returns true if the mode changed.
  bool setMode(OutputMode mode);
  OutputMode mode() const { return mode_; }
  bool synthActive() const { return mode_ == OutputMode::SynthOnly; }

  void handleEvent(const KeyboardEvent &event) override;
  void handleCommand(const PerformanceCommand &command) override;

private:
  IEventSink &synthSink_;
  IEventSink &midiSink_;
  OutputMode mode_;

  IEventSink &activeSink() const;
};
