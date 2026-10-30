#include "MidiEventSink.h"
#include "Config.h"

#include <MIDIUSB.h>

namespace {
constexpr uint8_t kMidiChannel = 0;

// Control change numbers. 20-25 are undefined in the MIDI spec and used for
// this instrument's own controls.
constexpr uint8_t kCcBankSelect = 0;
constexpr uint8_t kCcVolume = 7;
constexpr uint8_t kCcTempo = 20;
constexpr uint8_t kCcOctave = 21;
constexpr uint8_t kCcTranspose = 22;
constexpr uint8_t kCcEffect = 23;
constexpr uint8_t kCcEffectEnabled = 24;
constexpr uint8_t kCcRhythmPattern = 25;
constexpr uint8_t kCcSustain = 64;
constexpr uint8_t kCcSustainLevel = 70;
constexpr uint8_t kCcReleaseTime = 72;
constexpr uint8_t kCcAttackTime = 73;
constexpr uint8_t kCcDecayTime = 75;
constexpr uint8_t kCcRhythmPlaying = 111;
constexpr uint8_t kCcAllSoundOff = 120;
constexpr uint8_t kCcAllNotesOff = 123;

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

// Clamps to the 7-bit range MIDI data bytes allow.
void sendControlChange(uint8_t control, int value) {
  sendPacket(static_cast<uint8_t>(0xB0 | kMidiChannel), control, static_cast<uint8_t>(constrain(value, 0, 127)));
}

void sendProgramChange(uint8_t program) {
  const midiEventPacket_t packet = {0x0C, static_cast<uint8_t>(0xC0 | kMidiChannel), program, 0};
  MidiUSB.sendMIDI(packet);
}

// Envelope offsets go out as 0..4 (offset + 2).
int envelopeValue(int16_t offset) {
  return offset - config::kMinEnvelopeOffset;
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
  }

  MidiUSB.flush();
}

void MidiEventSink::handleCommand(const PerformanceCommand &command) {
  switch (command.type) {
    case CommandType::SelectBank:
      sendControlChange(kCcBankSelect, command.value);
      break;
    case CommandType::SelectPreset:
      sendProgramChange(static_cast<uint8_t>(command.value));
      break;
    case CommandType::Sustain:
      // Standard MIDI treats >= 64 as on.
      sendControlChange(kCcSustain, command.value != 0 ? 127 : 0);
      break;
    case CommandType::Volume:
      sendControlChange(kCcVolume, map(command.value, config::kMinMasterVolume, config::kMaxMasterVolume, 0, 127));
      break;
    case CommandType::Tempo:
      sendControlChange(kCcTempo, map(command.value, config::kTempoMinBpm, config::kTempoMaxBpm, 0, 127));
      break;
    case CommandType::Effect:
      sendControlChange(kCcEffect, command.value);
      break;
    case CommandType::EffectEnabled:
      sendControlChange(kCcEffectEnabled, command.value);
      break;
    case CommandType::RhythmPlaying:
      sendControlChange(kCcRhythmPlaying, command.value);
      break;
    case CommandType::RhythmPattern:
      sendControlChange(kCcRhythmPattern, command.value);
      break;
    case CommandType::OctaveShift:
      sendControlChange(kCcOctave, command.value - config::kMinOctaveShift);
      break;
    case CommandType::Transpose:
      sendControlChange(kCcTranspose, command.value - config::kMinTranspose);
      break;
    case CommandType::AttackOffset:
      sendControlChange(kCcAttackTime, envelopeValue(command.value));
      break;
    case CommandType::DecayOffset:
      sendControlChange(kCcDecayTime, envelopeValue(command.value));
      break;
    case CommandType::SustainOffset:
      sendControlChange(kCcSustainLevel, envelopeValue(command.value));
      break;
    case CommandType::ReleaseOffset:
      sendControlChange(kCcReleaseTime, envelopeValue(command.value));
      break;
    case CommandType::AllNotesOff:
      sendControlChange(kCcAllSoundOff, 0);
      sendControlChange(kCcAllNotesOff, 0);
      break;
  }

  MidiUSB.flush();
}
