#pragma once

#include "Config.h"

#include <Arduino.h>

struct RhythmPattern {
  const char *name;
  uint16_t kickMask;
  uint16_t snareMask;
  uint16_t hatMask;
  uint16_t percMask;
  uint16_t accentMask;
};

class RhythmEngine {
public:
  void begin(uint32_t sampleRate, const int16_t* sineTable = nullptr);
  void setBpm(uint8_t bpm);
  uint8_t bpm() const;

  void setPlaying(bool playing);
  bool isPlaying() const;

  void setPattern(uint8_t patternIndex);
  uint8_t currentPattern() const;
  const RhythmPattern &currentPatternDef() const;

  int16_t renderSample();
  static const RhythmPattern &patternAt(uint8_t patternIndex);

private:
  struct DrumVoice {
    bool active = false;
    uint8_t type = 0;
    int32_t amplitude = 0;
    uint32_t phase = 0;
    uint32_t phaseStep = 0;
    uint16_t remainingSamples = 0;
  };

  static constexpr uint8_t kPatternCount = 8;

  uint32_t sampleRate_ = config::kSampleRate;
  uint8_t bpm_ = config::kMetronomeDefaultBpm;
  bool playing_ = false;
  uint8_t currentPattern_ = 0;
  uint8_t currentStep_ = 0;
  uint32_t samplesUntilNextStep_ = 0;
  DrumVoice voices_[4];
  uint32_t noiseLfsr_ = 0x1aceb00c;
  const int16_t* sineTable_ = nullptr;

  void triggerStep(uint8_t step);
  void startVoice(uint8_t index, uint8_t type);
  int16_t renderVoice(DrumVoice &voice);
  uint32_t stepSamples() const;
};
