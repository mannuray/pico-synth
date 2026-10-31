// PerformanceController + OutputRouter: command order, preset defaults, routing, sustain end to end.
#include "ControlHold.h"
#include "PerformanceController.h"
#include "OutputRouter.h"
#include "SynthEventSink.h"
#include "SynthEngine.h"
#include "Presets.h"
#include "catalog.h"
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

// Records feedback cues as names ("Up", "Limit", ...).
struct RecFeedback : IFeedback {
  std::vector<std::string> cues;
  bool on = true;
  void cue(FeedbackCue c) override {
    static const char *names[] = {"Up", "Down", "Center", "Limit", "Blip"};
    cues.push_back(names[int(c)]);
  }
  void setEnabled(bool e) override { on = e; cues.push_back(e ? "Enabled" : "Disabled"); }
  bool enabled() const override { return on; }
  std::string last() const { return cues.empty() ? "" : cues.back(); }
};
static RecFeedback gCues;

struct Silent : AudioOutput {
  int32_t peak = 0;
  bool begin(uint32_t, uint8_t) override { return true; }
  void writeStereo(int16_t l, int16_t r) override { peak = max<int32_t>(peak, max(abs(l), abs(r))); }
};

int main() {
  Print log;
  // --- Sustain ordering: Sustain(0) must precede the held-over note-offs.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin(); out.log.clear();
    pc.handleControl(ControlType::SustainToggle, 0);
    pc.handleKey(0, true); pc.handleKey(0, false);
    CHECK(out.count("off48") == 0);                    // held by sustain
    pc.handleControl(ControlType::SustainToggle, 0);
    const int s = out.indexOf(cmd(CommandType::Sustain, 0)), n = out.indexOf("off48");
    CHECK(s >= 0 && n > s);
  }
  // --- Preset change sets state defaults and sends only what changed.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin();
    out.log.clear();
    const uint8_t organ = findPreset("Organ"), violin = findPreset("Violin");
    pc.handleControl(ControlType::InstrumentSelect, organ);   // Piano (Reverb on) -> Organ (effect off)
    CHECK(!pc.performance().effectEnabled());
    CHECK(out.count(cmd(CommandType::SelectPreset, organ)) == 1);
    CHECK(out.count(cmd(CommandType::EffectEnabled, 0)) == 1);
    CHECK(out.count(cmd(CommandType::Effect, int(EffectType::Reverb))) == 0);  // unchanged: not re-sent
    pc.handleControl(ControlType::InstrumentSelect, violin);  // Violin: Reverb on again
    CHECK(pc.performance().effectEnabled() && out.count(cmd(CommandType::EffectEnabled, 1)) == 1);
    // EffectSelect cycles from the preset's effect, not a stale one.
    pc.handleControl(ControlType::EffectSelect, 0);
    CHECK(pc.performance().effect() == EffectType::Delay);
  }
  // --- Volume reaches the synth at full resolution.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin(); out.log.clear();
    pc.handleControl(ControlType::VolumeDown, 0);
    CHECK(out.count(cmd(CommandType::Volume, (config::kDefaultMasterVolume * 23198) >> 15)) == 1);  // -3 dB
  }
  // --- Router: deactivated sinks get AllNotesOff; sync goes to the new sink.
  {
    Rec synthRec, midiRec;
    OutputRouter router(synthRec, midiRec, OutputMode::SynthOnly);
    PerformanceController pc(router, gCues, log); pc.begin();
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
    static Silent audio; static SynthEngine synth(audio, gFeedback); synth.begin();
    SynthEventSink sink(synth); Rec m;
    OutputRouter router(sink, m, OutputMode::SynthOnly);
    PerformanceController pc(router, gCues, log); pc.begin();
    pc.handleControl(ControlType::InstrumentSelect, findPreset("Violin"));  // sustained, Reverb on
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
  // --- Re-pressing a key that still rings under sustain ends its old note first.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin(); out.log.clear();
    pc.handleControl(ControlType::SustainToggle, 0);
    pc.handleKey(0, true); pc.handleKey(0, false);      // note 48, held by sustain
    pc.handleControl(ControlType::OctaveUp, 0);
    pc.handleKey(0, true); pc.handleKey(0, false);      // now note 60
    pc.handleControl(ControlType::SustainToggle, 0);
    CHECK(out.count("on48") == 1 && out.count("off48") == 1);
    CHECK(out.indexOf("off48") < out.indexOf("on60"));  // old note ended before the new one
    CHECK(out.count("on60") == 1 && out.count("off60") == 1);
  }
  // --- Choosing a rhythm recalls its usual tempo (Slow Rock = 66 BPM).
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin(); out.log.clear();
    pc.handleControl(ControlType::RhythmSelect, findPattern("SlowRock"));
    CHECK(pc.performance().tempoBpm() == 66);
    CHECK(out.count(cmd(CommandType::Tempo, 66)) == 1);
  }
  // --- Volume steps in 3 dB, reaches silence, comes back, and stops at full.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin();
    int presses = 0;
    while (pc.performance().masterVolume() > 0 && presses < 30) { pc.handleControl(ControlType::VolumeDown, 0); ++presses; }
    CHECK(pc.performance().masterVolume() == 0 && presses >= 9 && presses <= 12);  // ~-30 dB range
    pc.handleControl(ControlType::VolumeDown, 0);
    CHECK(gCues.last() == "Limit");
    pc.handleControl(ControlType::VolumeUp, 0);
    CHECK(pc.performance().masterVolume() > 0 && gCues.last() == "Blip");
    for (int i = 0; i < 20; ++i) pc.handleControl(ControlType::VolumeUp, 0);
    CHECK(pc.performance().masterVolume() == config::kMaxMasterVolume && gCues.last() == "Limit");
  }
  // --- Feedback cues for stepped values.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin();
    pc.handleControl(ControlType::OctaveUp, 0);   CHECK(gCues.last() == "Up");
    pc.handleControl(ControlType::OctaveDown, 0); CHECK(gCues.last() == "Center");
    pc.handleControl(ControlType::OctaveDown, 0); CHECK(gCues.last() == "Down");
    pc.handleControl(ControlType::OctaveDown, 0); CHECK(gCues.last() == "Down");
    pc.handleControl(ControlType::OctaveDown, 0); CHECK(gCues.last() == "Limit");  // -2 is the floor
    pc.handleControl(ControlType::SustainToggle, 0); CHECK(gCues.last() == "Up");
    pc.handleControl(ControlType::SustainToggle, 0); CHECK(gCues.last() == "Down");
    pc.toggleFeedback(); CHECK(gCues.last() == "Disabled");
    pc.toggleFeedback(); CHECK(gCues.last() == "Enabled");
  }
  // --- Effect Select also switches the effect on (Organ starts with it off).
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin();
    pc.handleControl(ControlType::InstrumentSelect, findPreset("Organ"));
    CHECK(!pc.performance().effectEnabled());
    out.log.clear();
    pc.handleControl(ControlType::EffectSelect, 0);
    CHECK(pc.performance().effectEnabled() && out.count(cmd(CommandType::EffectEnabled, 1)) == 1);
  }
  // --- Panic also releases sustain, and leaves the rhythm running.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin();
    pc.handleControl(ControlType::RhythmStartStop, 0);
    pc.handleControl(ControlType::SustainToggle, 0);
    out.log.clear();
    pc.handleControl(ControlType::Panic, 0);
    CHECK(out.indexOf(cmd(CommandType::AllNotesOff, 0)) < out.indexOf(cmd(CommandType::Sustain, 0)));
    CHECK(!pc.performance().sustainEnabled() && pc.performance().rhythmPlaying());
  }
  // --- Rhythm select keeps your tempo while playing; recalls the pattern's when stopped.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin();
    pc.handleControl(ControlType::RhythmStartStop, 0);
    const int tempo = pc.performance().tempoBpm() + 5;
    pc.handleControl(ControlType::TempoUp, 0);
    pc.handleControl(ControlType::RhythmSelect, findPattern("SlowRock"));  // while playing
    CHECK(pc.performance().tempoBpm() == tempo);
    pc.handleControl(ControlType::RhythmStartStop, 0);    // stop
    pc.handleControl(ControlType::RhythmSelect, findPattern("Disco"));     // while stopped
    CHECK(pc.performance().tempoBpm() == 120);
  }
  // --- Fill, Intro/Ending, Sync Start.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin(); out.log.clear();
    pc.handleControl(ControlType::RhythmFill, 0);        // stopped: arms sync start
    CHECK(pc.performance().syncStartArmed() && gCues.last() == "Up" && out.log.empty());
    pc.handleControl(ControlType::RhythmFill, 0);        // and disarms it
    CHECK(!pc.performance().syncStartArmed() && gCues.last() == "Down");
    pc.handleControl(ControlType::IntroEnding, 0);
    CHECK(pc.performance().rhythmPlaying() && out.count(cmd(CommandType::RhythmPlaying, kRhythmStartWithIntro)) == 1);
    pc.handleControl(ControlType::RhythmFill, 0);
    CHECK(out.count(cmd(CommandType::RhythmFill, 0)) == 1);
    pc.handleControl(ControlType::IntroEnding, 0);
    CHECK(!pc.performance().rhythmPlaying() && out.count(cmd(CommandType::RhythmPlaying, kRhythmStopWithEnding)) == 1);

    out.log.clear();
    pc.handleControl(ControlType::RhythmFill, 0);        // stopped: arm sync start
    CHECK(pc.performance().syncStartArmed() && gCues.last() == "Up" && out.log.empty());
    pc.handleKey(5, true);                                // first key starts the rhythm, before the note
    CHECK(pc.performance().rhythmPlaying() && !pc.performance().syncStartArmed());
    CHECK(out.indexOf(cmd(CommandType::RhythmPlaying, kRhythmStart)) == 0 && out.indexOf("on53") == 1);
    pc.handleKey(5, false);
    pc.handleKey(7, true);                                // later keys don't restart it
    CHECK(out.count(cmd(CommandType::RhythmPlaying, kRhythmStart)) == 1);
  }
  // --- Leslie speed: pressing the organ's button again; other presets just reload.
  {
    Rec out; PerformanceController pc(out, gCues, log); pc.begin(); out.log.clear();
    const uint8_t piano = findPreset("Piano"), organ = findPreset("Organ");
    pc.handleControl(ControlType::InstrumentSelect, piano);   // Piano again: no rotary
    CHECK(!pc.performance().rotaryFast() && out.count(cmd(CommandType::RotaryFast, 1)) == 0);
    pc.handleControl(ControlType::InstrumentSelect, organ);   // choose the organ
    CHECK(!pc.performance().rotaryFast());
    pc.handleControl(ControlType::InstrumentSelect, organ);   // organ again: fast
    CHECK(pc.performance().rotaryFast() && out.count(cmd(CommandType::RotaryFast, 1)) == 1 && gCues.last() == "Up");
    pc.handleControl(ControlType::InstrumentSelect, organ);   // and slow
    CHECK(!pc.performance().rotaryFast() && gCues.last() == "Down");
  }
  // --- Hold: Tempo/Volume/Transpose repeat after 0.5 s every 0.1 s; Panic long press fires once at 2 s.
  {
    ControlHold hold;
    int repeats = 0, longPresses = 0;
    auto update = [&](uint32_t ms) {
      hold.update(ms, [&](ControlType, uint8_t) { ++repeats; return true; }, [&](ControlType) { ++longPresses; });
    };
    hold.press(ControlType::TempoUp, 0, 1000);
    update(1499); CHECK(repeats == 0);
    update(1500); CHECK(repeats == 1);
    update(1800); CHECK(repeats == 4);
    hold.release(ControlType::TempoUp);
    update(3000); CHECK(repeats == 4);
    hold.press(ControlType::OctaveUp, 0, 4000);   // no repeat for octave
    update(6000); CHECK(repeats == 4 && longPresses == 0);
    hold.press(ControlType::Panic, 0, 7000);
    update(8999); CHECK(longPresses == 0);
    update(9000); update(9500); CHECK(longPresses == 1 && repeats == 4);
    // Repeating stops once the callback reports a limit.
    int limited = 0;
    hold.press(ControlType::VolumeUp, 0, 10000);
    hold.update(12000, [&](ControlType, uint8_t) { return ++limited < 3; }, [](ControlType) {});
    CHECK(limited == 3);
  }
  return finish("control_test");
}
