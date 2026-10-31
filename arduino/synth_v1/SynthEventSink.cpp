#include "SynthEventSink.h"

void SynthEventSink::handleEvent(const KeyboardEvent &event) {
  switch (event.type) {
    case KeyboardEventType::NoteOn:
      synth_.noteOn(event.note);
      break;
    case KeyboardEventType::NoteOff:
      synth_.noteOff(event.note);
      break;
  }
}

void SynthEventSink::handleCommand(const PerformanceCommand &command) {
  switch (command.type) {
    case CommandType::SelectPreset:
      synth_.setPreset(static_cast<uint8_t>(command.value));
      break;
    case CommandType::Volume:
      synth_.setMasterVolume(command.value);
      break;
    case CommandType::Tempo:
      synth_.setTempoBpm(static_cast<uint8_t>(command.value));
      break;
    case CommandType::Effect:
      synth_.setEffect(static_cast<EffectType>(command.value));
      break;
    case CommandType::EffectEnabled:
      synth_.setEffectEnabled(command.value != 0);
      break;
    case CommandType::RhythmPlaying:
      switch (command.value) {
        case kRhythmStart:          synth_.startRhythm(false); break;
        case kRhythmStartWithIntro: synth_.startRhythm(true); break;
        case kRhythmStopWithEnding: synth_.stopRhythm(true); break;
        default:                    synth_.stopRhythm(false); break;
      }
      break;
    case CommandType::RhythmPattern:
      synth_.setRhythmPattern(static_cast<uint8_t>(command.value));
      break;
    case CommandType::RhythmFill:
      synth_.rhythmFill();
      break;
    case CommandType::RotaryFast:
      synth_.setRotaryFast(command.value != 0);
      break;
    case CommandType::AllNotesOff:
      synth_.allNotesOff();
      break;
    case CommandType::Sustain:        // the controller holds back note-offs while sustain is on
    case CommandType::SelectBank:     // the preset index already includes the bank
    case CommandType::OctaveShift:    // applied to note numbers before they arrive
    case CommandType::Transpose:      // applied to note numbers before they arrive
      break;
  }
}
