#pragma once

#include <Arduino.h>

class IAudioRenderer {
public:
  virtual ~IAudioRenderer() = default;
  virtual void renderFrames(size_t frameCount) = 0;
  virtual uint32_t lastProcessingUs() const = 0;

  // Muted: only feedback sounds play (MIDI mode keeps the speaker otherwise quiet).
  virtual void setMusicMuted(bool muted) = 0;
  // A feedback sound is still playing, so rendering is needed even when muted.
  virtual bool feedbackActive() const = 0;
};
