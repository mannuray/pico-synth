// PerformanceController + OutputRouter: command order, preset defaults, routing, sustain end to end.
#include "PerformanceController.h"
#include "OutputRouter.h"
#include "SynthEventSink.h"
#include "SynthEngine.h"
#include "Presets.h"
#include "check.h"
#include <vector>
#include <string>

struct Rec : IEventSink {
  std::vector<std::string> log;
  void handleEvent(const KeyboardEvent &e) override { log.push_back(std::string(e.type == KeyboardEventType::NoteOn ? "on" : "off") + std::to_string(e.note)); }
  void handleCommand(const PerformanceCommand &c) override { log.push_back("c" + std::to_string(int(c.type)) + "=" + std::to_string(c.value)); }
  int count(const std::string &s) const { int n = 0; for (auto &x : log) n += x == s; return n; }
  int indexOf(const std::string &s) const { for (size_t i = 0; i < log.size(); ++i) if (log[i] == s) return i; return -1; }
};
std::string cmd(CommandType t, int v) { return "c" + std::to_string(int(t)) + "=" + std::to_string(v); }

struct Silent : AudioOutput {
  int32_t peak = 0;
  bool begin(uint32_t, uint8_t) override { return true; }
  void writeStereo(int16_t l, int16_t r) override { peak = max<int32_t>(peak, max(abs(l), abs(r))); }
};

int main() {
  Print log;
  // --- Sustain ordering: Sustain(0) must precede the held-over note-offs.
  {
    Rec out; PerformanceController pc(out, log); pc.begin(); out.log.clear();
    pc.handleControl(ControlType::SustainToggle, 0);
    pc.handleKey(0, true); pc.handleKey(0, false);
    CHECK(out.count("off48") == 0);                    // held by sustain
    pc.handleControl(ControlType::SustainToggle, 0);
    const int s = out.indexOf(cmd(CommandType::Sustain, 0)), n = out.indexOf("off48");
    CHECK(s >= 0 && n > s);
  }
  // --- Preset change sets state defaults and sends them; ADSR offsets reset.
  {
    Rec out; PerformanceController pc(out, log); pc.begin();
    pc.handleControl(ControlType::AttackUp, 0);
    CHECK(pc.performance().envelopeOffset(EnvelopeParam::Attack) == 1);
    for (int i = 0; i < 2; ++i) pc.handleControl(ControlType::AttackUp, 0);
    CHECK(pc.performance().envelopeOffset(EnvelopeParam::Attack) == -2);  // wrapped 2 -> -2
    out.log.clear();
    pc.handleControl(ControlType::InstrumentSelect, 4);  // GrandPno, default Reverb
    CHECK(pc.performance().effect() == EffectType::Reverb && pc.performance().effectEnabled());
    CHECK(pc.performance().envelopeOffset(EnvelopeParam::Attack) == 0);
    CHECK(out.count(cmd(CommandType::SelectPreset, 4)) == 1);
    CHECK(out.count(cmd(CommandType::Effect, int(EffectType::Reverb))) == 1);
    CHECK(out.count(cmd(CommandType::AttackOffset, 0)) == 1);
    // EffectSelect cycles from the preset's effect, not a stale one.
    pc.handleControl(ControlType::EffectSelect, 0);
    CHECK(pc.performance().effect() == EffectType::Delay);
  }
  // --- Volume reaches the synth at full resolution.
  {
    Rec out; PerformanceController pc(out, log); pc.begin(); out.log.clear();
    pc.handleControl(ControlType::VolumeDown, 0);
    CHECK(out.count(cmd(CommandType::Volume, config::kDefaultMasterVolume - config::kVolumeStep)) == 1);
  }
  // --- Router: deactivated sinks get AllNotesOff; sync goes to the new sink.
  {
    Rec synthRec, midiRec;
    OutputRouter router(synthRec, midiRec, OutputMode::SynthOnly);
    PerformanceController pc(router, log); pc.begin();
    pc.handleControl(ControlType::VolumeUp, 0);
    CHECK(midiRec.log.empty());
    CHECK(router.setMode(OutputMode::MidiOnly));
    CHECK(!router.setMode(OutputMode::MidiOnly));
    CHECK(synthRec.log.back() == cmd(CommandType::AllNotesOff, 0));
    pc.syncOutputs();
    CHECK(midiRec.count(cmd(CommandType::Volume, pc.performance().masterVolume())) == 1);
    CHECK(midiRec.count(cmd(CommandType::SelectPreset, 0)) == 1);
    CHECK(!router.synthActive());
  }
  // --- End to end: controller -> router -> synth sink -> engine; sustain releases to silence.
  {
    static Silent audio; static SynthEngine synth(audio); synth.begin();
    SynthEventSink sink(synth); Rec m;
    OutputRouter router(sink, m, OutputMode::SynthOnly);
    PerformanceController pc(router, log); pc.begin();
    pc.handleControl(ControlType::InstrumentSelect, 4);  // GrandPno
    pc.handleControl(ControlType::SustainToggle, 0);
    pc.handleKey(12, true); for (int b = 0; b < 20; ++b) synth.renderFrames(64);
    pc.handleKey(12, false); for (int b = 0; b < 20; ++b) synth.renderFrames(64);
    audio.peak = 0; for (int b = 0; b < 5; ++b) synth.renderFrames(64);
    CHECK(audio.peak > 200);                             // still sounding under sustain
    pc.handleControl(ControlType::SustainToggle, 0);
    pc.handleControl(ControlType::EffectToggle, 0);      // reverb off so the tail is just the voice
    for (int b = 0; b < 3000; ++b) synth.renderFrames(64);
    audio.peak = 0; for (int b = 0; b < 5; ++b) synth.renderFrames(64);
    CHECK(audio.peak <= 2);                              // released (dither only)
  }
  return finish("control_test");
}
