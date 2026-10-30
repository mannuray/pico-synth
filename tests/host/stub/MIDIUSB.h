#pragma once
// Records the packets the firmware sends instead of talking to USB.
#include <stdint.h>
#include <vector>

struct midiEventPacket_t { uint8_t header, byte1, byte2, byte3; };

struct MidiUsbStub {
  std::vector<midiEventPacket_t> sent;
  void sendMIDI(midiEventPacket_t p) { sent.push_back(p); }
  void flush() {}
};

extern MidiUsbStub MidiUSB;
