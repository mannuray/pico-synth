// SynthEngine behaviour: chunked rendering, rotary speaker speed, voice stealing.
#include "Presets.h"
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
  CHECK(fabs(measuredRotaryHz(0) - 2.4) < 0.3);  // JazzOrg
  CHECK(fabs(measuredRotaryHz(1) - 4.8) < 0.3);  // FullOrg
}

// With all voices held, new notes steal the oldest note, not always voice 0.
static void stealsOldestNote() {
  Capture out;
  SynthEngine engine(out);
  engine.begin();
  engine.setPreset(20);  // SubBass: sustained, no effect tail
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

int main() {
  chunkedRenderMatches();
  rotarySpinsAtPresetSpeed();
  stealsOldestNote();
  return finish("engine_test");
}
