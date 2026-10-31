// Full control path through AppController (with fake hardware) to the exact USB MIDI bytes.
#include "AppController.h"
#include "MidiEventSink.h"
#include "SynthEngine.h"
#include "SynthEventSink.h"
#include <MIDIUSB.h>
#include "catalog.h"
#include "check.h"
#include <string>
#include <vector>
MidiUsbStub MidiUSB;

std::string wire() {  // "B0 07 4F | C0 01 00 | ..." then clear
  std::string s; char buf[16];
  for (auto &p : MidiUSB.sent) { snprintf(buf, sizeof buf, "%s%02X %02X %02X", s.empty() ? "" : " | ", p.byte1, p.byte2, p.byte3); s += buf; }
  MidiUSB.sent.clear(); return s;
}
// Key matrix fake: the test queues button presses/releases.
struct ScriptedKeys : IKeyMatrix {
  std::vector<KeyMatrixEvent> queue;
  void push(uint8_t row, uint8_t col, bool pressed) { queue.push_back({row, col, pressed}); }
  bool poll(KeyMatrixEvent &e) override {
    if (queue.empty()) return false;
    e = queue.front();
    queue.erase(queue.begin());
    return true;
  }
};
struct Level : ILevelInput { uint16_t readLevel() override { return 0; } };
struct Renderer : IAudioRenderer {
  int calls = 0; bool muted = false; bool feedback = false;
  void renderFrames(size_t) override { ++calls; }
  uint32_t lastProcessingUs() const override { return 0; }
  void setMusicMuted(bool m) override { muted = m; }
  bool feedbackActive() const override { return feedback; }
};
struct Host : IHostLink { bool up = false; bool isConnected() override { return up; } };
struct Led : IStatusIndicator { void flash(uint32_t) override {} void update(uint32_t) override {} };
struct Clock : IClock { uint32_t ms = 0; uint32_t nowMs() const override { return ms; } uint32_t nowUs() const override { return ms * 1000; } };
struct Silent : AudioOutput { bool begin(uint32_t, uint8_t) override { return true; } void writeStereo(int16_t, int16_t) override {} };

int main() {
  Print log; Silent audio; SynthEngine synth(audio, gFeedback); synth.begin();
  SynthEventSink synthSink(synth); MidiEventSink midiSink;
  OutputRouter router(synthSink, midiSink, OutputMode::SynthOnly);
  PerformanceController perf(router, gFeedback, log);
  ScriptedKeys keys; Level level; Renderer renderer; Host host; Led led; Clock clock;
  AppController app(keys, level, renderer, router, perf, host, led, clock, log);

  app.begin();                         // boot without host
  CHECK(wire() == "");
  app.tick(); CHECK(renderer.calls == 1);

  // A USB flicker shorter than the debounce doesn't switch modes.
  host.up = true; clock.ms += 10; app.tick();
  host.up = false; clock.ms += 10; app.tick();
  host.up = true; clock.ms += 10; app.tick();
  clock.ms += config::kModeSwitchDebounceMs - 1; app.tick();
  CHECK(router.mode() == OutputMode::SynthOnly);
  const int rendersBefore = renderer.calls;

  clock.ms += 1; app.tick();           // connected for the full debounce -> MidiOnly
  CHECK(router.mode() == OutputMode::MidiOnly);
  CHECK(wire() == "");                 // nothing sent to the host on connect
  CHECK(renderer.calls == rendersBefore);  // no rendering in MidiOnly...
  CHECK(renderer.muted);
  renderer.feedback = true; app.tick();    // ...except while a feedback sound plays
  CHECK(renderer.calls == rendersBefore + 1);
  renderer.feedback = false;

  perf.handleControl(ControlType::InstrumentSelect, findPreset("Organ"));
  CHECK(wire() == "C0 10 00 | B0 18 00");                 // GM program 17 "Drawbar Organ"; its effect off (CC24 0)
  perf.handleControl(ControlType::InstrumentSelect, findPreset("Organ"));   // again: Leslie fast, not MIDI
  CHECK(wire() == "");
  perf.handleControl(ControlType::InstrumentSelect, findPreset("Violin"));
  CHECK(wire() == "C0 28 00 | B0 18 01");                 // GM program 41 "Violin"; effect on
  perf.handleControl(ControlType::InstrumentSelect, findPreset("Violin"));  // again: just reloads
  CHECK(wire() == "C0 28 00");
  perf.handleControl(ControlType::VolumeDown, 0);
  CHECK(wire() == "B0 07 43");                            // -3 dB step: 17398 -> CC7 67
  perf.handleControl(ControlType::OctaveUp, 0);
  CHECK(wire() == "B0 15 03");
  perf.handleControl(ControlType::TransposeDown, 0);
  CHECK(wire() == "B0 16 0B");
  perf.handleControl(ControlType::RhythmStartStop, 0);
  CHECK(wire() == "B0 6F 01");
  for (int i = 0; i < 30; ++i) perf.handleControl(ControlType::TempoUp, 0);  // clamps at 240
  MidiUSB.sent.erase(MidiUSB.sent.begin(), MidiUSB.sent.end() - 1);
  CHECK(wire() == "B0 14 7F");                            // HEAD sent 240 (invalid data byte)
  perf.handleControl(ControlType::SustainToggle, 0);
  CHECK(wire() == "B0 40 7F");
  perf.handleKey(0, true); perf.handleKey(0, false);
  CHECK(wire() == "90 3B 64");  // 48 +12 oct -1 transpose = 59; note-off withheld
  perf.handleControl(ControlType::SustainToggle, 0);
  CHECK(wire() == "B0 40 00 | 80 3B 00");                 // CC64 off, then the held note-off
  perf.handleControl(ControlType::Panic, 0);
  CHECK(wire() == "B0 78 00 | B0 7B 00");

  host.up = false; app.tick();                            // unplug: not yet (debounce)
  CHECK(router.mode() == OutputMode::MidiOnly);
  clock.ms += config::kModeSwitchDebounceMs; app.tick();  // -> SynthOnly, synth resynced
  CHECK(router.mode() == OutputMode::SynthOnly);
  CHECK(wire() == "B0 78 00 | B0 7B 00");                 // MIDI sink told all-notes-off (no-op when unplugged)
  CHECK(renderer.calls == rendersBefore + 2 && !renderer.muted);
  // Holding buttons, through the real matrix -> AppController path.
  auto runFor = [&](uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { clock.ms += 10; app.tick(); } };
  const int tempoBefore = perf.performance().tempoBpm();
  CHECK(tempoBefore == 240);               // raised to the maximum earlier in this test
  keys.push(0, 3, true);                   // R0 C3 = Tempo -
  runFor(1000);
  keys.push(0, 3, false);
  runFor(500);                             // released: no more repeats
  // Press (registered on the first 10 ms tick) + repeats at 0.5-0.9 s after it = 6 steps.
  CHECK(perf.performance().tempoBpm() == tempoBefore - 6 * 5);

  CHECK(gFeedback.enabled());
  keys.push(0, 5, true);                   // R0 C5 = Panic, held 2 s
  runFor(2100);
  keys.push(0, 5, false);
  runFor(50);
  CHECK(!gFeedback.enabled());             // feedback sounds switched off
  return finish("midi_test");
}
