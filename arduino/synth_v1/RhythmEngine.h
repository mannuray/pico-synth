#pragma once

#include "Config.h"
#include "RhythmPatterns.h"

#include <Arduino.h>

class RhythmEngine {
public:
  void begin(uint32_t sampleRate);
  void setBpm(uint8_t bpm);
  void setPlaying(bool playing);
  void setPattern(uint8_t patternIndex);

  int16_t renderSample();

private:
  struct DrumVoice {
    bool active = false;
    const int16_t* sampleData = nullptr;
    size_t sampleLength = 0;
    size_t position = 0;
    int32_t amplitude = 256;  // Q8 gain (256 = unity)
  };

  static constexpr uint8_t kVoiceCount = 4;

  uint32_t sampleRate_ = config::kSampleRate;
  uint8_t bpm_ = config::kMetronomeDefaultBpm;
  bool playing_ = false;
  uint8_t currentPattern_ = 0;
  uint8_t currentStep_ = 0;
  uint32_t samplesUntilNextStep_ = 0;
  DrumVoice voices_[kVoiceCount];
  uint32_t noiseLfsr_ = 0x1aceb00c;

  const RhythmPattern &currentPatternDef() const;

  void triggerStep(uint8_t step);
  void startVoice(uint8_t index, uint8_t sampleId, int32_t gain);
  uint32_t stepSamples() const;
};
