#pragma once

#include "OutputRouter.h"
#include "PerformanceState.h"
#include "PitchSettings.h"

struct StatusSnapshot {
  const PerformanceState &performance;
  const PitchSettings &pitch;
  OutputMode mode;
  uint16_t micLevel;
  uint16_t cpuLoadPercent;
};

// Prints the one-line periodic status report.
void printStatusLine(Print &out, const StatusSnapshot &status);
