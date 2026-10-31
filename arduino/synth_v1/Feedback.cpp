#include "Feedback.h"
#include "AudioHot.h"
#include "Config.h"
#include "Oscillator.h"

namespace {
constexpr int32_t kPeak = 3000;            // ~-21 dBFS before master volume: audible, unobtrusive
constexpr uint32_t kAttackSamples = 88;    // 2 ms fade-in so blips don't click

constexpr uint32_t msToSamples(uint32_t ms) {
  return ms * (config::kSampleRate / 1000);
}
}  // namespace

void FeedbackSounds::cue(FeedbackCue cue, uint8_t count) {
  if (!enabled_) {
    return;
  }
  switch (cue) {
    case FeedbackCue::Up: {
      static constexpr Tone tones[] = {{880, 35, 15}, {1320, 45, 0}};
      play(tones, 2);
      break;
    }
    case FeedbackCue::Down: {
      static constexpr Tone tones[] = {{1320, 35, 15}, {880, 45, 0}};
      play(tones, 2);
      break;
    }
    case FeedbackCue::Center: {
      static constexpr Tone tones[] = {{1047, 30, 40}, {1047, 30, 0}};
      play(tones, 2);
      break;
    }
    case FeedbackCue::Limit: {
      static constexpr Tone tones[] = {{220, 90, 0}};
      play(tones, 1);
      break;
    }
    case FeedbackCue::Blip: {
      static constexpr Tone tones[] = {{1200, 40, 0}};
      play(tones, 1);
      break;
    }
    case FeedbackCue::Count: {
      static constexpr Tone tones[] = {{1200, 40, 70}, {1200, 40, 70}, {1200, 40, 70}, {1200, 40, 0}};
      play(tones, static_cast<uint8_t>(constrain(count, 1, 4)));
      break;
    }
  }
}

void FeedbackSounds::setEnabled(bool enabled) {
  enabled_ = true;  // so the confirmation itself plays
  cue(enabled ? FeedbackCue::Up : FeedbackCue::Down);
  enabled_ = enabled;
}

void FeedbackSounds::play(const Tone *tones, uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    tones_[i] = tones[i];
  }
  toneCount_ = count;
  toneIndex_ = 0;
  startTone();
}

void FeedbackSounds::startTone() {
  const Tone &tone = tones_[toneIndex_];
  phase_ = 0;
  phaseStep_ = osc::hzToPhaseStep(tone.hz);
  toneSamples_ = msToSamples(tone.ms);
  position_ = 0;
}

int32_t AUDIO_HOT(FeedbackSounds::renderSample)() {
  if (toneCount_ == 0) {
    return 0;
  }
  const Tone &tone = tones_[toneIndex_];
  int32_t out = 0;
  if (position_ < toneSamples_) {
    // Quick fade-in, then a linear fade-out over the rest of the tone.
    const int32_t remaining = static_cast<int32_t>(toneSamples_ - position_);
    int32_t envelope = static_cast<int32_t>((static_cast<uint32_t>(kPeak) * remaining) / toneSamples_);
    if (position_ < kAttackSamples) {
      envelope = envelope * static_cast<int32_t>(position_) / static_cast<int32_t>(kAttackSamples);
    }
    phase_ += phaseStep_;
    out = (osc::sine(phase_) * envelope) >> 15;
  }
  if (++position_ >= toneSamples_ + msToSamples(tone.gapMs)) {
    if (++toneIndex_ >= toneCount_) {
      toneCount_ = 0;
    } else {
      startTone();
    }
  }
  return out;
}
