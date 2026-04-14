#include "MidiEventSink.h"

#include <MIDIUSB.h>

namespace {
constexpr uint8_t kMidiChannel = 0;

constexpr uint8_t cinForStatus(uint8_t status) {
  switch (status & 0xF0) {
    case 0x80:
      return 0x08;
    case 0x90:
      return 0x09;
    case 0xA0:
      return 0x0A;
    case 0xB0:
      return 0x0B;
    case 0xC0:
      return 0x0C;
    case 0xD0:
      return 0x0D;
    case 0xE0:
      return 0x0E;
    default:
      return 0x0F;
  }
}

void sendPacket(uint8_t status, uint8_t data1, uint8_t data2) {
  const midiEventPacket_t packet = {cinForStatus(status), status, data1, data2};
  MidiUSB.sendMIDI(packet);
}

void sendNoteOn(uint8_t note, uint8_t velocity = 100) {
  sendPacket(static_cast<uint8_t>(0x90 | kMidiChannel), note, velocity);
}

void sendNoteOff(uint8_t note, uint8_t velocity = 0) {
  sendPacket(static_cast<uint8_t>(0x80 | kMidiChannel), note, velocity);
}

void sendControlChange(uint8_t control, uint8_t value) {
  sendPacket(static_cast<uint8_t>(0xB0 | kMidiChannel), control, value);
}

void sendProgramChange(uint8_t program) {
  const midiEventPacket_t packet = {0x0C, static_cast<uint8_t>(0xC0 | kMidiChannel), program, 0};
  MidiUSB.sendMIDI(packet);
}

uint8_t controlToCc(ControlType control) {
  switch (control) {
    case ControlType::BankDown:
    case ControlType::BankUp:
      return 0x00;
    case ControlType::SustainToggle:
      return 64;
    case ControlType::VolumeDown:
    case ControlType::VolumeUp:
      return 7;
    case ControlType::RhythmStartStop:
      return 111;
    case ControlType::TempoDown:
    case ControlType::TempoUp:
      return 20;
    case ControlType::OctaveDown:
    case ControlType::OctaveUp:
      return 21;
    case ControlType::TransposeDown:
    case ControlType::TransposeUp:
      return 22;
    case ControlType::EffectSelect:
      return 23;
    case ControlType::EffectToggle:
      return 24;
    case ControlType::RhythmSelect:
      return 25;
    case ControlType::AttackUp:
      return 73;
    case ControlType::DecayUp:
      return 75;
    case ControlType::SustainUp:
      return 70;
    case ControlType::ReleaseUp:
      return 72;
    default:
      return 0x7F;
  }
}
}  // namespace

void MidiEventSink::handleEvent(const KeyboardEvent &event) {
  switch (event.type) {
    case KeyboardEventType::NoteOn:
      sendNoteOn(event.note);
      break;
    case KeyboardEventType::NoteOff:
      sendNoteOff(event.note);
      break;
    case KeyboardEventType::Control:
      sendControlChange(controlToCc(event.control), event.value);
      break;
  }

  MidiUSB.flush();
}

void MidiEventSink::handleCommand(const PerformanceCommand &command) {
  switch (command.control) {
    case ControlType::InstrumentSelect:
      sendProgramChange(command.value);
      break;
    case ControlType::Panic:
      sendControlChange(120, 0);
      sendControlChange(123, 0);
      break;
    default:
      sendControlChange(controlToCc(command.control), command.value);
      break;
  }

  MidiUSB.flush();
}
