#include "SerialDebugSink.h"

void SerialDebugSink::handleEvent(const KeyboardEvent &event) {
  switch (event.type) {
    case KeyboardEventType::NoteOn:
      out_.print("evt note_on ");
      out_.println(event.note);
      break;
    case KeyboardEventType::NoteOff:
      out_.print("evt note_off ");
      out_.println(event.note);
      break;
  }
}

void SerialDebugSink::handleCommand(const PerformanceCommand &command) {
  out_.print("cmd ");
  out_.print(static_cast<uint8_t>(command.type));
  out_.print(" value=");
  out_.println(command.value);
}
