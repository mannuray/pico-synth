#include "PerformanceController.h"
#include "Presets.h"
#include "RhythmPatterns.h"

namespace {
constexpr int kTempoStepBpm = 5;

// Volume moves in 3 dB steps: x0.708 down, x1.413 up (Q15). Below about
// -30 dB the next step down is silence; from silence the next step up is -30 dB.
constexpr int32_t kVolumeDownQ15 = 23198;
constexpr int32_t kVolumeUpQ15 = 46286;
constexpr int16_t kQuietestVolume = 1036;
}  // namespace

PerformanceController::PerformanceController(IEventSink &output, IFeedback &feedback, Print &log)
    : output_(output), feedback_(feedback), log_(log) {}

void PerformanceController::begin() {
  loadPresetDefaults();
  performance_.setTempoBpm(rhythmPatternAt(performance_.rhythmPattern()).defaultBpm);
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
  hitLimit_ = false;
  switch (control) {
    case ControlType::InstrumentSelect: selectInstrument(value); break;
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
    case ControlType::RhythmFill:       rhythmFill(); break;
    case ControlType::IntroEnding:      introOrEnding(); break;
    case ControlType::Panic:            panic(); break;
    case ControlType::None:             break;
  }
}

void PerformanceController::toggleFeedback() {
  feedback_.setEnabled(!feedback_.enabled());
}

void PerformanceController::syncOutputs() {
  sendPresetState();
  sendQuiet(CommandType::Sustain, performance_.sustainEnabled() ? 1 : 0);
  sendQuiet(CommandType::Volume, performance_.masterVolume());
  sendQuiet(CommandType::Tempo, performance_.tempoBpm());
  sendQuiet(CommandType::RhythmPattern, performance_.rhythmPattern());
  sendQuiet(CommandType::RhythmPlaying, performance_.rhythmPlaying() ? kRhythmStart : kRhythmStop);
  sendQuiet(CommandType::OctaveShift, pitch_.octaveShift());
  sendQuiet(CommandType::Transpose, pitch_.transpose());
  sendQuiet(CommandType::RotaryFast, performance_.rotaryFast() ? 1 : 0);
}

// ---- Key handling ----

bool PerformanceController::pressKey(uint8_t keyIndex) {
  if (keys_.isHeld(keyIndex)) {
    return false;
  }
  // Sync start: the first key played starts the rhythm, in time with it.
  if (performance_.syncStartArmed() && !performance_.rhythmPlaying()) {
    performance_.setSyncStartArmed(false);
    startRhythm(kRhythmStart);
  }
  if (keys_.isSounding(keyIndex)) {
    // Still ringing under sustain: end that note first, or it would hang
    // (its pitch may differ now if octave/transpose changed).
    sendNote(KeyboardEventType::NoteOff, keys_.soundingNote(keyIndex));
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

// Pressing the organ's button again switches its Leslie between slow and fast.
void PerformanceController::selectInstrument(uint8_t slot) {
  if (slot == performance_.instrumentSlot() && presetHasRotary()) {
    toggleLeslie();
    return;
  }
  performance_.setInstrumentSlot(slot);
  applyActivePreset();
  logPreset();
}

void PerformanceController::stepOctave(int8_t direction) {
  const int before = pitch_.octaveShift();
  pitch_.setOctaveShift(pitch_.octaveShift() + direction);
  send(CommandType::OctaveShift, pitch_.octaveShift());
  cueStep(before, pitch_.octaveShift(), direction, 0);
}

void PerformanceController::stepTranspose(int8_t direction) {
  const int before = pitch_.transpose();
  pitch_.setTranspose(pitch_.transpose() + direction);
  send(CommandType::Transpose, pitch_.transpose());
  cueStep(before, pitch_.transpose(), direction, 0);
}

void PerformanceController::stepVolume(int8_t direction) {
  const int32_t before = performance_.masterVolume();
  int32_t volume = before;
  if (direction < 0) {
    volume = (volume * kVolumeDownQ15) >> 15;
    if (volume < kQuietestVolume) volume = 0;
  } else {
    volume = (volume == 0) ? kQuietestVolume : ((volume * kVolumeUpQ15) >> 15);
  }
  performance_.setMasterVolume(volume);
  if (performance_.masterVolume() == before) {
    cueLimit();
    return;
  }
  send(CommandType::Volume, performance_.masterVolume());
  feedback_.cue(FeedbackCue::Blip);  // plays at the new volume
}

void PerformanceController::stepTempo(int8_t direction) {
  const int before = performance_.tempoBpm();
  performance_.setTempoBpm(performance_.tempoBpm() + direction * kTempoStepBpm);
  send(CommandType::Tempo, performance_.tempoBpm());
  cueStep(before, performance_.tempoBpm(), direction, -1);
}

void PerformanceController::toggleSustain() {
  performance_.setSustainEnabled(!performance_.sustainEnabled());
  // Outputs must see sustain off before the held-over note-offs arrive.
  send(CommandType::Sustain, performance_.sustainEnabled() ? 1 : 0);
  if (!performance_.sustainEnabled()) {
    releaseSustainedNotes();
  }
  feedback_.cue(performance_.sustainEnabled() ? FeedbackCue::Up : FeedbackCue::Down);
}

void PerformanceController::toggleEffect() {
  performance_.setEffectEnabled(!performance_.effectEnabled());
  send(CommandType::EffectEnabled, performance_.effectEnabled() ? 1 : 0);
  feedback_.cue(performance_.effectEnabled() ? FeedbackCue::Up : FeedbackCue::Down);
}

// Choosing an effect also switches it on, so the choice is heard.
void PerformanceController::nextEffect() {
  const uint8_t next = (static_cast<uint8_t>(performance_.effect()) + 1) % static_cast<uint8_t>(EffectType::Count);
  performance_.setEffect(static_cast<EffectType>(next));
  send(CommandType::Effect, static_cast<int16_t>(performance_.effect()));
  if (!performance_.effectEnabled()) {
    performance_.setEffectEnabled(true);
    send(CommandType::EffectEnabled, 1);
  }
  feedback_.cue(FeedbackCue::Blip);
}

void PerformanceController::toggleRhythm() {
  if (performance_.rhythmPlaying()) {
    performance_.setRhythmPlaying(false);
    send(CommandType::RhythmPlaying, kRhythmStop);
  } else {
    performance_.setSyncStartArmed(false);
    startRhythm(kRhythmStart);
  }
}

// Recalls the pattern's usual tempo only while stopped: when switching
// patterns mid-song, the tempo you're playing at is kept.
void PerformanceController::selectRhythm(uint8_t pattern) {
  performance_.setRhythmPattern(pattern);
  send(CommandType::RhythmPattern, performance_.rhythmPattern());
  if (!performance_.rhythmPlaying()) {
    performance_.setTempoBpm(rhythmPatternAt(performance_.rhythmPattern()).defaultBpm);
    send(CommandType::Tempo, performance_.tempoBpm());
  }
}

// Playing: a fill on the next bar. Stopped: arm (or disarm) sync start.
void PerformanceController::rhythmFill() {
  if (!performance_.rhythmPlaying()) {
    toggleSyncStart();
    return;
  }
  send(CommandType::RhythmFill, 0);
}

void PerformanceController::toggleSyncStart() {
  performance_.setSyncStartArmed(!performance_.syncStartArmed());
  feedback_.cue(performance_.syncStartArmed() ? FeedbackCue::Up : FeedbackCue::Down);
}

// Stopped: start with the intro (fill) bar. Playing: end with a fill and crash.
void PerformanceController::introOrEnding() {
  if (performance_.rhythmPlaying()) {
    performance_.setRhythmPlaying(false);
    send(CommandType::RhythmPlaying, kRhythmStopWithEnding);
  } else {
    performance_.setSyncStartArmed(false);
    startRhythm(kRhythmStartWithIntro);
  }
}

// Leslie fast/slow, only on presets with a rotary speaker.
void PerformanceController::toggleLeslie() {
  if (!presetHasRotary()) {
    cueLimit();
    return;
  }
  performance_.setRotaryFast(!performance_.rotaryFast());
  send(CommandType::RotaryFast, performance_.rotaryFast() ? 1 : 0);
  feedback_.cue(performance_.rotaryFast() ? FeedbackCue::Up : FeedbackCue::Down);
}

// Stops every note; also releases sustain so the next notes don't hang.
// The rhythm keeps playing.
void PerformanceController::panic() {
  keys_.reset();
  send(CommandType::AllNotesOff, 0);
  if (performance_.sustainEnabled()) {
    performance_.setSustainEnabled(false);
    send(CommandType::Sustain, 0);
  }
}

// ---- Presets ----

// A preset brings its own default effect.
void PerformanceController::loadPresetDefaults() {
  const Preset &preset = presets::at(performance_.activePresetIndex());
  performance_.setEffect(preset.defaultEffect);
  performance_.setEffectEnabled(preset.effectEnabled);
}

// Sends the preset plus only the defaults that changed, so MIDI hosts aren't
// sent effect CCs the user never touched.
void PerformanceController::applyActivePreset() {
  const PerformanceState before = performance_;
  loadPresetDefaults();

  sendQuiet(CommandType::SelectPreset, performance_.activePresetIndex());
  if (performance_.effect() != before.effect()) {
    sendQuiet(CommandType::Effect, static_cast<int16_t>(performance_.effect()));
  }
  if (performance_.effectEnabled() != before.effectEnabled()) {
    sendQuiet(CommandType::EffectEnabled, performance_.effectEnabled() ? 1 : 0);
  }
}

void PerformanceController::sendPresetState() {
  sendQuiet(CommandType::SelectPreset, performance_.activePresetIndex());
  sendQuiet(CommandType::Effect, static_cast<int16_t>(performance_.effect()));
  sendQuiet(CommandType::EffectEnabled, performance_.effectEnabled() ? 1 : 0);
}

bool PerformanceController::presetHasRotary() const {
  const OrganTraits *organ = presets::at(performance_.activePresetIndex()).organ;
  return organ != nullptr && organ->rotarySlowHzX10 > 0;
}

// ---- Output ----

void PerformanceController::startRhythm(int16_t transport) {
  performance_.setRhythmPlaying(true);
  send(CommandType::RhythmPlaying, transport);
}

// Feedback for a stepped value: "bonk" if it couldn't move, a double blip on
// reaching `center` (pass -1 for none), otherwise rising or falling notes.
void PerformanceController::cueStep(int before, int after, int direction, int center) {
  if (after == before) {
    cueLimit();
  } else if (after == center) {
    feedback_.cue(FeedbackCue::Center);
  } else {
    feedback_.cue(direction > 0 ? FeedbackCue::Up : FeedbackCue::Down);
  }
}

void PerformanceController::cueLimit() {
  hitLimit_ = true;
  feedback_.cue(FeedbackCue::Limit);
}

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
  log_.print("slot=");
  log_.print(performance_.instrumentSlot());
  log_.print(" preset=");
  log_.println(presets::at(performance_.activePresetIndex()).name);
}
