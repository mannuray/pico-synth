#include "RhythmEngine.h"

namespace {
constexpr uint8_t kKick = 0;
constexpr uint8_t kSnare = 1;
constexpr uint8_t kHat = 2;
constexpr uint8_t kPerc = 3;

// Patterns: 16 steps, MSB = step 0, LSB = step 15.
// Groove-focused patterns with musical feel.
const RhythmPattern kPatterns[8] = {
  // Name          Kick        Snare       Hat         Perc        Accent
  {"Rock",       0x9292,     0x0808,     0xEEEE,     0x0000,     0x8808},  // Classic 4-on-floor with open hat feel
  {"Funk",       0x8060,     0x0829,     0xFFFF,     0x0420,     0x8028},  // Syncopated kick, busy hats, ghost snares
  {"HipHop",     0x8220,     0x0808,     0xABA8,     0x0110,     0x8008},  // Boom bap with lazy swing feel
  {"Disco",      0x8888,     0x0808,     0xBBBB,     0x4444,     0x8888},  // Four-on-floor with offbeat perc
  {"Latin",      0x9249,     0x0820,     0xAAAA,     0x2490,     0x9009},  // Clave-inspired with syncopation
  {"Reggae",     0x0080,     0x0800,     0xAAAA,     0x2020,     0x0800},  // One drop: kick on 3, snare on 3
  {"DnB",        0x8002,     0x0808,     0xFFFF,     0x2200,     0x8008},  // Breakbeat: kick on 1 and syncopated
  {"Waltz",      0x8008,     0x0200,     0x8888,     0x0000,     0x8000}   // 3/4 feel in 16 steps
};

uint32_t hzToStep(float hz, uint32_t sampleRate) {
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

  // Humanization: add 0-1.5ms timing jitter
  noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);
  samplesUntilNextStep_ += (noiseLfsr_ & 0x3F);

  auto trigger = [&](uint8_t voiceIdx, uint8_t type) {
    startVoice(voiceIdx, type);
    if (accent) {
      voices_[voiceIdx].amplitude = (voices_[voiceIdx].amplitude * 140) >> 7;
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
  voice.age = 0;

  switch (type) {
    case kKick:
      voice.amplitude = 26000;
      voice.phaseStep = hzToStep(180.0f, sampleRate_);  // Start higher for punch
      voice.remainingSamples = sampleRate_ / 5;          // 200ms
      break;
    case kSnare:
      voice.amplitude = 18000;
      voice.phaseStep = hzToStep(180.0f, sampleRate_);   // Tonal body
      voice.remainingSamples = sampleRate_ / 7;           // ~140ms
      break;
    case kHat:
      voice.amplitude = 9000;
      voice.phaseStep = hzToStep(8500.0f, sampleRate_);  // Metallic ring
      voice.remainingSamples = sampleRate_ / 20;          // 50ms closed hat
      break;
    case kPerc:
    default:
      voice.amplitude = 14000;
      voice.phaseStep = hzToStep(1200.0f, sampleRate_);  // Cowbell/rimshot
      voice.remainingSamples = sampleRate_ / 15;          // ~66ms
      break;
  }
}

int16_t RhythmEngine::renderVoice(DrumVoice &voice) {
  if (!voice.active || voice.remainingSamples == 0) {
    voice.active = false;
    return 0;
  }

  voice.age++;
  int32_t sample = 0;

  switch (voice.type) {
    case kKick: {
      // Punchy kick: click transient + sine body with pitch sweep.
      int32_t body = 0;
      if (sineTable_) {
        // 512-sample table: use top 9 bits for index
        const uint16_t idx = static_cast<uint16_t>(voice.phase >> 23) & 0x1FFu;
        body = (static_cast<int32_t>(sineTable_[idx]) * voice.amplitude) >> 15;
      }

      // Sharp click transient in first ~1ms (44 samples)
      if (voice.age < 44) {
        noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);
        const int32_t click = ((noiseLfsr_ & 1u) ? 1 : -1) * (voice.amplitude >> 1);
        const int32_t clickEnv = (44 - voice.age) * voice.amplitude / 44;
        body += (click * clickEnv) >> 15;
      }

      sample = body;
      voice.phase += voice.phaseStep;

      // Exponential pitch sweep: 180Hz → ~38Hz
      if (voice.age < 200) {
        voice.phaseStep -= voice.phaseStep >> 6;  // ~1.5% drop per sample
        const uint32_t floor = hzToStep(38.0f, sampleRate_);
        if (voice.phaseStep < floor) voice.phaseStep = floor;
      }

      // Fast initial decay then slow body sustain
      if (voice.age < 80) {
        voice.amplitude -= voice.amplitude >> 7;  // Fast attack decay
      } else {
        voice.amplitude -= voice.amplitude >> 9;  // Slow body decay
      }
      break;
    }

    case kSnare: {
      // Snare: bright noise crack + tonal body + snare wire tail.
      noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);

      // Noise component (snare wire + crack)
      const int32_t noise = ((noiseLfsr_ & 1u) ? 1 : -1) * voice.amplitude;

      // Tonal body (~180Hz sine)
      int32_t tone = 0;
      if (sineTable_) {
        const uint16_t idx = static_cast<uint16_t>(voice.phase >> 23) & 0x1FFu;
        tone = (static_cast<int32_t>(sineTable_[idx]) * voice.amplitude) >> 16;
      }
      voice.phase += voice.phaseStep;

      // Mix: more noise than tone, noise ratio increases over time
      if (voice.age < 60) {
        // Initial crack: mostly noise + some body
        sample = (noise * 5 + tone * 3) >> 3;
      } else {
        // Tail: snare wire buzz, less body
        sample = (noise * 6 + tone) >> 3;
      }

      // Two-stage decay: fast crack, then slower wire tail
      if (voice.age < 40) {
        voice.amplitude -= voice.amplitude >> 5;   // Sharp crack decay
      } else {
        voice.amplitude -= voice.amplitude >> 7;   // Slower wire tail
      }
      break;
    }

    case kHat: {
      // Hi-hat: metallic shimmer (two detuned square waves + filtered noise).
      noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);

      // Metallic ring: two detuned high-freq square waves
      const int32_t sq1 = (voice.phase & 0x80000000u) ? 1 : -1;
      const uint32_t phase2 = voice.phase + (voice.phase >> 3); // Slightly detuned
      const int32_t sq2 = (phase2 & 0x80000000u) ? 1 : -1;
      const int32_t metal = (sq1 + sq2) * (voice.amplitude >> 2);

      // Noise component
      const int32_t noise = ((noiseLfsr_ & 1u) ? 1 : -1) * (voice.amplitude >> 1);

      sample = (metal + noise) >> 1;
      voice.phase += voice.phaseStep;

      // Fast decay for closed hat character
      voice.amplitude -= voice.amplitude >> 5;
      break;
    }

    case kPerc:
    default: {
      // Percussion: cowbell-style (two detuned tones) or rimshot click.
      if (sineTable_) {
        // Primary tone
        const uint16_t idx1 = static_cast<uint16_t>(voice.phase >> 23) & 0x1FFu;
        const int32_t tone1 = (static_cast<int32_t>(sineTable_[idx1]) * voice.amplitude) >> 15;

        // Detuned second tone (minor third above = ~1.19x frequency)
        const uint32_t phase2 = voice.phase + (voice.phase >> 2) + (voice.phase >> 4);
        const uint16_t idx2 = static_cast<uint16_t>(phase2 >> 23) & 0x1FFu;
        const int32_t tone2 = (static_cast<int32_t>(sineTable_[idx2]) * voice.amplitude) >> 16;

        sample = tone1 + tone2;
      }

      // Sharp click at start
      if (voice.age < 20) {
        noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);
        sample += ((noiseLfsr_ & 1u) ? 1 : -1) * (voice.amplitude >> 2);
      }

      voice.phase += voice.phaseStep;
      voice.amplitude -= voice.amplitude >> 6;
      break;
    }
  }

  voice.remainingSamples--;
  if (voice.remainingSamples == 0 || voice.amplitude < 30) {
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

  return static_cast<int16_t>(constrain(mix, -24000, 24000));
}
