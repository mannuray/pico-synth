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
      synth_.setRhythmPlaying(command.value != 0);
      break;
    case CommandType::RhythmPattern:
      synth_.setRhythmPattern(static_cast<uint8_t>(command.value));
      break;
    case CommandType::AttackOffset:
      synth_.setAttackOffset(static_cast<int8_t>(command.value));
      break;
    case CommandType::DecayOffset:
      synth_.setDecayOffset(static_cast<int8_t>(command.value));
      break;
    case CommandType::SustainOffset:
      synth_.setSustainOffset(static_cast<int8_t>(command.value));
      break;
    case CommandType::ReleaseOffset:
      synth_.setReleaseOffset(static_cast<int8_t>(command.value));
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
