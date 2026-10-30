#pragma once

// Whether a computer is connected to receive MIDI.
class IHostLink {
public:
  virtual ~IHostLink() = default;
  virtual bool isConnected() = 0;
};
