#pragma once

#include <Arduino.h>

// Short confirmation sounds for button presses (the keyboard has no display).
enum class FeedbackCue : uint8_t {
  Up,      // rising two notes: value went up / switched on
  Down,    // falling two notes: value went down / switched off
  Center,  // double blip: back to the default (octave 0, transpose 0)
  Limit,   // low "bonk": can't go further
  Blip,    // single blip: changed (volume, effect type)
  Count    // N blips: bank number
};

class IFeedback {
public:
  virtual ~IFeedback() = default;
  // count is used by FeedbackCue::Count (1-4).
  virtual void cue(FeedbackCue cue, uint8_t count = 1) = 0;
  // Turning feedback on or off confirms itself with Up/Down even when off.
  virtual void setEnabled(bool enabled) = 0;
  virtual bool enabled() const = 0;
};

// Plays feedback cues as short sine blips. Mixed into the output dry, after
// the effects, and never sent over MIDI.
class FeedbackSounds final : public IFeedback {
public:
  void cue(FeedbackCue cue, uint8_t count = 1) override;
  void setEnabled(bool enabled) override;
  bool enabled() const override { return enabled_; }

  bool active() const { return toneCount_ > 0; }
  int32_t renderSample();

private:
  struct Tone {
    uint16_t hz;
    uint16_t ms;
    uint16_t gapMs;  // silence after the tone
  };

  static constexpr uint8_t kMaxTones = 4;
  Tone tones_[kMaxTones] = {};
  uint8_t toneCount_ = 0;
  uint8_t toneIndex_ = 0;
  uint32_t phase_ = 0;
  uint32_t phaseStep_ = 0;
  uint32_t toneSamples_ = 0;   // length of the current tone
  uint32_t position_ = 0;      // position in the current tone, then its gap
  bool enabled_ = true;

  void play(const Tone *tones, uint8_t count);
  void startTone();
};
