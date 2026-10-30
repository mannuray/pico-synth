// Full control path through AppController (with fake hardware) to the exact USB MIDI bytes.
#include "AppController.h"
#include "MidiEventSink.h"
#include "SynthEngine.h"
#include "SynthEventSink.h"
#include <MIDIUSB.h>
#include "check.h"
#include <string>
MidiUsbStub MidiUSB;

std::string wire() {  // "B0 07 4F | C0 01 00 | ..." then clear
  std::string s; char buf[16];
  for (auto &p : MidiUSB.sent) { snprintf(buf, sizeof buf, "%s%02X %02X %02X", s.empty() ? "" : " | ", p.byte1, p.byte2, p.byte3); s += buf; }
  MidiUSB.sent.clear(); return s;
}
struct NoKeys : IKeyMatrix { bool poll(KeyMatrixEvent &) override { return false; } };
struct Level : ILevelInput { uint16_t readLevel() override { return 0; } };
struct Renderer : IAudioRenderer { int calls = 0; void renderFrames(size_t) override { ++calls; } uint32_t lastProcessingUs() const override { return 0; } };
struct Host : IHostLink { bool up = false; bool isConnected() override { return up; } };
struct Led : IStatusIndicator { void flash(uint32_t) override {} void update(uint32_t) override {} };
struct Clock : IClock { uint32_t ms = 0; uint32_t nowMs() const override { return ms; } uint32_t nowUs() const override { return ms * 1000; } };
struct Silent : AudioOutput { bool begin(uint32_t, uint8_t) override { return true; } void writeStereo(int16_t, int16_t) override {} };

int main() {
  Print log; Silent audio; SynthEngine synth(audio); synth.begin();
  SynthEventSink synthSink(synth); MidiEventSink midiSink;
  OutputRouter router(synthSink, midiSink, OutputMode::SynthOnly);
  PerformanceController perf(router, log);
  NoKeys keys; Level level; Renderer renderer; Host host; Led led; Clock clock;
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
  CHECK(renderer.calls == rendersBefore);  // no rendering in MidiOnly

  perf.handleControl(ControlType::InstrumentSelect, 1);   // GrandPno (Reverb) -> EPiano (Chorus)
  CHECK(wire() == "C0 04 00 | B0 17 02");                 // GM program 5 "Electric Piano 1", CC23=Chorus
  perf.handleControl(ControlType::InstrumentSelect, 1);   // same preset again
  CHECK(wire() == "C0 04 00");
  perf.handleControl(ControlType::BankUp, 0);             // -> bank 1 slot 1 Cello (Reverb)
  CHECK(wire() == "C0 2A 00 | B0 17 03");                 // no bank select; GM program 43 "Cello"
  perf.handleControl(ControlType::VolumeDown, 0);
  CHECK(wire() == "B0 07 4F");                            // map(20480) = 79, as HEAD
  perf.handleControl(ControlType::OctaveUp, 0);
  CHECK(wire() == "B0 15 03");
  perf.handleControl(ControlType::TransposeDown, 0);
  CHECK(wire() == "B0 16 0B");
  perf.handleControl(ControlType::AttackUp, 0);
  CHECK(wire() == "B0 49 03");
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
  CHECK(renderer.calls == rendersBefore + 1);
  return finish("midi_test");
}
