#pragma once

#include "AudioOutput.h"
#include "Config.h"
#include "RhythmEngine.h"
#include "WaveTableData.h"

enum class Waveform : uint8_t {
  Triangle = 0,
  Saw,
  Square,
  Sine,
  Wavetable,
  FM
};

enum class EffectType : uint8_t {
  Vibrato = 0,
  Tremolo,
  Chorus,
  Reverb,
  Delay,
  Bitcrusher,
  Count
};

struct Preset {
  const char *name;
  Waveform waveform;
  uint16_t attackMs;
  uint16_t decayMs;
  uint8_t sustainLevel;
  uint16_t releaseMs;
  uint16_t brightness;
  uint8_t modulationDepth;
  uint8_t wavetableIndex;
  uint8_t fmRatio;
  uint8_t fmDepth;
  bool unison;
  uint8_t detune;
  uint8_t resonance;
  uint8_t fmFeedback;
  uint16_t modDecayMs;
  // FM pairs 2-3 (Algorithm 5 style: parallel carrier-modulator pairs).
  // Set level to 0 to disable a pair (zero CPU cost).
  uint8_t fm2Ratio;       // pair 2 modulator frequency ratio
  uint8_t fm2Depth;       // pair 2 modulation depth
  uint8_t fm2Level;       // pair 2 output mix (0=off, 255=full)
  uint8_t fm3Ratio;       // pair 3 modulator frequency ratio
  uint8_t fm3Depth;       // pair 3 modulation depth
  uint8_t fm3Level;       // pair 3 output mix (0=off, 255=full)
  uint8_t vibratoRate;    // built-in vibrato speed (0=off, 1-255 maps to ~2-8Hz)
  uint8_t vibratoDepth;   // built-in vibrato intensity (0-255)
  EffectType defaultEffect; // auto-enabled effect for this preset
};

class SynthEngine {
public:
  explicit SynthEngine(AudioOutput &audioOutput);

  bool begin();
  void noteOn(uint8_t midiNote);
  void noteOff(uint8_t midiNote);
  void allNotesOff();
  void setSustainEnabled(bool enabled);
  bool isSustainEnabled() const;
  void setPreset(uint8_t presetIndex);
  void setMasterVolume(int16_t volume);
  void setEffect(EffectType effect);
  void setEffectEnabled(bool enabled);
  void setMetronome(bool enabled);
  void setMetronomeBpm(uint8_t bpm);
  void setRhythmPlaying(bool enabled);
  bool isRhythmPlaying() const;
  void setRhythmPattern(uint8_t patternIndex);
  uint8_t rhythmPattern() const;
  uint32_t lastProcessingUs() const { return lastProcessingUs_; }
  
  void setAttackOffset(int8_t offset);
  void setDecayOffset(int8_t offset);
  void setSustainOffset(int8_t offset);
  void setReleaseOffset(int8_t offset);

  void renderFrames(size_t frameCount, uint16_t micLevel);
  static const Preset &presetAt(uint8_t presetIndex);
  static const RhythmPattern &rhythmPatternAt(uint8_t patternIndex);

private:
  enum class EnvelopeStage : uint8_t {
    Off = 0,
    Attack,
    Decay,
    Sustain,
    Release
  };

  struct ComputedEnvelope {
    int16_t attackRate = 1;    // exponential approach rate (Q15)
    int16_t decayRate = 1;     // exponential decay rate (Q15)
    int16_t releaseRate = 1;   // exponential release rate (Q15)
    int32_t sustainLevel = 0;
    uint8_t filterBase = 32;
    uint8_t filterEnvDepth = 64;
    int32_t baseCutoff = 32;       // precomputed: filterBase + brightness/7
    int32_t resonanceDamping = 180; // precomputed: 180 - resonance/2
  };

  struct Voice {
    bool active = false;
    uint8_t midiNote = 0;
    uint32_t phase = 0;
    uint32_t phaseStep = 0;
    uint32_t modPhase = 0;
    uint32_t modPhaseStep = 0;
    uint32_t percussionPhase = 0;
    uint32_t percussionPhaseStep = 0;
    uint32_t unisonPhase[2] = {0, 0};
    // FM pairs 2-3 phase accumulators
    uint32_t fm2CarrierPhase = 0;
    uint32_t fm2CarrierPhaseStep = 0;
    uint32_t fm2ModPhase = 0;
    uint32_t fm2ModPhaseStep = 0;
    uint32_t fm3CarrierPhase = 0;
    uint32_t fm3ModPhase = 0;
    uint32_t fm3ModPhaseStep = 0;
    int32_t level = 0;
    int32_t modLevel = 0;
    int32_t modDecayStep = 0;
    int32_t percussionLevel = 0;
    int32_t percussionDecayStep = 0;
    int32_t clickLevel = 0;
    int16_t lastModOutput = 0;
    int32_t filterLow = 0;
    int32_t filterBand = 0;
    uint32_t vibratoPhase = 0;    // per-voice vibrato LFO phase
    uint32_t vibratoStep = 0;     // precomputed vibrato phase increment
    int16_t stereoPan = 0;        // -256 (full left) to +256 (full right)
    EnvelopeStage stage = EnvelopeStage::Off;
  };

  static constexpr size_t kDelaySamples = 1024;
  static constexpr size_t kChorusBufferSamples = 512;
  static constexpr size_t kReverbShortSamples = 389;
  static constexpr size_t kReverbMediumSamples = 617;
  static constexpr size_t kReverbLongSamples = 863;
  static constexpr size_t kReverbXLargeSamples = 1103;
  static constexpr size_t kWaveTableSize = 512;
  static constexpr uint8_t kFrameCount = 4;

  AudioOutput &audioOutput_;
  Voice voices_[config::kPolyphony];
  uint8_t presetIndex_ = 0;
  int16_t masterVolume_ = config::kDefaultMasterVolume;
  EffectType effect_ = EffectType::Vibrato;
  bool effectEnabled_ = false;
  bool metronomeEnabled_ = false;
  bool sustainEnabled_ = false;
  uint8_t metronomeBpm_ = config::kMetronomeDefaultBpm;
  uint32_t metronomeSamplesUntilClick_ = 0;
  uint16_t metronomeClickSamples_ = 0;
  uint32_t metronomePhase_ = 0;
  bool metronomeIsAccentBeat_ = false;
  int32_t bitcrushHeld_ = 0;
  uint8_t bitcrushCounter_ = 0;
  int16_t delayBuffer_[kDelaySamples] = {};
  size_t delayIndex_ = 0;
  int16_t chorusBuffer_[kChorusBufferSamples] = {};
  size_t chorusIndex_ = 0;
  int16_t reverbBufferL1_[kReverbShortSamples] = {};
  int16_t reverbBufferL2_[kReverbLongSamples] = {};
  int16_t reverbBufferR1_[kReverbMediumSamples] = {};
  int16_t reverbBufferR2_[kReverbXLargeSamples] = {};
  size_t reverbIndexL1_ = 0;
  size_t reverbIndexL2_ = 0;
  size_t reverbIndexR1_ = 0;
  size_t reverbIndexR2_ = 0;
  uint32_t lastProcessingUs_ = 0;
  uint32_t lfoPhase_ = 0;
  RhythmEngine rhythm_;
  ComputedEnvelope envelope_;
  int8_t attackOffset_ = 0;
  int8_t decayOffset_ = 0;
  int8_t sustainOffset_ = 0;
  int8_t releaseOffset_ = 0;
  bool tablesInitialized_ = false;
  uint32_t noiseSeed_ = 1;
  // Sine table and wavetable data loaded from flash (WaveTableData.h) — no RAM cost.

  const int16_t* sineTable() const { return kSineTable; }

  Voice *allocateVoice(uint8_t midiNote);
  static uint32_t midiNoteToPhaseStep(uint8_t midiNote);
  int16_t interpolateTable(const int16_t *table, uint32_t phase) const;
  int16_t sampleWave(Waveform waveform, uint32_t phase, uint8_t wavetableIndex = 0, uint32_t phaseStep = 0, int32_t framePos = 0) const;
  int32_t renderVoice(Voice &voice, const Preset &preset, int32_t lfo, bool metronomeAccent);
  void recomputeEnvelope(const Preset &preset);
  void initializeTables();
};
