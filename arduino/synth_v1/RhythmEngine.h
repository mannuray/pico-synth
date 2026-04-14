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
  uint8_t percSample;  // which perc sample to use (index into kDrumSamples)
  bool openHat;         // use open hat instead of closed on hat hits
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
    const int16_t* sampleData = nullptr;
    size_t sampleLength = 0;
    size_t position = 0;
    int32_t amplitude = 256;  // Q8 gain (256 = unity)
  };

  static constexpr uint8_t kPatternCount = 8;
  static constexpr uint8_t kVoiceCount = 4;

  uint32_t sampleRate_ = config::kSampleRate;
  uint8_t bpm_ = config::kMetronomeDefaultBpm;
  bool playing_ = false;
  uint8_t currentPattern_ = 0;
  uint8_t currentStep_ = 0;
  uint32_t samplesUntilNextStep_ = 0;
  DrumVoice voices_[kVoiceCount];
  uint32_t noiseLfsr_ = 0x1aceb00c;
  const int16_t* sineTable_ = nullptr;

  void triggerStep(uint8_t step);
  void startVoice(uint8_t index, uint8_t sampleId, int32_t gain);
  uint32_t stepSamples() const;
};
