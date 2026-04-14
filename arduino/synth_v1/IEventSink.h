#pragma once

#include "KeyboardEvent.h"
#include "PerformanceCommand.h"

class IEventSink {
public:
  virtual ~IEventSink() = default;
  virtual void handleEvent(const KeyboardEvent &event) = 0;
  virtual void handleCommand(const PerformanceCommand &command) = 0;
};
