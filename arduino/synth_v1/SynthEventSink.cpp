#include "SynthEventSink.h"

void SynthEventSink::handleEvent(const KeyboardEvent &event) {
  switch (event.type) {
    case KeyboardEventType::NoteOn:
      synth_.noteOn(event.note);
      break;
    case KeyboardEventType::NoteOff:
      synth_.noteOff(event.note);
      break;
    case KeyboardEventType::Control:
      break;
  }
}

void SynthEventSink::handleCommand(const PerformanceCommand &command) {
  switch (command.control) {
    case ControlType::SustainToggle:
      synth_.setSustainEnabled(command.value != 0);
      break;
    case ControlType::VolumeDown:
    case ControlType::VolumeUp:
      synth_.setMasterVolume(map(command.value, 0, 127, 0, 32767));
      break;
    case ControlType::TempoDown:
    case ControlType::TempoUp:
      synth_.setMetronomeBpm(command.value);
      break;
    case ControlType::EffectSelect:
      synth_.setEffect(static_cast<EffectType>(command.value % static_cast<uint8_t>(EffectType::Count)));
      break;
    case ControlType::EffectToggle:
      synth_.setEffectEnabled(command.value != 0);
      break;
    case ControlType::RhythmStartStop:
      synth_.setRhythmPlaying(command.value != 0);
      break;
    case ControlType::RhythmSelect:
      synth_.setRhythmPattern(command.value);
      break;
    case ControlType::Panic:
      synth_.allNotesOff();
      break;
    case ControlType::InstrumentSelect:
    case ControlType::BankDown:
    case ControlType::BankUp:
    case ControlType::OctaveDown:
    case ControlType::OctaveUp:
    case ControlType::TransposeDown:
    case ControlType::TransposeUp:
    case ControlType::None:
      break;
  }
}
