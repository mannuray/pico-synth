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
  updateShape();
  return audioOutput_.begin(config::kSampleRate, config::kBitsPerSample);
}

void SynthEngine::noteOn(uint8_t midiNote) {
  Voice &voice = allocateVoice();
  const uint8_t voiceIndex = static_cast<uint8_t>(&voice - &voices_[0]);
  voice.start(midiNote, presets::at(presetIndex_), kStereoPan[voiceIndex & 7], noise_);
}

void SynthEngine::noteOff(uint8_t midiNote) {
  if (sustainEnabled_) {
    return;
  }
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

void SynthEngine::setSustainEnabled(bool enabled) {
  sustainEnabled_ = enabled;
}

void SynthEngine::setPreset(uint8_t presetIndex) {
  presetIndex_ = presetIndex % config::kPresetCount;
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
  tempoBpm_ = constrain(bpm, config::kMetronomeMinBpm, config::kMetronomeMaxBpm);
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

// Free voice first, then one already releasing, else steal voice 0.
Voice &SynthEngine::allocateVoice() {
  for (Voice &voice : voices_) {
    if (!voice.active()) {
      return voice;
    }
  }

  for (Voice &voice : voices_) {
    if (voice.releasing()) {
      return voice;
    }
  }

  return voices_[0];
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
  const OrganTraits *organ = preset.organ;

  const uint32_t startUs = micros();

  // The rotary speaker re-samples the first active voice. nullptr = none active.
  const Voice *rotarySource = nullptr;
  if (organ != nullptr) {
    for (const Voice &voice : voices_) {
      if (voice.active()) { rotarySource = &voice; break; }
    }
  }

  EffectContext ctx{0, 0, preset.modulationDepth, osc::kSine};

  for (size_t i = 0; i < frameCount; ++i) {
    ctx.lfo = 0;
    if (needsLfo) {
      lfoPhase_ += kLfoPhaseStep;
      ctx.lfo = dsp::triangle(lfoPhase_);
    }
    ctx.lfoPhase = lfoPhase_;
    const VoiceModulation modulation = effect.voiceModulation(ctx);
    if (organ != nullptr) {
      rotary_.advance(*organ);
    }

    const int32_t rhythmSample = rhythm_.renderSample();
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

    const int32_t mixL = tonalMixL + rhythmSample;
    const int32_t mixR = tonalMixR + rhythmSample;
    // Mono mix for effect processing.
    int32_t mix = ((mixL + mixR) >> 1) * masterVolume_ >> 15;
    int32_t wetL = 0;
    int32_t wetR = 0;
    const bool effectWet = effect.process(mix, wetL, wetR, ctx);

    if (organ != nullptr && rotarySource != nullptr) {
      // Rotary speaker replaces the effect's stereo output on organ presets.
      rotary_.render(*organ, preset, *rotarySource, mix, blockL_[i], blockR_[i]);
    } else if (effectWet) {
      blockL_[i] = dsp::softClip(wetL);
      blockR_[i] = dsp::softClip(wetR);
    } else {
      // Stereo output from per-voice panning (no effect output).
      // Add 1-bit triangular dither to reduce quantization noise on quiet tails.
      const int32_t dither = static_cast<int32_t>((noise_.next() >> 30) & 3) - 1; // -1, 0, or 1
      blockL_[i] = dsp::softClip(((mixL * masterVolume_) >> 15) + dither);
      blockR_[i] = dsp::softClip(((mixR * masterVolume_) >> 15) + dither);
    }
  }

  const uint32_t processingUs = micros() - startUs;

  for (size_t i = 0; i < frameCount; ++i) {
    audioOutput_.writeStereo(blockL_[i], blockR_[i]);
  }
  return processingUs;
}
