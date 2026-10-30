#pragma once

#include "ControlType.h"

#include <Arduino.h>

enum class BindingKind : uint8_t {
  Empty = 0,
  Note,
  Control
};

struct MatrixBinding {
  BindingKind kind;
  uint8_t value;
  ControlType control;
};

constexpr MatrixBinding kEmptyBinding{BindingKind::Empty, 0, ControlType::None};

constexpr MatrixBinding buildNote(uint8_t keyIndex) {
  return MatrixBinding{BindingKind::Note, keyIndex, ControlType::None};
}

constexpr MatrixBinding buildControl(ControlType control, uint8_t value = 0) {
  return MatrixBinding{BindingKind::Control, value, control};
}

constexpr MatrixBinding kMatrixBindings[9][8] = {
  {
    buildControl(ControlType::RhythmStartStop), // R0 C0
    buildControl(ControlType::TransposeDown),   // R0 C1
    buildControl(ControlType::TransposeUp),     // R0 C2
    buildControl(ControlType::TempoDown),       // R0 C3
    buildControl(ControlType::VolumeDown),      // R0 C4
    buildControl(ControlType::Panic),           // R0 C5
    buildControl(ControlType::EffectSelect),    // R0 C6
    buildControl(ControlType::EffectToggle)     // R0 C7
  },
  {
    buildControl(ControlType::InstrumentSelect, 0), // R1 C0
    buildControl(ControlType::InstrumentSelect, 1), // R1 C1
    buildControl(ControlType::InstrumentSelect, 2), // R1 C2
    buildControl(ControlType::InstrumentSelect, 3), // R1 C3
    buildControl(ControlType::InstrumentSelect, 4), // R1 C4
    buildControl(ControlType::InstrumentSelect, 5), // R1 C5
    buildControl(ControlType::InstrumentSelect, 6), // R1 C6
    buildControl(ControlType::InstrumentSelect, 7)  // R1 C7
  },
  {
    buildNote(36), // R2 C0
    buildControl(ControlType::AttackUp),  // R2 C1
    buildControl(ControlType::DecayUp),   // R2 C2
    buildControl(ControlType::SustainUp),  // R2 C3
    buildNote(32), // R2 C4
    buildNote(33), // R2 C5
    buildNote(34), // R2 C6
    buildNote(35)  // R2 C7
  },
  {
    buildNote(28), // R3 C0
    buildNote(29), // R3 C1
    buildNote(30), // R3 C2
    buildNote(31), // R3 C3
    buildNote(24), // R3 C4
    buildNote(25), // R3 C5
    buildNote(26), // R3 C6
    buildNote(27)  // R3 C7
  },
  {
    buildNote(20), // R4 C0
    buildNote(21), // R4 C1
    buildNote(22), // R4 C2
    buildNote(23), // R4 C3
    buildNote(16), // R4 C4
    buildNote(17), // R4 C5
    buildNote(18), // R4 C6
    buildNote(19)  // R4 C7
  },
  {
    buildNote(12), // R5 C0
    buildNote(13), // R5 C1
    buildNote(14), // R5 C2
    buildNote(15), // R5 C3
    buildNote(8),  // R5 C4
    buildNote(9),  // R5 C5
    buildNote(10), // R5 C6
    buildNote(11)  // R5 C7
  },
  {
    buildControl(ControlType::OctaveUp),      // R6 C0
    buildControl(ControlType::SustainToggle), // R6 C1
    buildControl(ControlType::ReleaseUp),     // R6 C2
    buildControl(ControlType::TempoUp),       // R6 C3
    buildControl(ControlType::VolumeUp),      // R6 C4
    buildControl(ControlType::BankDown),      // R6 C5
    buildControl(ControlType::BankUp),        // R6 C6
    buildControl(ControlType::OctaveDown)     // R6 C7
  },
  {
    buildControl(ControlType::RhythmSelect, 0), // R7 C0
    buildControl(ControlType::RhythmSelect, 1), // R7 C1
    buildControl(ControlType::RhythmSelect, 2), // R7 C2
    buildControl(ControlType::RhythmSelect, 3), // R7 C3
    buildControl(ControlType::RhythmSelect, 4), // R7 C4
    buildControl(ControlType::RhythmSelect, 5), // R7 C5
    buildControl(ControlType::RhythmSelect, 6), // R7 C6
    buildControl(ControlType::RhythmSelect, 7)  // R7 C7
  },
  {
    buildNote(4), // R8 C0
    buildNote(5), // R8 C1
    buildNote(6), // R8 C2
    buildNote(7), // R8 C3
    buildNote(0), // R8 C4
    buildNote(1), // R8 C5
    buildNote(2), // R8 C6
    buildNote(3)  // R8 C7
  }
};
