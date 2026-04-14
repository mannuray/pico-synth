#include "AppController.h"
#include <MIDIUSB.h>

namespace {
constexpr uint32_t kLedHeartbeatMs = 1000;
constexpr uint32_t kLedOnWindowMs = 90;
constexpr uint32_t kLedNoteFlashMs = 45;
}

AppController::AppController(MatrixScanner &matrixScanner,
                             MicInput &micInput,
                             SynthEngine &synthEngine,
                             IEventSink &synthSink,
                             IEventSink &midiSink,
                             IEventSink &debugSink)
    : matrixScanner_(matrixScanner),
      micInput_(micInput),
      synth_(synthEngine),
      synthSink_(synthSink),
      midiSink_(midiSink),
      debugSink_(debugSink) {}

void AppController::begin() {
  matrixScanner_.begin();
  micInput_.begin();
  updatePreset();

  lastScanMs_ = millis();
  lastStatusMs_ = millis();
  lastLedMs_ = millis();
  lastMeasurementUs_ = micros();
  totalProcessingUs_ = 0;
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

void AppController::tick() {
  const uint32_t now = millis();

  // Auto-detect USB host: computer connected = MidiOnly, power only = SynthOnly.
  const bool usbConnected = MidiUSB.connected();
  const OutputMode desiredMode = usbConnected ? OutputMode::MidiOnly : OutputMode::SynthOnly;
  if (outputMode_ != desiredMode) {
    if (desiredMode == OutputMode::MidiOnly) {
      synth_.allNotesOff();
    }
    outputMode_ = desiredMode;
  }

  if (now - lastScanMs_ >= config::kScanIntervalMs) {
    processMatrix();
    lastScanMs_ = now;
  }

  micLevel_ = micInput_.readLevel();

  // Skip audio rendering in MidiOnly mode (save CPU).
  if (outputMode_ == OutputMode::MidiOnly) {
    updateStatusLed(now);
    if (now - lastStatusMs_ >= config::kStatusIntervalMs) {
      printStatus();
      lastStatusMs_ = now;
    }
    return;
  }

  synth_.renderFrames(config::kAudioBlockFrames, micLevel_);
  totalProcessingUs_ += synth_.lastProcessingUs();
  updateStatusLed(now);
  if (now - lastStatusMs_ >= config::kStatusIntervalMs) {
    const uint32_t nowUs = micros();
    const uint32_t totalElapsedUs = nowUs - lastMeasurementUs_;
    if (totalElapsedUs > 0) {
      cpuLoad_ = (totalProcessingUs_ * 100) / totalElapsedUs;
    }
    totalProcessingUs_ = 0;
    lastMeasurementUs_ = nowUs;

    printStatus();
    lastStatusMs_ = now;
  }
}

void AppController::printStatus() const {
  const Preset &preset = SynthEngine::presetAt(performanceState_.activePresetIndex());
  Serial.print("preset=");
  Serial.print(preset.name);
  Serial.print(" mode=");
  Serial.print(modeName(outputMode_));
  Serial.print(" bank=");
  Serial.print(performanceState_.bank());
  Serial.print(" slot=");
  Serial.print(performanceState_.instrumentSlot());
  Serial.print(" octave=");
  Serial.print(keyboardState_.octaveShift());
  Serial.print(" transpose=");
  Serial.print(keyboardState_.transpose());
  Serial.print(" sustain=");
  Serial.print(synth_.isSustainEnabled() ? "on" : "off");
  Serial.print(" effect=");
  Serial.print(effectName(performanceState_.effect()));
  Serial.print(performanceState_.effectEnabled() ? "(on)" : "(off)");
  Serial.print(" rhythm=");
  Serial.print(performanceState_.rhythmPlaying() ? "on" : "off");
  Serial.print(" pattern=");
  Serial.print(SynthEngine::rhythmPatternAt(performanceState_.rhythmPattern()).name);
  Serial.print(" bpm=");
  Serial.print(performanceState_.metronomeBpm());
  Serial.print(" volume=");
  Serial.print(performanceState_.masterVolume());
  Serial.print(" mic=");
  Serial.print(micLevel_);
  Serial.print(" cpu=");
  Serial.print(cpuLoad_);
  Serial.println("%");
}

void AppController::updatePreset() {
  if (outputMode_ != OutputMode::MidiOnly) {
    attackOffset_ = 0;
    decayOffset_ = 0;
    sustainOffset_ = 0;
    releaseOffset_ = 0;
    synth_.setAttackOffset(0);
    synth_.setDecayOffset(0);
    synth_.setSustainOffset(0);
    synth_.setReleaseOffset(0);
    synth_.setPreset(performanceState_.activePresetIndex());
  }
}

void AppController::releaseSustainedNotes() {
  for (uint8_t keyIndex = 0; keyIndex < 37; ++keyIndex) {
    if (keyboardState_.isKeyActive(keyIndex) && !keyboardState_.isHeld(keyIndex)) {
      dispatchEvent(KeyboardEvent{
          KeyboardEventType::NoteOff,
          keyboardState_.activeMidiNote(keyIndex),
          ControlType::None,
          0});
      keyboardState_.setKeyActive(keyIndex, false);
    }
  }
}

void AppController::processMatrix() {
  MatrixScanner::Event event;
  while (matrixScanner_.poll(event)) {
    const MatrixBinding &binding = kMatrixBindings[event.row][event.col];
    if (binding.kind == BindingKind::Note) {
      handleNoteEvent(binding, event.pressed);
    } else if (binding.kind == BindingKind::Control && event.pressed) {
      handleControlPress(binding);
    }
  }
}

void AppController::handleNoteEvent(const MatrixBinding &binding, bool pressed) {
  const uint8_t keyIndex = binding.value;
  if (pressed) {
    if (keyboardState_.isHeld(keyIndex)) {
      return;
    }

    const uint8_t midiNote = keyboardState_.midiForKeyIndex(keyIndex);
    keyboardState_.pressKey(keyIndex);
    keyboardState_.setKeyActive(keyIndex, true);
    keyboardState_.setActiveMidiNote(keyIndex, midiNote);
    ledFlashUntilMs_ = millis() + kLedNoteFlashMs;
    dispatchEvent(KeyboardEvent{KeyboardEventType::NoteOn, midiNote, ControlType::None, 0});
    Serial.print("note_on key=");
    Serial.print(keyIndex);
    Serial.print(" midi=");
      Serial.println(midiNote);
  } else {
    if (!keyboardState_.isHeld(keyIndex)) {
      return;
    }

    keyboardState_.releaseKey(keyIndex);
    if (!synth_.isSustainEnabled()) {
      const uint8_t releasedNote = keyboardState_.activeMidiNote(keyIndex);
      dispatchEvent(KeyboardEvent{
          KeyboardEventType::NoteOff,
          releasedNote,
          ControlType::None,
          0});
      keyboardState_.setKeyActive(keyIndex, false);
      Serial.print("note_off key=");
      Serial.print(keyIndex);
      Serial.print(" midi=");
      Serial.println(releasedNote);
    }
  }
}

void AppController::updateStatusLed(uint32_t now) {
#ifdef LED_BUILTIN
  const bool flashActive = now < ledFlashUntilMs_;
  const bool heartbeatOn = ((now - lastLedMs_) % kLedHeartbeatMs) < kLedOnWindowMs;
  digitalWrite(LED_BUILTIN, (flashActive || heartbeatOn) ? HIGH : LOW);
#else
  (void)now;
#endif
}

void AppController::handleControlPress(const MatrixBinding &binding) {
  switch (binding.control) {
    case ControlType::InstrumentSelect:
      performanceState_.setInstrumentSlot(binding.value);
      updatePreset();
      Serial.print("instrument bank=");
      Serial.print(performanceState_.bank());
      Serial.print(" slot=");
      Serial.print(performanceState_.instrumentSlot());
      Serial.print(" preset=");
      Serial.println(SynthEngine::presetAt(performanceState_.activePresetIndex()).name);
      dispatchControlCommand(binding.control, performanceState_.activePresetIndex());
      break;
    case ControlType::BankDown:
      if (performanceState_.bank() > 0) {
        performanceState_.setBank(performanceState_.bank() - 1);
        updatePreset();
      }
      Serial.print("bank=");
      Serial.print(performanceState_.bank());
      Serial.print(" preset=");
      Serial.println(SynthEngine::presetAt(performanceState_.activePresetIndex()).name);
      dispatchControlCommand(binding.control, performanceState_.bank());
      break;
    case ControlType::BankUp:
      if (performanceState_.bank() + 1 < config::kBankCount) {
        performanceState_.setBank(performanceState_.bank() + 1);
        updatePreset();
      }
      Serial.print("bank=");
      Serial.print(performanceState_.bank());
      Serial.print(" preset=");
      Serial.println(SynthEngine::presetAt(performanceState_.activePresetIndex()).name);
      dispatchControlCommand(binding.control, performanceState_.bank());
      break;
    case ControlType::OctaveDown:
      keyboardState_.setOctaveShift(keyboardState_.octaveShift() - 1);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(keyboardState_.octaveShift() - config::kMinOctaveShift));
      break;
    case ControlType::OctaveUp:
      keyboardState_.setOctaveShift(keyboardState_.octaveShift() + 1);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(keyboardState_.octaveShift() - config::kMinOctaveShift));
      break;
    case ControlType::SustainToggle:
      // Toggle sustain state (track locally even in MidiOnly so CC64 is correct).
      synth_.setSustainEnabled(!synth_.isSustainEnabled());
      if (outputMode_ != OutputMode::MidiOnly && !synth_.isSustainEnabled()) {
        releaseSustainedNotes();
      }
      // MIDI CC64 sustain: 127=on, 0=off (standard MIDI requires >= 64 for on).
      dispatchControlCommand(binding.control, synth_.isSustainEnabled() ? static_cast<uint8_t>(127) : kControlValueOff);
      break;
    case ControlType::VolumeDown:
      performanceState_.setMasterVolume(performanceState_.masterVolume() - config::kVolumeStep);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(map(performanceState_.masterVolume(), config::kMinMasterVolume, config::kMaxMasterVolume, 0, 127)));
      break;
    case ControlType::VolumeUp:
      performanceState_.setMasterVolume(performanceState_.masterVolume() + config::kVolumeStep);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(map(performanceState_.masterVolume(), config::kMinMasterVolume, config::kMaxMasterVolume, 0, 127)));
      break;
    case ControlType::TempoDown:
      performanceState_.setMetronomeBpm(performanceState_.metronomeBpm() - 5);
      dispatchControlCommand(binding.control, performanceState_.metronomeBpm());
      break;
    case ControlType::TempoUp:
      performanceState_.setMetronomeBpm(performanceState_.metronomeBpm() + 5);
      dispatchControlCommand(binding.control, performanceState_.metronomeBpm());
      break;
    case ControlType::Panic:
      keyboardState_.resetTransientState();
      dispatchControlCommand(binding.control, kControlValueOn);
      break;
    case ControlType::EffectSelect:
      performanceState_.setEffect(static_cast<EffectType>((static_cast<uint8_t>(performanceState_.effect()) + 1) % static_cast<uint8_t>(EffectType::Count)));
      dispatchControlCommand(binding.control, static_cast<uint8_t>(performanceState_.effect()));
      break;
    case ControlType::EffectToggle:
      performanceState_.setEffectEnabled(!performanceState_.effectEnabled());
      dispatchControlCommand(binding.control, performanceState_.effectEnabled() ? kControlValueOn : kControlValueOff);
      break;
    case ControlType::RhythmStartStop:
      performanceState_.setRhythmPlaying(!performanceState_.rhythmPlaying());
      dispatchControlCommand(binding.control, performanceState_.rhythmPlaying() ? kControlValueOn : kControlValueOff);
      break;
    case ControlType::RhythmSelect:
      performanceState_.setRhythmPattern(binding.value);
      dispatchControlCommand(binding.control, performanceState_.rhythmPattern());
      break;
    case ControlType::TransposeDown:
      keyboardState_.setTranspose(keyboardState_.transpose() - 1);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(keyboardState_.transpose() - config::kMinTranspose));
      break;
    case ControlType::TransposeUp:
      keyboardState_.setTranspose(keyboardState_.transpose() + 1);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(keyboardState_.transpose() - config::kMinTranspose));
      break;
    case ControlType::AttackUp:
      attackOffset_ = (attackOffset_ + 3) % 5 - 2; // cycles -2, -1, 0, 1, 2
      synth_.setAttackOffset(attackOffset_);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(attackOffset_ + 2));
      break;
    case ControlType::DecayUp:
      decayOffset_ = (decayOffset_ + 3) % 5 - 2;
      synth_.setDecayOffset(decayOffset_);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(decayOffset_ + 2));
      break;
    case ControlType::SustainUp:
      sustainOffset_ = (sustainOffset_ + 3) % 5 - 2;
      synth_.setSustainOffset(sustainOffset_);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(sustainOffset_ + 2));
      break;
    case ControlType::ReleaseUp:
      releaseOffset_ = (releaseOffset_ + 3) % 5 - 2;
      synth_.setReleaseOffset(releaseOffset_);
      dispatchControlCommand(binding.control, static_cast<uint8_t>(releaseOffset_ + 2));
      break;
    case ControlType::None:
      break;
  }
}

void AppController::dispatchControlCommand(ControlType control, uint8_t value) {
  Serial.print("control ");
  Serial.print(static_cast<uint8_t>(control));
  Serial.print(" value=");
  Serial.println(value);
  dispatchCommand(PerformanceCommand{control, value});
}

void AppController::dispatchEvent(const KeyboardEvent &event) {
  switch (outputMode_) {
    case OutputMode::SynthOnly:
      synthSink_.handleEvent(event);
      break;
    case OutputMode::MidiOnly:
      midiSink_.handleEvent(event);
      break;
    case OutputMode::SynthAndMidi:
      synthSink_.handleEvent(event);
      midiSink_.handleEvent(event);
      break;
    case OutputMode::Debug:
      debugSink_.handleEvent(event);
      break;
  }
}

void AppController::dispatchCommand(const PerformanceCommand &command) {
  switch (outputMode_) {
    case OutputMode::SynthOnly:
      synthSink_.handleCommand(command);
      break;
    case OutputMode::MidiOnly:
      midiSink_.handleCommand(command);
      break;
    case OutputMode::SynthAndMidi:
      synthSink_.handleCommand(command);
      midiSink_.handleCommand(command);
      break;
    case OutputMode::Debug:
      debugSink_.handleCommand(command);
      break;
  }
}

const char *AppController::modeName(OutputMode mode) {
  switch (mode) {
    case OutputMode::SynthOnly:
      return "Synth";
    case OutputMode::MidiOnly:
      return "Midi";
    case OutputMode::SynthAndMidi:
      return "Both";
    case OutputMode::Debug:
      return "Debug";
    default:
      return "Unknown";
  }
}

const char *AppController::effectName(EffectType effect) {
  switch (effect) {
    case EffectType::Vibrato:
      return "Vibrato";
    case EffectType::Tremolo:
      return "Tremolo";
    case EffectType::Chorus:
      return "Chorus";
    case EffectType::Reverb:
      return "Reverb";
    case EffectType::Delay:
      return "Delay";
    case EffectType::Bitcrusher:
      return "Bitcrush";
    default:
      return "Unknown";
  }
}
