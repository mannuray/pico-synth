#include "SynthEngine.h"
#include "AudioHot.h"
#include "Oscillator.h"
#include "Presets.h"

namespace {
constexpr uint32_t kLfoPhaseStep = static_cast<uint32_t>((5.0 * 4294967296.0) / config::kSampleRate);

// Per-voice stereo spread: voice 0 center-left, 1 center-right, 2 more left, ...
constexpr int16_t kStereoPan[8] = {-80, 80, -160, 160, -40, 40, -120, 120};
}  // namespace

InlineWorker SynthEngine::inlineWorker_;

SynthEngine::SynthEngine(AudioOutput &audioOutput, FeedbackSounds &feedback, IParallelWorker &worker)
    : audioOutput_(audioOutput), feedback_(feedback), worker_(worker) {
  voiceNoise_[1].seed = 0x9e3779b9u;  // independent noise for the second half
}

bool SynthEngine::begin() {
  rhythm_.begin(config::kSampleRate);
  rhythm_.setBpm(tempoBpm_);
  preset_ = presets::at(presetIndex_);
  rotary_.configure(preset_.organ);
  updateShape();
  return audioOutput_.begin(config::kSampleRate, config::kBitsPerSample);
}

void SynthEngine::noteOn(uint8_t midiNote) {
  Voice &voice = allocateVoice();
  const uint8_t voiceIndex = static_cast<uint8_t>(&voice - &voices_[0]);
  voiceStartOrder_[voiceIndex] = ++notesStarted_;
  // Organ percussion is single-trigger: only when no other key is held.
  bool otherKeyHeld = false;
  for (const Voice &other : voices_) {
    otherKeyHeld = otherKeyHeld || (&other != &voice && other.active() && !other.releasing());
  }
  voice.start(midiNote, preset_, kStereoPan[voiceIndex & 7], !otherKeyHeld, noise_);
}

// Sustain is applied upstream: note-offs for sustained keys arrive when it's released.
void SynthEngine::noteOff(uint8_t midiNote) {
  for (Voice &voice : voices_) {
    if (voice.active() && voice.midiNote() == midiNote) {
      voice.release();
    }
  }
}

void SynthEngine::allNotesOff() {
  for (Voice &voice : voices_) {
    voice.reset();
  }
}

void SynthEngine::setPreset(uint8_t presetIndex) {
  presetIndex_ = presetIndex % config::kPresetCount;
  preset_ = presets::at(presetIndex_);
  rotary_.configure(preset_.organ);
  updateShape();
}

void SynthEngine::setMasterVolume(int16_t volume) {
  masterVolume_ = constrain(volume, config::kMinMasterVolume, config::kMaxMasterVolume);
}

void SynthEngine::setEffect(EffectType effect) {
  effects_.select(effect);
}

void SynthEngine::setEffectEnabled(bool enabled) {
  effects_.setEnabled(enabled);
}

void SynthEngine::setTempoBpm(uint8_t bpm) {
  tempoBpm_ = constrain(bpm, config::kTempoMinBpm, config::kTempoMaxBpm);
  rhythm_.setBpm(tempoBpm_);
}

void SynthEngine::startRhythm(bool intro) {
  rhythm_.start(intro);
}

void SynthEngine::stopRhythm(bool ending) {
  rhythm_.stop(ending);
}

void SynthEngine::rhythmFill() {
  rhythm_.requestFill();
}

void SynthEngine::setRotaryFast(bool fast) {
  rotary_.setFast(fast);
}

void SynthEngine::setRhythmPattern(uint8_t patternIndex) {
  rhythm_.setPattern(patternIndex);
}

// Free voice first, then the quietest releasing voice, else the oldest note.
Voice &SynthEngine::allocateVoice() {
  for (Voice &voice : voices_) {
    if (!voice.active()) {
      return voice;
    }
  }

  Voice *quietestReleasing = nullptr;
  for (Voice &voice : voices_) {
    if (voice.releasing() && (quietestReleasing == nullptr || voice.level() < quietestReleasing->level())) {
      quietestReleasing = &voice;
    }
  }
  if (quietestReleasing != nullptr) {
    return *quietestReleasing;
  }

  uint8_t oldest = 0;
  for (uint8_t i = 1; i < config::kPolyphony; ++i) {
    if (voiceStartOrder_[i] < voiceStartOrder_[oldest]) {
      oldest = i;
    }
  }
  return voices_[oldest];
}

void SynthEngine::updateShape() {
  shape_ = computeVoiceShape(preset_);
}

// Renders in blocks of at most kAudioBlockFrames, the size of the scratch buffers.
void SynthEngine::renderFrames(size_t frameCount) {
  uint32_t processingUs = 0;
  while (frameCount > 0) {
    const size_t blockFrames = min<size_t>(frameCount, config::kAudioBlockFrames);
    processingUs += renderBlock(blockFrames);
    frameCount -= blockFrames;
  }
  lastProcessingUs_ = processingUs;
}

// Renders one half of the voices (0 = voices 0-3, 1 = voices 4-7) for the
// current block into voiceMix_[half].
void AUDIO_HOT(SynthEngine::renderVoiceHalf)(uint8_t half) {
  VoiceMix &mix = voiceMix_[half];
  memset(&mix, 0, sizeof(mix));
  for (uint8_t v = half * kVoicesPerHalf; v < (half + 1) * kVoicesPerHalf; ++v) {
    const size_t sounded = voices_[v].renderBlock(preset_, shape_, modulation_, blockFrames_,
                                                  mix.left, mix.right, voiceNoise_[half]);
    for (size_t i = 0; i < sounded; ++i) {
      ++mix.activeCount[i];
    }
  }
}

void AUDIO_HOT(SynthEngine::renderSecondHalf)(void *engine) {
  static_cast<SynthEngine *>(engine)->renderVoiceHalf(1);
}

// Returns the time spent computing the block, excluding the output write.
uint32_t AUDIO_HOT(SynthEngine::renderBlock)(size_t frameCount) {
  AudioEffect &effect = effects_.active();
  const uint32_t startUs = micros();

  // Feedback blips follow the volume, but stay audible near volume 0.
  const int32_t feedbackLevel = max<int32_t>(masterVolume_, 8192);

  if (musicMuted_) {
    for (size_t i = 0; i < frameCount; ++i) {
      const int32_t feedbackSample = (feedback_.renderSample() * feedbackLevel) >> 15;
      blockL_[i] = dsp::softClip(feedbackSample);
      blockR_[i] = dsp::softClip(feedbackSample);
    }
  } else {
    // Pass 1: the effect's LFO and per-voice modulation for each frame.
    const bool needsLfo = effect.usesLfo();
    for (size_t i = 0; i < frameCount; ++i) {
      int32_t lfo = 0;
      if (needsLfo) {
        lfoPhase_ += kLfoPhaseStep;
        lfo = dsp::triangle(lfoPhase_);
      }
      lfoPhases_[i] = lfoPhase_;
      lfoValues_[i] = lfo;
      modulation_[i] = effect.voiceModulation(EffectContext{lfoPhase_, lfo, osc::kSine});
    }

    // Pass 2: voices, half on the worker (second core), half here.
    blockFrames_ = frameCount;
    worker_.start(&SynthEngine::renderSecondHalf, this);
    renderVoiceHalf(0);
    worker_.wait();

    // Pass 3: mix, effect, rotary speaker, drums, feedback, output.
    for (size_t i = 0; i < frameCount; ++i) {
      int32_t tonalMixL = voiceMix_[0].left[i] + voiceMix_[1].left[i];
      int32_t tonalMixR = voiceMix_[0].right[i] + voiceMix_[1].right[i];
      const uint8_t activeVoiceCount = voiceMix_[0].activeCount[i] + voiceMix_[1].activeCount[i];
      if (activeVoiceCount > 1) {
        tonalMixL = (tonalMixL * 3) / (activeVoiceCount + 2);
        tonalMixR = (tonalMixR * 3) / (activeVoiceCount + 2);
      }

      // Clamp so the volume multiply stays within 32 bits.
      const int32_t mixL = constrain(tonalMixL, -65535, 65535);
      const int32_t mixR = constrain(tonalMixR, -65535, 65535);
      int32_t left = (mixL * masterVolume_) >> 15;
      int32_t right = (mixR * masterVolume_) >> 15;

      EffectContext ctx{lfoPhases_[i], lfoValues_[i], osc::kSine};
      effect.process(left, right, ctx);
      if (rotary_.enabled()) {
        rotary_.process(left, right);
      }

      // Drums join after the effect and the rotary speaker: they stay dry and
      // sound the same whichever preset is selected.
      const int32_t drums = (constrain(rhythm_.renderSample(), -65535, 65535) * masterVolume_) >> 15;
      const int32_t feedbackSample = (feedback_.renderSample() * feedbackLevel) >> 15;
      left += drums + feedbackSample;
      right += drums + feedbackSample;

      // Triangular dither (difference of two random bits: -1, 0 or +1) reduces
      // quantization noise on quiet tails.
      const uint32_t bits = noise_.next();
      const int32_t dither = static_cast<int32_t>(bits >> 31) - static_cast<int32_t>((bits >> 30) & 1u);
      blockL_[i] = dsp::softClip(left + dither);
      blockR_[i] = dsp::softClip(right + dither);
    }
  }

  const uint32_t processingUs = micros() - startUs;

  for (size_t i = 0; i < frameCount; ++i) {
    audioOutput_.writeStereo(blockL_[i], blockR_[i]);
  }
  return processingUs;
}
