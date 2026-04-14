#pragma once

#include "IEventSink.h"

class MidiEventSink final : public IEventSink {
public:
  void handleEvent(const KeyboardEvent &event) override;
  void handleCommand(const PerformanceCommand &command) override;
};
