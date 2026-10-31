// Renders every preset with each effect setting (and once with drums) and
// prints one hash per case. run.sh compares the output with golden_expected.txt,
// so any change to the audio path shows up as a changed hash.
#include "Feedback.h"
#include "SynthEngine.h"
#include <new>
#include <stdio.h>

struct HashingOutput : AudioOutput {
  uint64_t hash = 1469598103934665603ull;  // FNV-1a
  bool begin(uint32_t, uint8_t) override { return true; }
  void writeStereo(int16_t l, int16_t r) override {
    for (int16_t s : {l, r}) {
      hash ^= static_cast<uint16_t>(s);
      hash *= 1099511628211ull;
    }
  }
};

static void render(SynthEngine &engine, int blocks) {
  for (int b = 0; b < blocks; ++b) engine.renderFrames(64);
}

static void runCase(int preset, int effect, bool rhythm) {
  static HashingOutput out;
  out = HashingOutput{};
  alignas(SynthEngine) static unsigned char storage[sizeof(SynthEngine)];
  static FeedbackSounds feedback;
  SynthEngine &engine = *new (storage) SynthEngine(out, feedback);
  engine.begin();
  engine.setPreset(preset);
  if (effect < 0) {
    engine.setEffectEnabled(false);
  } else {
    engine.setEffect(static_cast<EffectType>(effect));
    engine.setEffectEnabled(true);
  }
  if (rhythm) {
    engine.setTempoBpm(132);
    engine.setRhythmPattern(preset % 8);
    engine.startRhythm(false);
  }

  const uint8_t notes[] = {48, 60, 64, 67, 72};
  for (uint8_t note : notes) {
    engine.noteOn(note);
    render(engine, 40);
  }
  render(engine, 80);  // keys 48 and 60 held by sustain (controller withholds note-offs)
  for (uint8_t note : notes) engine.noteOff(note);
  render(engine, 400);

  engine.~SynthEngine();
  printf("p%02d e%2d r%d %016llx\n", preset, effect, rhythm, static_cast<unsigned long long>(out.hash));
}

int main() {
  for (int preset = 0; preset < 24; ++preset) {
    for (int effect = -1; effect < 6; ++effect) runCase(preset, effect, false);
    runCase(preset, -1, true);
  }
}
