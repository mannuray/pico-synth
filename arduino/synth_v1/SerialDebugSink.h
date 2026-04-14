#pragma once

#include "IEventSink.h"

class SerialDebugSink final : public IEventSink {
public:
  void handleEvent(const KeyboardEvent &event) override;
  void handleCommand(const PerformanceCommand &command) override;
};
