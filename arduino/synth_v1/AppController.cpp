#include "AppController.h"
#include "MatrixMap.h"
#include "StatusReporter.h"

AppController::AppController(IKeyMatrix &keyMatrix,
                             ILevelInput &levelInput,
                             IAudioRenderer &audioRenderer,
                             OutputRouter &router,
                             PerformanceController &performance,
                             IHostLink &hostLink,
                             IStatusIndicator &statusIndicator,
                             const IClock &clock,
                             Print &log)
    : keyMatrix_(keyMatrix),
      levelInput_(levelInput),
      audioRenderer_(audioRenderer),
      router_(router),
      performance_(performance),
      hostLink_(hostLink),
      statusIndicator_(statusIndicator),
      clock_(clock),
      log_(log) {}

void AppController::begin() {
  router_.setMode(detectOutputMode());
  performance_.begin();
  if (router_.synthActive()) {
    performance_.syncOutputs();
  }

  lastScanMs_ = clock_.nowMs();
  lastStatusMs_ = clock_.nowMs();
  cpuMeter_.reset(clock_.nowUs());
}

void AppController::tick() {
  const uint32_t now = clock_.nowMs();

  // The synth ignores commands while it isn't an output, so bring it up to
  // date when it becomes one. MIDI hosts keep their own state; send them nothing.
  if (router_.setMode(detectOutputMode()) && router_.synthActive()) {
    performance_.syncOutputs();
  }

  if (now - lastScanMs_ >= config::kScanIntervalMs) {
    processMatrix(now);
    lastScanMs_ = now;
  }

  micLevel_ = levelInput_.readLevel();

  // Skip audio rendering when the synth isn't an output (save CPU).
  if (router_.synthActive()) {
    audioRenderer_.renderFrames(config::kAudioBlockFrames);
    cpuMeter_.addBusyUs(audioRenderer_.lastProcessingUs());
  }

  statusIndicator_.update(now);
  if (now - lastStatusMs_ >= config::kStatusIntervalMs) {
    cpuMeter_.update(clock_.nowUs());
    printStatus();
    lastStatusMs_ = now;
  }
}

void AppController::printStatus() const {
  ::printStatus(log_, StatusSnapshot{performance_.performance(),
                                     performance_.pitch(),
                                     router_.mode(),
                                     micLevel_,
                                     cpuMeter_.loadPercent()});
}

// Computer connected = MidiOnly, power only = SynthOnly.
OutputMode AppController::detectOutputMode() {
  return hostLink_.isConnected() ? OutputMode::MidiOnly : OutputMode::SynthOnly;
}

void AppController::processMatrix(uint32_t nowMs) {
  KeyMatrixEvent event;
  while (keyMatrix_.poll(event)) {
    const MatrixBinding &binding = kMatrixBindings[event.row][event.col];
    if (binding.kind == BindingKind::Note) {
      if (performance_.handleKey(binding.value, event.pressed)) {
        statusIndicator_.flash(nowMs);
      }
    } else if (binding.kind == BindingKind::Control && event.pressed) {
      performance_.handleControl(binding.control, binding.value);
    }
  }
}
