#pragma once

#include "AudioOutput.h"
#include "Config.h"
#include "Dsp.h"
#include "Effects.h"
#include "Feedback.h"
#include "IAudioRenderer.h"
#include "IParallelWorker.h"
#include "RhythmEngine.h"
#include "RotarySpeaker.h"
#include "SynthTypes.h"
#include "Voice.h"
#include "VoiceShape.h"

// Polyphonic synth: allocates voices, mixes them with the drums, runs the
// master effect and the organ's rotary speaker, and writes to the audio output.
class SynthEngine final : public IAudioRenderer {
public:
  // worker renders half of the voices in parallel (the RP2040's second core);
  // by default everything runs on the calling core.
  SynthEngine(AudioOutput &audioOutput, FeedbackSounds &feedback, IParallelWorker &worker = inlineWorker_);

  bool begin();
  void noteOn(uint8_t midiNote);
  void noteOff(uint8_t midiNote);
  void allNotesOff();
  void setPreset(uint8_t presetIndex);
  void setMasterVolume(int16_t volume);
  void setEffect(EffectType effect);
  void setEffectEnabled(bool enabled);
  void setTempoBpm(uint8_t bpm);
  void startRhythm(bool intro);
  void stopRhythm(bool ending);
  void rhythmFill();
  void setRhythmPattern(uint8_t patternIndex);
  void setRotaryFast(bool fast);

  void renderFrames(size_t frameCount) override;
  uint32_t lastProcessingUs() const override { return lastProcessingUs_; }
  void setMusicMuted(bool muted) override { musicMuted_ = muted; }
  bool feedbackActive() const override { return feedback_.active(); }

private:
  static constexpr size_t kBlock = config::kAudioBlockFrames;
  static constexpr uint8_t kVoicesPerHalf = config::kPolyphony / 2;

  // Voice output for one block from one half of the voices.
  struct VoiceMix {
    int32_t left[kBlock];
    int32_t right[kBlock];
    uint8_t activeCount[kBlock];  // voices sounding at each frame
  };

  static InlineWorker inlineWorker_;

  AudioOutput &audioOutput_;
  FeedbackSounds &feedback_;
  IParallelWorker &worker_;
  bool musicMuted_ = false;
  Voice voices_[config::kPolyphony];
  uint32_t voiceStartOrder_[config::kPolyphony] = {};  // for stealing the oldest note
  uint32_t notesStarted_ = 0;
  uint8_t presetIndex_ = 0;
  Preset preset_;  // RAM copy of the active preset
  VoiceShape shape_;
  int16_t masterVolume_ = config::kDefaultMasterVolume;
  uint8_t tempoBpm_ = config::kTempoDefaultBpm;
  uint32_t lastProcessingUs_ = 0;
  uint32_t lfoPhase_ = 0;
  RhythmEngine rhythm_;
  EffectRack effects_;
  RotarySpeaker rotary_;
  dsp::NoiseSource noise_;           // note starts and dither (main core)
  dsp::NoiseSource voiceNoise_[2];   // key-click noise, one per half (one per core)
  VoiceModulation modulation_[kBlock];
  uint32_t lfoPhases_[kBlock] = {};
  int32_t lfoValues_[kBlock] = {};
  VoiceMix voiceMix_[2];
  size_t blockFrames_ = 0;           // frames in the block being rendered
  int16_t blockL_[config::kAudioBlockFrames] = {};
  int16_t blockR_[config::kAudioBlockFrames] = {};

  Voice &allocateVoice();
  void updateShape();
  uint32_t renderBlock(size_t frameCount);
  void renderVoiceHalf(uint8_t half);
  static void renderSecondHalf(void *engine);
};
