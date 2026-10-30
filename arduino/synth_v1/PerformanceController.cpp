#include "PerformanceController.h"
#include "Presets.h"

namespace {
constexpr int kTempoStepBpm = 5;
}

PerformanceController::PerformanceController(IEventSink &output, Print &log)
    : output_(output), log_(log) {}

void PerformanceController::begin() {
  loadPresetDefaults();
}

bool PerformanceController::handleKey(uint8_t keyIndex, bool pressed) {
  if (keyIndex >= KeyTracker::kKeyCount) {
    return false;
  }
  if (pressed) {
    return pressKey(keyIndex);
  }
  releaseKey(keyIndex);
  return false;
}

void PerformanceController::handleControl(ControlType control, uint8_t value) {
  switch (control) {
    case ControlType::InstrumentSelect: selectInstrument(value); break;
    case ControlType::BankDown:         stepBank(-1); break;
    case ControlType::BankUp:           stepBank(+1); break;
    case ControlType::OctaveDown:       stepOctave(-1); break;
    case ControlType::OctaveUp:         stepOctave(+1); break;
    case ControlType::TransposeDown:    stepTranspose(-1); break;
    case ControlType::TransposeUp:      stepTranspose(+1); break;
    case ControlType::VolumeDown:       stepVolume(-1); break;
    case ControlType::VolumeUp:         stepVolume(+1); break;
    case ControlType::TempoDown:        stepTempo(-1); break;
    case ControlType::TempoUp:          stepTempo(+1); break;
    case ControlType::SustainToggle:    toggleSustain(); break;
    case ControlType::EffectSelect:     nextEffect(); break;
    case ControlType::EffectToggle:     toggleEffect(); break;
    case ControlType::RhythmStartStop:  toggleRhythm(); break;
    case ControlType::RhythmSelect:     selectRhythm(value); break;
    case ControlType::AttackUp:         cycleEnvelopeOffset(EnvelopeParam::Attack); break;
    case ControlType::DecayUp:          cycleEnvelopeOffset(EnvelopeParam::Decay); break;
    case ControlType::SustainUp:        cycleEnvelopeOffset(EnvelopeParam::Sustain); break;
    case ControlType::ReleaseUp:        cycleEnvelopeOffset(EnvelopeParam::Release); break;
    case ControlType::Panic:            panic(); break;
    case ControlType::None:             break;
  }
}

void PerformanceController::syncOutputs() {
  sendQuiet(CommandType::SelectBank, performance_.bank());
  sendPresetState();
  sendQuiet(CommandType::Sustain, performance_.sustainEnabled() ? 1 : 0);
  sendQuiet(CommandType::Volume, performance_.masterVolume());
  sendQuiet(CommandType::Tempo, performance_.tempoBpm());
  sendQuiet(CommandType::RhythmPattern, performance_.rhythmPattern());
  sendQuiet(CommandType::RhythmPlaying, performance_.rhythmPlaying() ? 1 : 0);
  sendQuiet(CommandType::OctaveShift, pitch_.octaveShift());
  sendQuiet(CommandType::Transpose, pitch_.transpose());
}

// ---- Key handling ----

bool PerformanceController::pressKey(uint8_t keyIndex) {
  if (keys_.isHeld(keyIndex)) {
    return false;
  }
  const uint8_t midiNote = pitch_.midiNoteForKey(keyIndex);
  keys_.press(keyIndex, midiNote);
  sendNote(KeyboardEventType::NoteOn, midiNote);
  logNote("note_on", keyIndex, midiNote);
  return true;
}

// While sustain is on the key keeps sounding; releaseSustainedNotes() ends it later.
void PerformanceController::releaseKey(uint8_t keyIndex) {
  if (!keys_.isHeld(keyIndex)) {
    return;
  }
  keys_.release(keyIndex);
  if (performance_.sustainEnabled()) {
    return;
  }
  const uint8_t midiNote = keys_.soundingNote(keyIndex);
  sendNote(KeyboardEventType::NoteOff, midiNote);
  keys_.stopSounding(keyIndex);
  logNote("note_off", keyIndex, midiNote);
}

void PerformanceController::releaseSustainedNotes() {
  for (uint8_t keyIndex = 0; keyIndex < KeyTracker::kKeyCount; ++keyIndex) {
    if (keys_.isSounding(keyIndex) && !keys_.isHeld(keyIndex)) {
      sendNote(KeyboardEventType::NoteOff, keys_.soundingNote(keyIndex));
      keys_.stopSounding(keyIndex);
    }
  }
}

// ---- Button actions ----

void PerformanceController::selectInstrument(uint8_t slot) {
  performance_.setInstrumentSlot(slot);
  applyActivePreset();
  logPreset();
}

void PerformanceController::stepBank(int8_t direction) {
  const int bank = performance_.bank() + direction;
  if (bank >= 0 && bank < config::kBankCount) {
    performance_.setBank(static_cast<uint8_t>(bank));
    send(CommandType::SelectBank, performance_.bank());
    applyActivePreset();
  }
  logPreset();
}

void PerformanceController::stepOctave(int8_t direction) {
  pitch_.setOctaveShift(pitch_.octaveShift() + direction);
  send(CommandType::OctaveShift, pitch_.octaveShift());
}

void PerformanceController::stepTranspose(int8_t direction) {
  pitch_.setTranspose(pitch_.transpose() + direction);
  send(CommandType::Transpose, pitch_.transpose());
}

void PerformanceController::stepVolume(int8_t direction) {
  performance_.setMasterVolume(performance_.masterVolume() + direction * config::kVolumeStep);
  send(CommandType::Volume, performance_.masterVolume());
}

void PerformanceController::stepTempo(int8_t direction) {
  performance_.setTempoBpm(performance_.tempoBpm() + direction * kTempoStepBpm);
  send(CommandType::Tempo, performance_.tempoBpm());
}

void PerformanceController::toggleSustain() {
  performance_.setSustainEnabled(!performance_.sustainEnabled());
  // Outputs must see sustain off before the held-over note-offs arrive.
  send(CommandType::Sustain, performance_.sustainEnabled() ? 1 : 0);
  if (!performance_.sustainEnabled()) {
    releaseSustainedNotes();
  }
}

void PerformanceController::toggleEffect() {
  performance_.setEffectEnabled(!performance_.effectEnabled());
  send(CommandType::EffectEnabled, performance_.effectEnabled() ? 1 : 0);
}

void PerformanceController::nextEffect() {
  const uint8_t next = (static_cast<uint8_t>(performance_.effect()) + 1) % static_cast<uint8_t>(EffectType::Count);
  performance_.setEffect(static_cast<EffectType>(next));
  send(CommandType::Effect, static_cast<int16_t>(performance_.effect()));
}

void PerformanceController::toggleRhythm() {
  performance_.setRhythmPlaying(!performance_.rhythmPlaying());
  send(CommandType::RhythmPlaying, performance_.rhythmPlaying() ? 1 : 0);
}

void PerformanceController::selectRhythm(uint8_t pattern) {
  performance_.setRhythmPattern(pattern);
  send(CommandType::RhythmPattern, performance_.rhythmPattern());
}

// Cycles -2, -1, 0, 1, 2, -2, ...
void PerformanceController::cycleEnvelopeOffset(EnvelopeParam param) {
  const int8_t current = performance_.envelopeOffset(param);
  const int8_t next = (current >= config::kMaxEnvelopeOffset) ? config::kMinEnvelopeOffset : static_cast<int8_t>(current + 1);
  performance_.setEnvelopeOffset(param, next);
  send(envelopeCommand(param), next);
}

void PerformanceController::panic() {
  keys_.reset();
  send(CommandType::AllNotesOff, 0);
}

// ---- Presets ----

// A preset brings its own default effect and resets the envelope tweaks.
void PerformanceController::loadPresetDefaults() {
  const Preset &preset = presets::at(performance_.activePresetIndex());
  performance_.resetEnvelopeOffsets();
  performance_.setEffect(preset.defaultEffect);
  performance_.setEffectEnabled(true);
}

// Sends the preset plus only the defaults that changed, so MIDI hosts aren't
// sent envelope or effect CCs the user never touched.
void PerformanceController::applyActivePreset() {
  const PerformanceState before = performance_;
  loadPresetDefaults();

  sendQuiet(CommandType::SelectPreset, performance_.activePresetIndex());
  for (uint8_t i = 0; i < static_cast<uint8_t>(EnvelopeParam::Count); ++i) {
    const EnvelopeParam param = static_cast<EnvelopeParam>(i);
    if (performance_.envelopeOffset(param) != before.envelopeOffset(param)) {
      sendQuiet(envelopeCommand(param), performance_.envelopeOffset(param));
    }
  }
  if (performance_.effect() != before.effect()) {
    sendQuiet(CommandType::Effect, static_cast<int16_t>(performance_.effect()));
  }
  if (performance_.effectEnabled() != before.effectEnabled()) {
    sendQuiet(CommandType::EffectEnabled, performance_.effectEnabled() ? 1 : 0);
  }
}

// Preset first: the envelope offsets apply on top of it.
void PerformanceController::sendPresetState() {
  sendQuiet(CommandType::SelectPreset, performance_.activePresetIndex());
  for (uint8_t i = 0; i < static_cast<uint8_t>(EnvelopeParam::Count); ++i) {
    const EnvelopeParam param = static_cast<EnvelopeParam>(i);
    sendQuiet(envelopeCommand(param), performance_.envelopeOffset(param));
  }
  sendQuiet(CommandType::Effect, static_cast<int16_t>(performance_.effect()));
  sendQuiet(CommandType::EffectEnabled, performance_.effectEnabled() ? 1 : 0);
}

// ---- Output ----

void PerformanceController::send(CommandType type, int16_t value) {
  log_.print("cmd ");
  log_.print(static_cast<uint8_t>(type));
  log_.print(" value=");
  log_.println(value);
  sendQuiet(type, value);
}

void PerformanceController::sendQuiet(CommandType type, int16_t value) {
  output_.handleCommand(PerformanceCommand{type, value});
}

void PerformanceController::sendNote(KeyboardEventType type, uint8_t note) {
  output_.handleEvent(KeyboardEvent{type, note});
}

void PerformanceController::logNote(const char *label, uint8_t keyIndex, uint8_t midiNote) {
  log_.print(label);
  log_.print(" key=");
  log_.print(keyIndex);
  log_.print(" midi=");
  log_.println(midiNote);
}

void PerformanceController::logPreset() {
  log_.print("bank=");
  log_.print(performance_.bank());
  log_.print(" slot=");
  log_.print(performance_.instrumentSlot());
  log_.print(" preset=");
  log_.println(presets::at(performance_.activePresetIndex()).name);
}

CommandType PerformanceController::envelopeCommand(EnvelopeParam param) {
  switch (param) {
    case EnvelopeParam::Attack:
      return CommandType::AttackOffset;
    case EnvelopeParam::Decay:
      return CommandType::DecayOffset;
    case EnvelopeParam::Sustain:
      return CommandType::SustainOffset;
    case EnvelopeParam::Release:
    case EnvelopeParam::Count:
      break;
  }
  return CommandType::ReleaseOffset;
}
