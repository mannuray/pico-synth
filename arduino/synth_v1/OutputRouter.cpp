#include "OutputRouter.h"

namespace {
// Bit positions match the order of OutputRouter::sinks_.
constexpr uint8_t kSynthBit = 1u << 0;
constexpr uint8_t kMidiBit = 1u << 1;
constexpr uint8_t kDebugBit = 1u << 2;
}  // namespace

const char *outputModeName(OutputMode mode) {
  switch (mode) {
    case OutputMode::SynthOnly:
      return "Synth";
    case OutputMode::MidiOnly:
      return "Midi";
    case OutputMode::SynthAndMidi:
      return "Both";
    case OutputMode::Debug:
      return "Debug";
  }
  return "Unknown";
}

OutputRouter::OutputRouter(IEventSink &synthSink, IEventSink &midiSink, IEventSink &debugSink, OutputMode initialMode)
    : sinks_{&synthSink, &midiSink, &debugSink}, mode_(initialMode) {}

bool OutputRouter::setMode(OutputMode mode) {
  if (mode == mode_) {
    return false;
  }

  const uint8_t deactivated = sinkMask(mode_) & ~sinkMask(mode);
  for (uint8_t i = 0; i < kSinkCount; ++i) {
    if (deactivated & (1u << i)) {
      sinks_[i]->handleCommand(PerformanceCommand{CommandType::AllNotesOff, 0});
    }
  }

  mode_ = mode;
  return true;
}

bool OutputRouter::synthActive() const {
  return (sinkMask(mode_) & kSynthBit) != 0;
}

void OutputRouter::handleEvent(const KeyboardEvent &event) {
  const uint8_t mask = sinkMask(mode_);
  for (uint8_t i = 0; i < kSinkCount; ++i) {
    if (mask & (1u << i)) {
      sinks_[i]->handleEvent(event);
    }
  }
}

void OutputRouter::handleCommand(const PerformanceCommand &command) {
  const uint8_t mask = sinkMask(mode_);
  for (uint8_t i = 0; i < kSinkCount; ++i) {
    if (mask & (1u << i)) {
      sinks_[i]->handleCommand(command);
    }
  }
}

uint8_t OutputRouter::sinkMask(OutputMode mode) {
  switch (mode) {
    case OutputMode::SynthOnly:
      return kSynthBit;
    case OutputMode::MidiOnly:
      return kMidiBit;
    case OutputMode::SynthAndMidi:
      return kSynthBit | kMidiBit;
    case OutputMode::Debug:
      return kDebugBit;
  }
  return 0;
}
