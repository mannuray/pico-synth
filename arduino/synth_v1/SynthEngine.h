#pragma once

#include "AudioOutput.h"
#include "Config.h"
#include "Dsp.h"
#include "Effects.h"
#include "IAudioRenderer.h"
#include "RhythmEngine.h"
#include "RotarySpeaker.h"
#include "SynthTypes.h"
#include "Voice.h"
#include "VoiceShape.h"

// Polyphonic synth: allocates voices, mixes them with the drums, runs the
// master effect (or the organ's rotary speaker) and writes to the audio output.
class SynthEngine final : public IAudioRenderer {
public:
  explicit SynthEngine(AudioOutput &audioOutput);

  bool begin();
  void noteOn(uint8_t midiNote);
  void noteOff(uint8_t midiNote);
  void allNotesOff();
  void setPreset(uint8_t presetIndex);
  void setMasterVolume(int16_t volume);
  void setEffect(EffectType effect);
  void setEffectEnabled(bool enabled);
  void setTempoBpm(uint8_t bpm);
  void setRhythmPlaying(bool enabled);
  void setRhythmPattern(uint8_t patternIndex);

  void setAttackOffset(int8_t offset);
  void setDecayOffset(int8_t offset);
  void setSustainOffset(int8_t offset);
  void setReleaseOffset(int8_t offset);

  void renderFrames(size_t frameCount) override;
  uint32_t lastProcessingUs() const override { return lastProcessingUs_; }

private:
  AudioOutput &audioOutput_;
  Voice voices_[config::kPolyphony];
  uint32_t voiceStartOrder_[config::kPolyphony] = {};  // for stealing the oldest note
  uint32_t notesStarted_ = 0;
  uint8_t presetIndex_ = 0;
  VoiceShape shape_;
  EnvelopeOffsets envelopeOffsets_;
  int16_t masterVolume_ = config::kDefaultMasterVolume;
  uint8_t tempoBpm_ = config::kTempoDefaultBpm;
  uint32_t lastProcessingUs_ = 0;
  uint32_t lfoPhase_ = 0;
  RhythmEngine rhythm_;
  EffectRack effects_;
  RotarySpeaker rotary_;
  dsp::NoiseSource noise_;
  int16_t blockL_[config::kAudioBlockFrames] = {};
  int16_t blockR_[config::kAudioBlockFrames] = {};

  Voice &allocateVoice();
  void updateShape();
  uint32_t renderBlock(size_t frameCount);
};
