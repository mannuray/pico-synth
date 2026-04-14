#include "RhythmEngine.h"

namespace {
constexpr uint8_t kKick = 0;
constexpr uint8_t kSnare = 1;
constexpr uint8_t kHat = 2;
constexpr uint8_t kPerc = 3;

const RhythmPattern kPatterns[8] = {
  // Name, Kick, Snare, Hat, Perc, Accent
  {"Techno",  0x8888, 0x0808, 0xAAAA, 0x2222, 0x8888},
  {"BoomBap", 0x8220, 0x0808, 0xABA8, 0x0110, 0x8008},
  {"Funk",    0x8060, 0x0829, 0xFFFF, 0x0000, 0x8020},
  {"Samba",   0x9999, 0x0000, 0xAAAA, 0x2490, 0x9009},
  {"Rock",    0x9292, 0x0808, 0xEEEE, 0x0000, 0x8000},
  {"Disco",   0x8888, 0x0808, 0xBBBB, 0x1111, 0x8888},
  {"Ambient", 0x8008, 0x0000, 0x8080, 0x4444, 0x8000},
  {"Trap",    0x8202, 0x0808, 0xFFF0, 0x000F, 0x8008}
};

uint32_t hzToPhaseStep(float hz, uint32_t sampleRate) {
  return static_cast<uint32_t>((hz * 4294967296.0f) / sampleRate);
}
}

void RhythmEngine::begin(uint32_t sampleRate, const int16_t* sineTable) {
  sampleRate_ = sampleRate;
  sineTable_ = sineTable;
  samplesUntilNextStep_ = stepSamples();
}

void RhythmEngine::setBpm(uint8_t bpm) {
  bpm_ = constrain(bpm, config::kMetronomeMinBpm, config::kMetronomeMaxBpm);
}

uint8_t RhythmEngine::bpm() const {
  return bpm_;
}

void RhythmEngine::setPlaying(bool playing) {
  playing_ = playing;
  if (playing_) {
    currentStep_ = 0;
    samplesUntilNextStep_ = 0;
  }
}

bool RhythmEngine::isPlaying() const {
  return playing_;
}

void RhythmEngine::setPattern(uint8_t patternIndex) {
  currentPattern_ = patternIndex % kPatternCount;
}

uint8_t RhythmEngine::currentPattern() const {
  return currentPattern_;
}

const RhythmPattern &RhythmEngine::currentPatternDef() const {
  return patternAt(currentPattern_);
}

const RhythmPattern &RhythmEngine::patternAt(uint8_t patternIndex) {
  return kPatterns[patternIndex % kPatternCount];
}

uint32_t RhythmEngine::stepSamples() const {
  return (sampleRate_ * 60u) / (static_cast<uint32_t>(bpm_) * 4u);
}

void RhythmEngine::triggerStep(uint8_t step) {
  const RhythmPattern &pattern = currentPatternDef();
  const uint16_t bit = static_cast<uint16_t>(1u << (15 - step));
  const bool accent = (pattern.accentMask & bit);

  // Humanization: add 0-2ms jitter to start time
  samplesUntilNextStep_ += (noiseLfsr_ & 0x1F); 

  auto trigger = [&](uint8_t voiceIdx, uint8_t type) {
    startVoice(voiceIdx, type);
    if (accent) {
      voices_[voiceIdx].amplitude = (voices_[voiceIdx].amplitude * 160) / 100;
    }
  };

  if (pattern.kickMask & bit) trigger(0, kKick);
  if (pattern.snareMask & bit) trigger(1, kSnare);
  if (pattern.hatMask & bit) trigger(2, kHat);
  if (pattern.percMask & bit) trigger(3, kPerc);
}

void RhythmEngine::startVoice(uint8_t index, uint8_t type) {
  DrumVoice &voice = voices_[index];
  voice.active = true;
  voice.type = type;
  voice.phase = 0;

  switch (type) {
    case kKick:
      voice.amplitude = 22000;
      voice.phaseStep = hzToPhaseStep(120.0f, sampleRate_);
      voice.remainingSamples = sampleRate_ / 6;
      break;
    case kSnare:
      voice.amplitude = 15000;
      voice.phaseStep = hzToPhaseStep(240.0f, sampleRate_);
      voice.remainingSamples = sampleRate_ / 10;
      break;
    case kHat:
      voice.amplitude = 10000;
      voice.phaseStep = hzToPhaseStep(6000.0f, sampleRate_);
      voice.remainingSamples = sampleRate_ / 30;
      break;
    case kPerc:
    default:
      voice.amplitude = 16000;
      voice.phaseStep = hzToPhaseStep(1800.0f, sampleRate_);
      voice.remainingSamples = sampleRate_ / 25;
      break;
  }
}

int16_t RhythmEngine::renderVoice(DrumVoice &voice) {
  if (!voice.active || voice.remainingSamples == 0) {
    voice.active = false;
    return 0;
  }

  int32_t sample = 0;
  switch (voice.type) {
    case kKick:
      if (sineTable_) {
        sample = (static_cast<int32_t>(sineTable_[voice.phase >> 24]) * voice.amplitude) >> 15;
      } else {
        sample = ((voice.phase & 0x80000000u) ? 1 : -1) * voice.amplitude;
      }
      voice.phase += voice.phaseStep;
      // Exponential frequency drop
      voice.phaseStep = (static_cast<uint64_t>(voice.phaseStep) * 990) / 1000;
      if (voice.phaseStep < hzToPhaseStep(32.0f, sampleRate_)) voice.phaseStep = hzToPhaseStep(32.0f, sampleRate_);
      voice.amplitude = (voice.amplitude * 990) / 1000;
      break;
    case kSnare: {
      noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);
      const int32_t noise = (noiseLfsr_ & 1u) ? (voice.amplitude) : -(voice.amplitude);
      int32_t tone = 0;
      if (sineTable_) {
        tone = (static_cast<int32_t>(sineTable_[voice.phase >> 24]) * voice.amplitude) >> 16;
      }
      sample = (noise * 3 + tone) / 4;
      voice.phase += voice.phaseStep;
      voice.amplitude = (voice.amplitude * 985) / 1000;
      break;
    }
    case kHat:
      noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);
      // Metallic Hat: high-pass filtered noise
      sample = (noiseLfsr_ & 1u) ? (voice.amplitude) : -(voice.amplitude);
      if (voice.remainingSamples > (sampleRate_ / 60)) {
         sample += (voice.phase & 0x80000000u) ? (voice.amplitude / 2) : -(voice.amplitude / 2);
      }
      voice.phase += voice.phaseStep;
      voice.amplitude = (voice.amplitude * 930) / 1000;
      break;
    case kPerc:
    default:
      if (sineTable_) {
        sample = (static_cast<int32_t>(sineTable_[voice.phase >> 24]) * voice.amplitude) >> 15;
      }
      voice.phase += voice.phaseStep;
      voice.amplitude = (voice.amplitude * 970) / 1000;
      break;
  }

  voice.remainingSamples--;
  if (voice.remainingSamples == 0 || voice.amplitude < 50) {
    voice.active = false;
  }

  return static_cast<int16_t>(constrain(sample, -32768, 32767));
}

int16_t RhythmEngine::renderSample() {
  if (playing_) {
    if (samplesUntilNextStep_ == 0) {
      triggerStep(currentStep_);
      currentStep_ = (currentStep_ + 1) % 16;
      samplesUntilNextStep_ = stepSamples();
    }
    samplesUntilNextStep_--;
  }

  int32_t mix = 0;
  for (DrumVoice &voice : voices_) {
    mix += renderVoice(voice);
  }

  return static_cast<int16_t>(constrain(mix, -20000, 20000));
}
