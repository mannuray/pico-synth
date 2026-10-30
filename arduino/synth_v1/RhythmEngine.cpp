#include "RhythmEngine.h"
#include "DrumSamples.h"

namespace {
// Simple, popular rhythm patterns.
// 16 steps per bar (16th notes). MSB = step 0, LSB = step 15.
// x = hit, . = rest
//
// Step:  0123456789ABCDEF
// Beat:  1 . . . 2 . . . 3 . . . 4 . . .
//
const RhythmPattern kPatterns[kRhythmPatternCount] = {
  // Name          Kick        Snare       Hat         Perc        Accent     Perc#              Open
  // Pop: simple 4/4. Kick on 1&3, clap on 2&4, 8th note hats.
  {"Pop",        0x8080,     0x0808,     0xAAAA,     0x0000,     0x8808,     kDrumId_Clap,      false},

  // Rock: driving 8ths with steady kick, snare on 2&4.
  {"Rock",       0x9090,     0x0808,     0xAAAA,     0x0000,     0x8808,     kDrumId_Clave,     false},

  // Ballad: sparse and gentle, low tom fill on beat 3.
  {"Ballad",     0x8000,     0x0008,     0x8888,     0x0080,     0x8008,     kDrumId_LowTom,    false},

  // Disco: four-on-floor kick, open hat on offbeats, cowbell accents.
  {"Disco",      0x8888,     0x0808,     0x5555,     0x4444,     0x8888,     kDrumId_Cowbell,   true},

  // Funk: syncopated kick, clap on 2&4, rimshot ghost notes.
  {"Funk",       0x8020,     0x0808,     0xAAAA,     0x0040,     0x8008,     kDrumId_Clap,      false},

  // Latin: son clave 3-2 pattern with rimshot.
  {"Latin",      0x8020,     0x0000,     0xAAAA,     0x9200,     0x9008,     kDrumId_Clave,     false},

  // Waltz: 3/4 feel (emphasis on 1, lighter on 2&3).
  {"Waltz",      0x8000,     0x0020,     0x8820,     0x0000,     0x8000,     kDrumId_Clave,     false},

  // March: steady, militaristic, snare on every beat.
  {"March",      0x8080,     0x2828,     0xAAAA,     0x0000,     0x8808,     kDrumId_Rimshot,   false}
};
}

void RhythmEngine::begin(uint32_t sampleRate) {
  sampleRate_ = sampleRate;
  samplesUntilNextStep_ = stepSamples();
}

void RhythmEngine::setBpm(uint8_t bpm) {
  bpm_ = constrain(bpm, config::kTempoMinBpm, config::kTempoMaxBpm);
}

void RhythmEngine::setPlaying(bool playing) {
  playing_ = playing;
  if (playing_) {
    currentStep_ = 0;
    samplesUntilNextStep_ = 0;
  }
}

void RhythmEngine::setPattern(uint8_t patternIndex) {
  currentPattern_ = patternIndex % kRhythmPatternCount;
}

const RhythmPattern &RhythmEngine::currentPatternDef() const {
  return rhythmPatternAt(currentPattern_);
}

const RhythmPattern &rhythmPatternAt(uint8_t patternIndex) {
  return kPatterns[patternIndex % kRhythmPatternCount];
}

uint32_t RhythmEngine::stepSamples() const {
  return (sampleRate_ * 60u) / (static_cast<uint32_t>(bpm_) * 4u);
}

void RhythmEngine::triggerStep(uint8_t step) {
  const RhythmPattern &pattern = currentPatternDef();
  const uint16_t bit = static_cast<uint16_t>(1u << (15 - step));
  const bool accent = (pattern.accentMask & bit);
  const int32_t gain = accent ? 320 : 256;

  // Humanization jitter: 0-1.4ms
  noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);
  samplesUntilNextStep_ += (noiseLfsr_ & 0x3F);

  if (pattern.kickMask & bit) {
    startVoice(0, kDrumId_Kick, gain);
  }
  if (pattern.snareMask & bit) {
    startVoice(1, kDrumId_Snare, gain);
  }
  if (pattern.hatMask & bit) {
    startVoice(2, pattern.openHat ? kDrumId_OpenHat : kDrumId_ClosedHat, gain);
  }
  if (pattern.percMask & bit) {
    startVoice(3, pattern.percSample, gain);
  }
}

void RhythmEngine::startVoice(uint8_t index, uint8_t sampleId, int32_t gain) {
  if (sampleId >= kDrumSampleCount) return;
  DrumVoice &voice = voices_[index];
  voice.active = true;
  voice.sampleData = kDrumSamples[sampleId].data;
  voice.sampleLength = kDrumSamples[sampleId].length;
  voice.position = 0;
  voice.amplitude = gain;
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
    if (!voice.active) continue;
    if (voice.position >= voice.sampleLength) {
      voice.active = false;
      continue;
    }
    mix += (static_cast<int32_t>(voice.sampleData[voice.position]) * voice.amplitude) >> 8;
    voice.position++;
  }

  return static_cast<int16_t>(constrain(mix, -32000, 32000));
}
