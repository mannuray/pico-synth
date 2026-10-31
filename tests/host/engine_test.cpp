// SynthEngine behaviour: chunked rendering, rotary speaker speed, voice stealing.
#include "Presets.h"
#include "catalog.h"
#include "RhythmPatterns.h"
#include "SampleSet.h"
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
  SynthEngine ea(a, gFeedback), eb(b, gFeedback);
  for (SynthEngine *e : {&ea, &eb}) {
    e->begin();
    e->setPreset(findPreset("Violin"));
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
static double measuredRotaryHz(uint8_t preset, bool fast) {
  Capture out;
  SynthEngine engine(out, gFeedback);
  engine.begin();
  engine.setPreset(preset);
  engine.setEffectEnabled(false);
  engine.setRotaryFast(fast);
  engine.noteOn(60);
  render(engine, 44100 * 8 / 64);

  const size_t window = 441;  // 10 ms
  std::vector<double> balance;
  for (size_t i = 44100 * 4; i + window <= out.left.size(); i += window) {  // after the speed ramp
    double l = 0, r = 0;
    for (size_t k = 0; k < window; ++k) { l += fabs(out.left[i + k]); r += fabs(out.right[i + k]); }
    balance.push_back((l - r) / (l + r + 1));
  }
  double mean = 0;
  for (double v : balance) mean += v;
  mean /= balance.size();
  // Strongest frequency of the balance swing (direct DFT scan, 0.3-10 Hz).
  double bestHz = 0, bestPower = 0;
  for (double hz = 0.3; hz <= 10.0; hz += 0.02) {
    double re = 0, im = 0;
    for (size_t k = 0; k < balance.size(); ++k) {
      const double t = k * 0.01;
      re += (balance[k] - mean) * cos(2 * M_PI * hz * t);
      im += (balance[k] - mean) * sin(2 * M_PI * hz * t);
    }
    if (re * re + im * im > bestPower) {
      bestPower = re * re + im * im;
      bestHz = hz;
    }
  }
  return bestHz;
}

static void rotarySpinsAtPresetSpeed() {
  CHECK(fabs(measuredRotaryHz(findPreset("Organ"), false) - 0.8) < 0.2);  // Leslie slow ("chorale")
  CHECK(fabs(measuredRotaryHz(findPreset("Organ"), true) - 6.7) < 0.5);   // and fast ("tremolo")
}

// Frequency of the strongest component within a semitone of `hz`, and its
// power (DFT scan in 0.5-cent steps over 16384 samples from 0.1 s).
struct Partial { double hz, power; };
static Partial strongestNear(const std::vector<int16_t> &x, double hz) {
  Partial best = {hz, 0};
  for (double c = -100; c <= 100; c += 0.5) {
    const double f = hz * pow(2.0, c / 1200);
    double re = 0, im = 0;
    for (size_t i = 4410; i < 4410 + 16384; ++i) {
      re += x[i] * cos(2 * M_PI * f * i / 44100);
      im += x[i] * sin(2 * M_PI * f * i / 44100);
    }
    if (re * re + im * im > best.power) best = {f, re * re + im * im};
  }
  return best;
}

// MIDI note numbers match pitch (A4 = 69 = 440 Hz), the preset octave included.
// Spectral, as sampled pianos and music boxes are slightly inharmonic: the
// partial at the note's frequency is close (recorded instruments aren't
// perfectly tuned) and nothing an octave or two below outweighs it.
// Bank 1 is within 10 cents. Some bank-2 recordings are further out at their
// own root key, as FluidSynth plays them too: Vibraphone F#6 +16, Sax F#3 -12,
// Steel Drum F#5 -11 cents; so 20 there.
static void notesPlayAtMidiPitch() {
  struct Check { const char *name; double maxCents; };
  for (const Check &c : {Check{"Organ", 10}, Check{"Violin", 10}, Check{"Trumpet", 10}, Check{"Piano", 10},
                         Check{"Guitar", 10}, Check{"MusicBox", 10}, Check{"EPiano", 20}, Check{"Strings", 20},
                         Check{"Flute", 20}, Check{"Sax", 20}, Check{"Clarinet", 20}, Check{"Harmonica", 20},
                         Check{"Vibraphone", 20}, Check{"SteelDrum", 20}}) {
    for (uint8_t note : {45, 57, 69, 81}) {
      Capture out;
      SynthEngine engine(out, gFeedback);
      engine.begin();
      engine.setPreset(findPreset(c.name));
      engine.setEffectEnabled(false);
      engine.noteOn(note);
      render(engine, 44100 / 64);
      const double expected = 440.0 * pow(2.0, (note - 69) / 12.0);
      const Partial f0 = strongestNear(out.left, expected);
      const double cents = 1200.0 * log2(f0.hz / expected);
      const double below = fmax(strongestNear(out.left, expected / 2).power, strongestNear(out.left, expected / 4).power);
      if (fabs(cents) >= c.maxCents || below > f0.power) printf("  %s note %d: %+.1f cents, sub-octave %.1f dB\n", c.name, note, cents, 10 * log10(below / f0.power));
      CHECK(fabs(cents) < c.maxCents);
      CHECK(below < f0.power);
    }
  }
}

// The organ's percussion rings at the 3rd harmonic of the sounding note
// (1320 Hz for A4), and fades: it follows the sample's pitch, not the zone's.
static void organPercussionOnThirdHarmonic() {
  Capture out;
  SynthEngine engine(out, gFeedback);
  engine.begin();
  engine.setPreset(findPreset("Organ"));
  engine.setEffectEnabled(false);
  engine.noteOn(69);
  render(engine, 44100 / 64);
  auto powerAt = [&](double hz, size_t start) {
    double re = 0, im = 0;
    for (size_t i = start; i < start + 2646; ++i) {  // 60 ms
      re += out.left[i] * cos(2 * M_PI * hz * i / 44100);
      im += out.left[i] * sin(2 * M_PI * hz * i / 44100);
    }
    return re * re + im * im;
  };
  CHECK(powerAt(1320, 0) > 100 * powerAt(1320, 22050));  // strong at first, gone by 0.5 s
  CHECK(powerAt(1320, 0) > 100 * powerAt(4 * 1320, 0));  // and not two octaves up
}

// Volume 0 is silent for every preset, including the organ's rotary speaker.
static void zeroVolumeIsSilent() {
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    Capture out;
    SynthEngine engine(out, gFeedback);
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
    SynthEngine engine(out, gFeedback);
    engine.begin();
    engine.setPreset(preset);
    engine.setEffect(presets::at(preset).defaultEffect);
    engine.setEffectEnabled(true);
    engine.setMasterVolume(config::kMaxMasterVolume);
    engine.startRhythm(false);
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
  SynthEngine engine(out, gFeedback);
  engine.begin();
  engine.setPreset(findPreset("Organ"));  // sustained, no effect tail
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
    SynthEngine engine(out, gFeedback);
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

// Every key a player can reach (octave and transpose included) sounds on
// every preset: no zone gap leaves a note silent.
static void everyKeySounds() {
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    for (int note = 12; note <= 108; ++note) {
      Capture out;
      SynthEngine engine(out, gFeedback);
      engine.begin();
      engine.setPreset(preset);
      engine.noteOn(static_cast<uint8_t>(note));
      render(engine, 70);  // 0.1 s
      int32_t peak = 0;
      for (int16_t v : out.left) peak = max<int32_t>(peak, abs(v));
      if (peak <= 200) printf("  %s note %d is silent (peak %d)\n", presets::at(preset).name, note, peak);
      CHECK(peak > 200);
    }
  }
}

// Sample zones are well formed: loop inside the data, and the guard sample
// after the end repeats the loop start, so interpolation across the loop seam
// (and at the very end) reads valid, continuous data.
static void sampleZonesAreSeamless() {
  CHECK(samples::rate() == 22050);
  for (uint8_t set = 0; set < samples::setCount(); ++set) {
    const SampleZone *previous = nullptr;
    for (int note = 0; note < 128; ++note) {
      const SampleZone &z = samples::zoneFor(set, static_cast<uint8_t>(note));
      if (&z == previous) continue;
      previous = &z;
      CHECK(z.loopStart < z.loopEnd && z.loopEnd <= z.length && z.loopEnd - z.loopStart >= 16);
      if (!z.looped) continue;
      CHECK(z.data[z.loopEnd] == z.data[z.loopStart]);
      // The loop end was cross-faded into the audio just before the loop start.
      CHECK(abs(z.data[z.loopEnd - 1] - z.data[z.loopStart - 1]) <= 1);
    }
  }
}

// Every MIDI note on every preset (octave and transpose reach the extremes).
// Under the sanitizer this catches overflow at very low and very high pitches.
static void allNotesRenderSafely() {
  for (uint8_t preset = 0; preset < config::kPresetCount; ++preset) {
    Capture out;
    SynthEngine engine(out, gFeedback);
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
    SynthEngine engine(out, gFeedback);
    engine.begin();
    engine.setPreset(preset);
    engine.setEffect(presets::at(preset).defaultEffect);
    engine.setEffectEnabled(effectOn);
    engine.setTempoBpm(112);
    engine.setRhythmPattern(findPattern("Rock"));
    engine.startRhythm(false);
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
  for (const char *presetName : {"Piano", "Organ", "Violin", "Trumpet"}) {
    for (const char *patternName : {"Rock", "Disco", "March"}) {
      const uint8_t preset = findPreset(presetName), pattern = findPattern(patternName);
      Capture out;
      SynthEngine engine(out, gFeedback);
      engine.begin();
      engine.setPreset(preset);
      engine.setEffect(presets::at(preset).defaultEffect);
      engine.setEffectEnabled(presets::at(preset).effectEnabled);
      engine.setTempoBpm(rhythmPatternAt(pattern).defaultBpm);
      engine.setRhythmPattern(pattern);
      engine.startRhythm(false);
      for (uint8_t note : {48, 55, 60, 64, 67, 72}) engine.noteOn(note);
      render(engine, 44100 * 4 / 64);
      size_t over = 0;
      for (size_t i = 0; i < out.left.size(); ++i) over += abs(out.left[i]) > 24000 || abs(out.right[i]) > 24000;
      CHECK(over * 1000 < out.left.size());  // under 0.1%
    }
  }
}

// Music muted (MIDI mode): notes and drums are silent, feedback blips still play.
static void mutedEnginePlaysOnlyFeedback() {
  Capture out;
  FeedbackSounds feedback;
  SynthEngine engine(out, feedback);
  engine.begin();
  engine.startRhythm(false);
  engine.noteOn(60);
  engine.setMusicMuted(true);
  render(engine, 100);
  CHECK(out.peakSince(0) == 0);
  feedback.cue(FeedbackCue::Up);
  CHECK(engine.feedbackActive());
  const size_t mark = out.left.size();
  render(engine, 20);
  CHECK(out.peakSince(mark) > 300);
  render(engine, 100);
  CHECK(!engine.feedbackActive());
}

int main() {
  chunkedRenderMatches();
  rotarySpinsAtPresetSpeed();
  notesPlayAtMidiPitch();
  organPercussionOnThirdHarmonic();
  zeroVolumeIsSilent();
  fullVolumeChordsStayInRange();
  chordsHaveHeadroom();
  allNotesRenderSafely();
  everyKeySounds();
  sampleZonesAreSeamless();
  drumsStayDry();
  chordsWithDrumsHaveHeadroom();
  mutedEnginePlaysOnlyFeedback();
  stealsOldestNote();
  return finish("engine_test");
}
