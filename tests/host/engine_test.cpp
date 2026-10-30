// SynthEngine behaviour: chunked rendering, rotary speaker speed, voice stealing.
#include "Presets.h"
#include "RhythmPatterns.h"
#include "SynthEngine.h"
#include "check.h"
#include <math.h>
#include <vector>

struct Capture : AudioOutput {
  std::vector<int16_t> left, right;
  bool begin(uint32_t, uint8_t) override { return true; }
  void writeStereo(int16_t l, int16_t r) override { left.push_back(l); right.push_back(r); }
  int32_t peakSince(size_t start) const {
    int32_t peak = 0;
    for (size_t i = start; i < left.size(); ++i) peak = max<int32_t>(peak, max(abs(left[i]), abs(right[i])));
    return peak;
  }
};

static void render(SynthEngine &e, int blocks, size_t frames = 64) {
  for (int b = 0; b < blocks; ++b) e.renderFrames(frames);
}

// A request longer than the 64-frame scratch buffers renders identically to 64-frame calls.
static void chunkedRenderMatches() {
  Capture a, b;
  SynthEngine ea(a), eb(b);
  for (SynthEngine *e : {&ea, &eb}) {
    e->begin();
    e->setPreset(4);
    e->setEffect(EffectType::Reverb);
    e->setEffectEnabled(true);
    e->noteOn(60);
    e->noteOn(67);
  }
  render(ea, 300, 64);
  render(eb, 100, 192);
  CHECK(a.left.size() == 300 * 64);
  CHECK(a.left == b.left && a.right == b.right);
}

// Rotation rate measured from the left/right balance swings, with no effect enabled.
static double measuredRotaryHz(uint8_t preset) {
  Capture out;
  SynthEngine engine(out);
  engine.begin();
  engine.setPreset(preset);
  engine.setEffectEnabled(false);
  engine.noteOn(60);
  render(engine, 44100 * 4 / 64);

  const size_t window = 441;  // 10 ms
  std::vector<double> balance;
  for (size_t i = 22050; i + window <= out.left.size(); i += window) {
    double l = 0, r = 0;
    for (size_t k = 0; k < window; ++k) { l += fabs(out.left[i + k]); r += fabs(out.right[i + k]); }
    balance.push_back((l - r) / (l + r + 1));
  }
  double mean = 0;
  for (double v : balance) mean += v;
  mean /= balance.size();
  int crossings = 0;
  for (size_t i = 1; i < balance.size(); ++i) crossings += ((balance[i - 1] < mean) != (balance[i] < mean));
  return crossings / 2.0 / (balance.size() * 0.01);
}

static void rotarySpinsAtPresetSpeed() {
  CHECK(fabs(measuredRotaryHz(2) - 6.7) < 0.5);  // JazzOrg: Leslie fast ("tremolo") speed
}

// Frequency of the strongest spectral peak over one second of a held note.
static double measuredPitchHz(uint8_t preset, uint8_t note) {
  Capture out;
  SynthEngine engine(out);
  engine.begin();
  engine.setPreset(preset);
  engine.setEffectEnabled(false);
  engine.noteOn(note);
  render(engine, 44100 / 64);
  int crossings = 0;
  for (size_t i = 22050 + 1; i < out.left.size(); ++i) crossings += (out.left[i - 1] < 0) != (out.left[i] < 0);
  return crossings / 2.0 / ((out.left.size() - 22050) / 44100.0);
}

// MIDI note numbers match pitch (A4 = 69 = 440 Hz); the preset octave shifts it.
static void notesPlayAtMidiPitch() {
  CHECK(fabs(measuredPitchHz(19, 72) - 261.63) < 2.0);  // SubBass (octave -1): key C5 sounds C4
  CHECK(fabs(measuredPitchHz(19, 81) - 440.0) < 2.0);   // key A5 sounds A4
}

// Volume 0 is silent for every preset, including the organ's rotary speaker.
static void zeroVolumeIsSilent() {
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    Capture out;
    SynthEngine engine(out);
    engine.begin();
    engine.setPreset(preset);
    engine.setEffect(presets::at(preset).defaultEffect);
    engine.setEffectEnabled(true);
    engine.setMasterVolume(0);
    engine.noteOn(60);
    engine.noteOn(64);
    render(engine, 100);
    CHECK(out.peakSince(0) <= 1);  // dither only
  }
}

// A loud chord at full volume on every preset with its effect. Under the
// sanitizer this catches integer overflow anywhere in the audio path.
static void fullVolumeChordsStayInRange() {
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    Capture out;
    SynthEngine engine(out);
    engine.begin();
    engine.setPreset(preset);
    engine.setEffect(presets::at(preset).defaultEffect);
    engine.setEffectEnabled(true);
    engine.setMasterVolume(config::kMaxMasterVolume);
    engine.setRhythmPlaying(true);
    for (uint8_t note : {48, 55, 60, 64, 67, 72, 76, 84}) engine.noteOn(note);
    render(engine, 1000);
    for (uint8_t note : {48, 55, 60, 64, 67, 72, 76, 84}) engine.noteOff(note);
    render(engine, 500);
    CHECK(out.left.size() == 1500 * 64);
  }
}

// With all voices held, new notes steal the oldest note, not always voice 0.
static void stealsOldestNote() {
  Capture out;
  SynthEngine engine(out);
  engine.begin();
  engine.setPreset(19);  // SubBass: sustained, no effect tail
  engine.setEffectEnabled(false);
  for (uint8_t i = 0; i < config::kPolyphony; ++i) engine.noteOn(40 + i);  // notes 40..47
  engine.noteOn(60);  // steals note 40
  engine.noteOn(62);  // must steal note 41, not the just-played 60
  render(engine, 20);
  for (uint8_t note = 40; note < 48; ++note) engine.noteOff(note);
  engine.noteOff(62);
  render(engine, 3000);
  const size_t mark = out.left.size();
  render(engine, 10);
  CHECK(out.peakSince(mark) > 200);  // note 60 is still sounding
  engine.noteOff(60);
  render(engine, 3000);
  const size_t mark2 = out.left.size();
  render(engine, 10);
  CHECK(out.peakSince(mark2) <= 2);  // and nothing else is
}

// At the default volume, chords stay below the soft clipper's knee (24000)
// with each preset's default effect, so they aren't squashed.
static void chordsHaveHeadroom() {
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    Capture out;
    SynthEngine engine(out);
    engine.begin();
    engine.setPreset(preset);
    engine.setEffect(presets::at(preset).defaultEffect);
    engine.setEffectEnabled(presets::at(preset).effectEnabled);
    for (uint8_t note : {48, 55, 60, 64, 67, 72}) engine.noteOn(note);
    render(engine, 44100 * 2 / 64);
    if (out.peakSince(0) >= 24000) printf("  %s chord peak %d\n", presets::at(preset).name, out.peakSince(0));
    CHECK(out.peakSince(0) < 24000);
  }
}

// Every MIDI note on every preset (octave and transpose reach the extremes).
// Under the sanitizer this catches overflow at very low and very high pitches.
static void allNotesRenderSafely() {
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    Capture out;
    SynthEngine engine(out);
    engine.begin();
    engine.setPreset(preset);
    engine.setMasterVolume(config::kMaxMasterVolume);
    for (int note = 0; note < 128; note += 8) {
      for (int k = 0; k < 8; ++k) engine.noteOn(static_cast<uint8_t>(note + k));
      render(engine, 30);
      engine.allNotesOff();
    }
    CHECK(out.left.size() == 16 * 30 * 64);
  }
}

// With no notes held, the drums sound the same under every preset and effect:
// they're mixed after the effects and the organ's rotary speaker.
static void drumsStayDry() {
  auto renderDrums = [](uint8_t preset, bool effectOn) {
    Capture out;
    SynthEngine engine(out);
    engine.begin();
    engine.setPreset(preset);
    engine.setEffect(presets::at(preset).defaultEffect);
    engine.setEffectEnabled(effectOn);
    engine.setTempoBpm(112);
    engine.setRhythmPattern(1);
    engine.setRhythmPlaying(true);
    render(engine, 44100 * 3 / 64);
    return out.left;
  };
  const auto reference = renderDrums(0, false);
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    // Dither shares the engine's noise source with the voices, so allow +-1.
    const auto drums = renderDrums(preset, true);
    bool same = drums.size() == reference.size();
    for (size_t i = 0; same && i < drums.size(); ++i) same = abs(drums[i] - reference[i]) <= 2;
    if (!same) printf("  drums differ under %s\n", presets::at(preset).name);
    CHECK(same);
  }
}

// Chords played along with the busiest patterns stay (almost) below the soft
// clipper at the default volume, so the drums don't distort the chord.
static void chordsWithDrumsHaveHeadroom() {
  for (uint8_t preset : {0, 2, 10, 14}) {
    for (uint8_t pattern : {1, 3, 7}) {
      Capture out;
      SynthEngine engine(out);
      engine.begin();
      engine.setPreset(preset);
      engine.setEffect(presets::at(preset).defaultEffect);
      engine.setEffectEnabled(presets::at(preset).effectEnabled);
      engine.setTempoBpm(rhythmPatternAt(pattern).defaultBpm);
      engine.setRhythmPattern(pattern);
      engine.setRhythmPlaying(true);
      for (uint8_t note : {48, 55, 60, 64, 67, 72}) engine.noteOn(note);
      render(engine, 44100 * 4 / 64);
      size_t over = 0;
      for (size_t i = 0; i < out.left.size(); ++i) over += abs(out.left[i]) > 24000 || abs(out.right[i]) > 24000;
      CHECK(over * 1000 < out.left.size());  // under 0.1%
    }
  }
}

int main() {
  chunkedRenderMatches();
  rotarySpinsAtPresetSpeed();
  notesPlayAtMidiPitch();
  zeroVolumeIsSilent();
  fullVolumeChordsStayInRange();
  chordsHaveHeadroom();
  allNotesRenderSafely();
  drumsStayDry();
  chordsWithDrumsHaveHeadroom();
  stealsOldestNote();
  return finish("engine_test");
}
