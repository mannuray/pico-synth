#pragma once

#include "IEventSink.h"

class SerialDebugSink final : public IEventSink {
public:
  explicit SerialDebugSink(Print &out) : out_(out) {}

  void handleEvent(const KeyboardEvent &event) override;
  void handleCommand(const PerformanceCommand &command) override;

private:
  Print &out_;
};
