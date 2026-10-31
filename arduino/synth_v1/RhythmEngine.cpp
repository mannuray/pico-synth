#include "RhythmEngine.h"
#include "AudioHot.h"
#include "DrumSamples.h"

#include <initializer_list>
#include <string.h>

namespace {
// Step grid:  1e&a2e&a3e&a4e&a   (16 steps = one 4/4 bar of 16th notes)
//
// Sources for the grooves: son clave 3-2 hits on 1, 2&, 4 | 2, 3 (two bars);
// disco = four-on-the-floor kick with open hats on the "and" of each beat;
// funk = ghost notes around the backbeat (never three in a row).
const RhythmPattern kPatterns[kRhythmPatternCount] = {
  {.name = "Pop", .defaultBpm = 100, .stepsPerBar = 16, .swingPercent = 50,
   .snareSample = kDrumId_Snare, .percSample = kDrumId_Clave,
   .kick      = "X.....x.X.......",
   .snare     = "....X.......X...",
   .hat       = "X.x.x.x.X.x.x.x.",
   .openHat   = nullptr, .perc = nullptr, .tom = nullptr,
   .fillKick  = nullptr,
   .fillSnare = "....X.......XxXx",
   .fillHat   = "X.x.x.x.X.x.....",
   .fillTom   = nullptr},

  {.name = "Rock", .defaultBpm = 112, .stepsPerBar = 16, .swingPercent = 50,
   .snareSample = kDrumId_Snare, .percSample = kDrumId_Clave,
   .kick      = "X.......X.x.....",
   .snare     = "....X.......X...",
   .hat       = "X.x.X.x.X.x.X.x.",
   .openHat   = nullptr, .perc = nullptr, .tom = nullptr,
   .fillKick  = "X.......X.......",
   .fillSnare = "....X...XxXx....",
   .fillHat   = "X.x.X.x.........",
   .fillTom   = "............HhLl"},

  {.name = "Ballad", .defaultBpm = 72, .stepsPerBar = 16, .swingPercent = 50,
   .snareSample = kDrumId_Rimshot, .percSample = kDrumId_Clave,  // cross-stick
   .kick      = "X.........x.....",
   .snare     = "....x.......x...",
   .hat       = "x.g.x.g.x.g.x.g.",
   .openHat   = nullptr, .perc = nullptr, .tom = nullptr,
   .fillKick  = nullptr,
   .fillSnare = "....x...........",
   .fillHat   = "x.g.x.g.........",
   .fillTom   = "........h.h.m.L."},

  {.name = "Disco", .defaultBpm = 120, .stepsPerBar = 16, .swingPercent = 50,
   .snareSample = kDrumId_Clap, .percSample = kDrumId_Cowbell,
   .kick      = "X...X...X...X...",
   .snare     = "....X.......X...",
   .hat       = "xg.gxg.gxg.gxg.g",
   .openHat   = "..x...x...x...x.",
   .perc      = nullptr, .tom = nullptr,
   .fillKick  = nullptr,
   .fillSnare = "....X...xxxxXxXx",
   .fillHat   = "xg.gxg.gx...x...",
   .fillTom   = nullptr},

  {.name = "Funk", .defaultBpm = 96, .stepsPerBar = 16, .swingPercent = 56,
   .snareSample = kDrumId_Snare, .percSample = kDrumId_Clave,
   .kick      = "X.x.......x..x..",
   .snare     = "....X..g.g..X..g",
   .hat       = "XgxgXgxgXgxgXg.g",
   .openHat   = "..............x.",
   .perc      = nullptr, .tom = nullptr,
   .fillKick  = nullptr,
   .fillSnare = "....X..g.g..XXxX",
   .fillHat   = "XgxgXgxgXgxg....",
   .fillTom   = nullptr},

  // Son clave 3-2 over two bars, with a tumbao-style kick ("and" of 2, beat 4).
  {.name = "Latin", .defaultBpm = 105, .stepsPerBar = 16, .swingPercent = 50,
   .snareSample = kDrumId_Rimshot, .percSample = kDrumId_Clave,
   .kick      = "......x.....X.........x.....X...",
   .snare     = nullptr,
   .hat       = "x.g.x.g.x.g.x.g.x.g.x.g.x.g.x.g.",
   .openHat   = nullptr,
   .perc      = "X.....X.....X.......X...X.......",
   .tom       = nullptr,
   .fillKick  = nullptr, .fillSnare = nullptr, .fillHat = nullptr, .fillTom = nullptr},

  // 3/4: "oom-pah-pah".
  {.name = "Waltz", .defaultBpm = 90, .stepsPerBar = 12, .swingPercent = 50,
   .snareSample = kDrumId_Snare, .percSample = kDrumId_Clave,
   .kick      = "X...........",
   .snare     = "....g...g...",
   .hat       = "....x...x...",
   .openHat   = nullptr, .perc = nullptr, .tom = nullptr,
   .fillKick  = nullptr,
   .fillSnare = "....g.......",
   .fillHat   = "....x.......",
   .fillTom   = "........m.L."},

  // Bass drum on every beat; snare drags into the accented 2 and 4.
  {.name = "March", .defaultBpm = 116, .stepsPerBar = 16, .swingPercent = 50,
   .snareSample = kDrumId_Snare, .percSample = kDrumId_Clave,
   .kick      = "X...x...X...x...",
   .snare     = "x.xxX...x.xxX.x.",
   .hat       = nullptr, .openHat = nullptr, .perc = nullptr, .tom = nullptr,
   .fillKick  = nullptr,
   .fillSnare = "x.xxX.xxXxXxXxXx",
   .fillHat   = nullptr,
   .fillTom   = nullptr},
};

// Mix level of each drum (Q8): kick and snare lead, cymbals sit lower.
// An accented kick peaks at ~0.86 of full scale, leaving room for the snare
// and hats that land with it.
constexpr int32_t kKickGain = 220;
constexpr int32_t kSnareGain = 185;
constexpr int32_t kHatGain = 100;
constexpr int32_t kPercGain = 150;
constexpr int32_t kTomGain = 170;
constexpr int32_t kCrashGain = 100;

// Level of the whole drum mix (Q8). Set so that playing chords along with the
// busiest patterns stays (almost) below the soft clipper at the default volume.
constexpr int32_t kDrumBusGain = 200;

// Velocity (Q8) for the step characters. Ghost notes ~-9 dB under normal hits.
int32_t velocityGain(char c) {
  switch (c) {
    case 'X': case 'H': case 'M': case 'L':
      return 256;
    case 'x': case 'h': case 'm': case 'l':
      return 190;
    case 'g':
      return 72;
    default:
      return 0;
  }
}

uint8_t tomSample(char c) {
  switch (c) {
    case 'H': case 'h':
      return kDrumId_HighTom;
    case 'M': case 'm':
      return kDrumId_MidTom;
    default:
      return kDrumId_LowTom;
  }
}

bool hasFill(const RhythmPattern &p) {
  return p.fillKick || p.fillSnare || p.fillHat || p.fillTom;
}

size_t patternLength(const RhythmPattern &p) {
  for (const char *lane : {p.kick, p.snare, p.hat, p.openHat, p.perc, p.tom}) {
    if (lane != nullptr) {
      return strlen(lane);
    }
  }
  return p.stepsPerBar;
}

char laneAt(const char *lane, uint16_t step) {
  return lane != nullptr ? lane[step] : '.';
}
}  // namespace

void RhythmEngine::begin(uint32_t sampleRate) {
  sampleRate_ = sampleRate;
  samplesUntilNextStep_ = stepSamples();
}

void RhythmEngine::setBpm(uint8_t bpm) {
  bpm_ = constrain(bpm, config::kTempoMinBpm, config::kTempoMaxBpm);
}

void RhythmEngine::start(bool intro) {
  playing_ = true;
  currentPattern_ = pendingPattern_;
  currentStep_ = 0;
  // An intro is simply the fill bar played first; its crash lands on bar 1.
  barInPhrase_ = (intro && hasFill(currentPatternDef())) ? 3 : 0;
  crashNext_ = false;
  fillRequested_ = false;
  endingRequested_ = false;
  endingAfterFill_ = false;
  samplesUntilNextStep_ = 0;
}

void RhythmEngine::stop(bool ending) {
  if (ending && playing_) {
    endingRequested_ = true;
  } else {
    playing_ = false;
  }
}

void RhythmEngine::requestFill() {
  if (playing_) {
    fillRequested_ = true;
  }
}

// Bar-line actions, in order: finish an ending, switch pattern, start an
// ending or requested fill.
void RhythmEngine::onBarLine() {
  if (endingAfterFill_) {
    hit(kCrashVoice, kDrumId_Crash, 'X', kCrashGain, 0);
    playing_ = false;
    endingAfterFill_ = false;
    crashNext_ = false;
    return;
  }
  if (pendingPattern_ != currentPattern_) {
    currentPattern_ = pendingPattern_;
    currentStep_ = 0;
    barInPhrase_ = 0;
    crashNext_ = false;
  }
  if (endingRequested_) {
    endingRequested_ = false;
    if (hasFill(currentPatternDef())) {
      barInPhrase_ = 3;
      endingAfterFill_ = true;
    } else {
      hit(kCrashVoice, kDrumId_Crash, 'X', kCrashGain, 0);
      playing_ = false;
      crashNext_ = false;
    }
    return;
  }
  if (fillRequested_) {
    fillRequested_ = false;
    barInPhrase_ = 3;
  }
}

void RhythmEngine::setPattern(uint8_t patternIndex) {
  pendingPattern_ = patternIndex % kRhythmPatternCount;
  if (!playing_) {
    currentPattern_ = pendingPattern_;
  }
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

void RhythmEngine::triggerStep(uint16_t step) {
  const RhythmPattern &p = currentPatternDef();

  // Phrase tracking: 4 bars, the last one is the fill, a crash follows it.
  if (step % p.stepsPerBar == 0) {
    if (crashNext_) {
      hit(kCrashVoice, kDrumId_Crash, 'X', kCrashGain, 0);
      crashNext_ = false;
    }
    if (barInPhrase_ == 3 && hasFill(p)) {
      crashNext_ = true;  // set at the fill's start, played at the next downbeat
    }
  }
  // Fill lanes are one bar long and replace the normal lanes on bar 4 of the phrase.
  const bool fill = barInPhrase_ == 3;
  const uint16_t barStep = step % p.stepsPerBar;
  auto charAt = [&](const char *normal, const char *fillLane) {
    return fill && fillLane ? fillLane[barStep] : laneAt(normal, step);
  };

  // Swing delays the off-beat 16ths: at 66% the pair splits 2:1 (triplet feel).
  const uint16_t swingDelay = (step % 2 == 1)
      ? static_cast<uint16_t>(stepSamples() * 2u * (p.swingPercent - 50u) / 100u)
      : 0;

  const char kick = charAt(p.kick, p.fillKick);
  const char snare = charAt(p.snare, p.fillSnare);
  const char hat = charAt(p.hat, p.fillHat);
  const char openHat = laneAt(p.openHat, step);
  const char perc = laneAt(p.perc, step);
  const char tom = charAt(p.tom, p.fillTom);

  if (kick != '.') hit(kKickVoice, kDrumId_Kick, kick, kKickGain, swingDelay);
  if (snare != '.') hit(kSnareVoice, p.snareSample, snare, kSnareGain, swingDelay);
  if (openHat != '.') {
    hit(kHatVoice, kDrumId_OpenHat, openHat, kHatGain, swingDelay);
  } else if (hat != '.') {
    hit(kHatVoice, kDrumId_ClosedHat, hat, kHatGain, swingDelay);
  }
  if (perc != '.') hit(kPercVoice, p.percSample, perc, kPercGain, swingDelay);
  if (tom != '.') hit(kTomVoice, tomSample(tom), tom, kTomGain, swingDelay);
}

void RhythmEngine::hit(uint8_t voiceIndex, uint8_t sampleId, char velocity, int32_t laneGain, uint16_t delay) {
  const int32_t gain = (velocityGain(velocity) * laneGain) >> 8;
  if (sampleId >= kDrumSampleCount || gain == 0) return;
  DrumVoice &voice = voices_[voiceIndex];
  voice.queued = true;
  voice.queuedData = kDrumSamples[sampleId].data;
  voice.queuedLength = kDrumSamples[sampleId].length;
  voice.queuedAmplitude = gain;

  // Humanization: start each hit 0-1.4 ms late, without moving the beat grid.
  noiseLfsr_ = (noiseLfsr_ >> 1) ^ (static_cast<uint32_t>(-(noiseLfsr_ & 1u)) & 0xD0000001u);
  voice.queuedDelay = static_cast<uint16_t>(delay + (noiseLfsr_ & 0x3F));
}

int32_t AUDIO_HOT(RhythmEngine::renderSample)() {
  if (playing_) {
    if (samplesUntilNextStep_ == 0 && currentStep_ % currentPatternDef().stepsPerBar == 0) {
      onBarLine();
    }
  }
  if (playing_) {
    if (samplesUntilNextStep_ == 0) {
      const RhythmPattern &p = currentPatternDef();
      const uint16_t length = static_cast<uint16_t>(patternLength(p));
      if (currentStep_ >= length) currentStep_ = 0;
      triggerStep(currentStep_);
      currentStep_ = static_cast<uint16_t>(currentStep_ + 1);
      if (currentStep_ % p.stepsPerBar == 0) {
        barInPhrase_ = static_cast<uint8_t>((barInPhrase_ + 1) % 4);
      }
      if (currentStep_ >= length) currentStep_ = 0;
      samplesUntilNextStep_ = stepSamples();
    }
    samplesUntilNextStep_--;
  }

  int32_t mix = 0;
  for (DrumVoice &voice : voices_) {
    if (voice.queued) {
      if (voice.queuedDelay > 0) {
        --voice.queuedDelay;
      } else {
        voice.active = true;
        voice.sampleData = voice.queuedData;
        voice.sampleLength = voice.queuedLength;
        voice.amplitude = voice.queuedAmplitude;
        voice.position = 0;
        voice.queued = false;
      }
    }
    if (!voice.active) continue;
    if (voice.position >= voice.sampleLength) {
      voice.active = false;
      continue;
    }
    mix += (static_cast<int32_t>(voice.sampleData[voice.position]) * voice.amplitude) >> 8;
    voice.position++;
  }

  return (mix * kDrumBusGain) >> 8;
}
