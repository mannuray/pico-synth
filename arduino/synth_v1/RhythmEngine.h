#pragma once

#include "Config.h"
#include "RhythmPatterns.h"

#include <Arduino.h>

// Plays the selected RhythmPattern from PCM drum samples, sample-accurately,
// with swing, per-hit velocity, light humanization, fills and crashes.
class RhythmEngine {
public:
  void begin(uint32_t sampleRate);
  void setBpm(uint8_t bpm);
  // intro: begin with the pattern's fill bar (a count-in), then the groove.
  void start(bool intro = false);
  // ending: finish the current bar, play a fill bar, then a final crash.
  void stop(bool ending = false);
  // Plays the fill on the next bar (restarting the 4-bar phrase).
  void requestFill();
  bool playing() const { return playing_; }
  // While playing, the new pattern starts at the next bar line.
  void setPattern(uint8_t patternIndex);

  // One sample of the drum mix. Not clipped: it is mixed with the voices and
  // goes through the engine's soft clipper.
  int32_t renderSample();

private:
  // A hit is queued with a delay (swing + humanization); the previous sound on
  // the voice keeps playing until the queued one starts, then is cut (choke).
  struct DrumVoice {
    bool active = false;
    const int16_t *sampleData = nullptr;
    size_t sampleLength = 0;
    size_t position = 0;
    int32_t amplitude = 256;   // Q8 gain (256 = unity)

    bool queued = false;
    const int16_t *queuedData = nullptr;
    size_t queuedLength = 0;
    int32_t queuedAmplitude = 0;
    uint16_t queuedDelay = 0;  // samples until the queued hit starts
  };

  enum Lane : uint8_t { kKickVoice = 0, kSnareVoice, kHatVoice, kPercVoice, kTomVoice, kCrashVoice, kVoiceCount };

  uint32_t sampleRate_ = config::kSampleRate;
  uint8_t bpm_ = config::kTempoDefaultBpm;
  bool playing_ = false;
  uint8_t currentPattern_ = 0;
  uint8_t pendingPattern_ = 0;  // applied at the next bar line while playing
  uint16_t currentStep_ = 0;
  uint8_t barInPhrase_ = 0;   // 0..3; bar 3 plays the fill
  bool crashNext_ = false;    // crash on the downbeat after a fill
  bool fillRequested_ = false;
  bool endingRequested_ = false;
  bool endingAfterFill_ = false;  // ending fill is playing: stop at the next downbeat
  uint32_t samplesUntilNextStep_ = 0;
  DrumVoice voices_[kVoiceCount];
  uint32_t noiseLfsr_ = 0x1aceb00c;

  const RhythmPattern &currentPatternDef() const;
  void onBarLine();
  void triggerStep(uint16_t step);
  void hit(uint8_t voice, uint8_t sampleId, char velocity, int32_t laneGain, uint16_t delay);
  uint32_t stepSamples() const;
};
