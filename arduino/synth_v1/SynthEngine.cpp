#include "SynthEngine.h"
#include "Oscillator.h"
#include "Presets.h"

namespace {
constexpr uint32_t kLfoPhaseStep = static_cast<uint32_t>((5.0 * 4294967296.0) / config::kSampleRate);

// Per-voice stereo spread: voice 0 center-left, 1 center-right, 2 more left, ...
constexpr int16_t kStereoPan[8] = {-80, 80, -160, 160, -40, 40, -120, 120};
}  // namespace

SynthEngine::SynthEngine(AudioOutput &audioOutput) : audioOutput_(audioOutput) {}

bool SynthEngine::begin() {
  rhythm_.begin(config::kSampleRate);
  rhythm_.setBpm(tempoBpm_);
  rotary_.configure(presets::at(presetIndex_).organ);
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
  voice.start(midiNote, presets::at(presetIndex_), kStereoPan[voiceIndex & 7], !otherKeyHeld, noise_);
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
  rotary_.configure(presets::at(presetIndex_).organ);
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

void SynthEngine::setRhythmPlaying(bool enabled) {
  rhythm_.setPlaying(enabled);
}

void SynthEngine::setRhythmPattern(uint8_t patternIndex) {
  rhythm_.setPattern(patternIndex);
}

void SynthEngine::setAttackOffset(int8_t offset) {
  envelopeOffsets_.attack = offset;
  updateShape();
}

void SynthEngine::setDecayOffset(int8_t offset) {
  envelopeOffsets_.decay = offset;
  updateShape();
}

void SynthEngine::setSustainOffset(int8_t offset) {
  envelopeOffsets_.sustain = offset;
  updateShape();
}

void SynthEngine::setReleaseOffset(int8_t offset) {
  envelopeOffsets_.release = offset;
  updateShape();
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
  shape_ = computeVoiceShape(presets::at(presetIndex_), envelopeOffsets_);
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

// Returns the time spent computing the block, excluding the output write.
uint32_t SynthEngine::renderBlock(size_t frameCount) {
  const Preset &preset = presets::at(presetIndex_);
  AudioEffect &effect = effects_.active();
  const bool needsLfo = effect.usesLfo();

  const uint32_t startUs = micros();

  EffectContext ctx{0, 0, osc::kSine};

  for (size_t i = 0; i < frameCount; ++i) {
    ctx.lfo = 0;
    if (needsLfo) {
      lfoPhase_ += kLfoPhaseStep;
      ctx.lfo = dsp::triangle(lfoPhase_);
    }
    ctx.lfoPhase = lfoPhase_;
    const VoiceModulation modulation = effect.voiceModulation(ctx);

    int32_t tonalMixL = 0;
    int32_t tonalMixR = 0;
    uint8_t activeVoiceCount = 0;
    for (Voice &voice : voices_) {
      if (!voice.active()) continue;
      ++activeVoiceCount;
      const int32_t voiceSample = voice.render(preset, shape_, modulation, noise_);
      // Per-voice stereo panning: pan is -256..+256.
      const int32_t panR = 256 + voice.stereoPan();  // 0..512
      const int32_t panL = 512 - panR;               // 512..0
      tonalMixL += (voiceSample * panL) >> 9;
      tonalMixR += (voiceSample * panR) >> 9;
    }

    if (activeVoiceCount > 1) {
      tonalMixL = (tonalMixL * 3) / (activeVoiceCount + 2);
      tonalMixR = (tonalMixR * 3) / (activeVoiceCount + 2);
    }

    // Clamp so the volume multiply stays within 32 bits.
    const int32_t mixL = constrain(tonalMixL, -65535, 65535);
    const int32_t mixR = constrain(tonalMixR, -65535, 65535);
    int32_t left = (mixL * masterVolume_) >> 15;
    int32_t right = (mixR * masterVolume_) >> 15;

    effect.process(left, right, ctx);
    if (rotary_.enabled()) {
      rotary_.process(left, right);
    }

    // Drums join after the effect and the rotary speaker: they stay dry and
    // sound the same whichever preset is selected.
    const int32_t drums = (constrain(rhythm_.renderSample(), -65535, 65535) * masterVolume_) >> 15;
    left += drums;
    right += drums;

    // Triangular dither (difference of two random bits: -1, 0 or +1) reduces
    // quantization noise on quiet tails.
    const uint32_t bits = noise_.next();
    const int32_t dither = static_cast<int32_t>(bits >> 31) - static_cast<int32_t>((bits >> 30) & 1u);
    blockL_[i] = dsp::softClip(left + dither);
    blockR_[i] = dsp::softClip(right + dither);
  }

  const uint32_t processingUs = micros() - startUs;

  for (size_t i = 0; i < frameCount; ++i) {
    audioOutput_.writeStereo(blockL_[i], blockR_[i]);
  }
  return processingUs;
}
