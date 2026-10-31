#pragma once

#include "ControlHold.h"
#include "CpuMeter.h"
#include "IAudioRenderer.h"
#include "IClock.h"
#include "IHostLink.h"
#include "IKeyMatrix.h"
#include "ILevelInput.h"
#include "IStatusIndicator.h"
#include "OutputRouter.h"
#include "PerformanceController.h"

// Main loop: schedules scanning, audio rendering and status reporting,
// and picks the output mode from the USB host connection.
class AppController {
public:
  AppController(IKeyMatrix &keyMatrix,
                ILevelInput &levelInput,
                IAudioRenderer &audioRenderer,
                OutputRouter &router,
                PerformanceController &performance,
                IHostLink &hostLink,
                IStatusIndicator &statusIndicator,
                const IClock &clock,
                Print &log);

  void begin();
  void tick();
  void printStatus() const;

private:
  IKeyMatrix &keyMatrix_;
  ILevelInput &levelInput_;
  IAudioRenderer &audioRenderer_;
  OutputRouter &router_;
  PerformanceController &performance_;
  IHostLink &hostLink_;
  IStatusIndicator &statusIndicator_;
  const IClock &clock_;
  Print &log_;
  CpuMeter cpuMeter_;
  ControlHold controlHold_;
  uint16_t micLevel_ = 0;
  uint32_t lastScanMs_ = 0;
  uint32_t lastStatusMs_ = 0;
  OutputMode pendingMode_ = OutputMode::SynthOnly;
  uint32_t pendingSinceMs_ = 0;

  OutputMode detectOutputMode();
  void updateOutputMode(uint32_t nowMs);
  void processMatrix(uint32_t nowMs);
};
