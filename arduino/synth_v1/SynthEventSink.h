#pragma once

#include "IEventSink.h"
#include "SynthEngine.h"

class SynthEventSink final : public IEventSink {
public:
  explicit SynthEventSink(SynthEngine &synthEngine) : synth_(synthEngine) {}

  void handleEvent(const KeyboardEvent &event) override;
  void handleCommand(const PerformanceCommand &command) override;

private:
  SynthEngine &synth_;
};
