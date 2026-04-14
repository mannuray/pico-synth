#include "SerialDebugSink.h"

void SerialDebugSink::handleEvent(const KeyboardEvent &event) {
  switch (event.type) {
    case KeyboardEventType::NoteOn:
      Serial.print("evt note_on ");
      Serial.println(event.note);
      break;
    case KeyboardEventType::NoteOff:
      Serial.print("evt note_off ");
      Serial.println(event.note);
      break;
    case KeyboardEventType::Control:
      Serial.print("evt control ");
      Serial.print(static_cast<uint8_t>(event.control));
      Serial.print(" value=");
      Serial.println(event.value);
      break;
  }
}

void SerialDebugSink::handleCommand(const PerformanceCommand &command) {
  Serial.print("cmd control ");
  Serial.print(static_cast<uint8_t>(command.control));
  Serial.print(" value=");
  Serial.println(command.value);
}
