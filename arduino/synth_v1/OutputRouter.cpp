#include "OutputRouter.h"

const char *outputModeName(OutputMode mode) {
  switch (mode) {
    case OutputMode::SynthOnly:
      return "Synth";
    case OutputMode::MidiOnly:
      return "Midi";
  }
  return "Unknown";
}

OutputRouter::OutputRouter(IEventSink &synthSink, IEventSink &midiSink, OutputMode initialMode)
    : synthSink_(synthSink), midiSink_(midiSink), mode_(initialMode) {}

bool OutputRouter::setMode(OutputMode mode) {
  if (mode == mode_) {
    return false;
  }
  activeSink().handleCommand(PerformanceCommand{CommandType::AllNotesOff, 0});
  mode_ = mode;
  return true;
}

void OutputRouter::handleEvent(const KeyboardEvent &event) {
  activeSink().handleEvent(event);
}

void OutputRouter::handleCommand(const PerformanceCommand &command) {
  activeSink().handleCommand(command);
}

IEventSink &OutputRouter::activeSink() const {
  return synthActive() ? synthSink_ : midiSink_;
}
