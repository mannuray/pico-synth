#include "StatusReporter.h"
#include "Presets.h"
#include "RhythmPatterns.h"

void printStatusLine(Print &out, const StatusSnapshot &status) {
  const PerformanceState &performance = status.performance;
  out.print("preset=");
  out.print(presets::at(performance.activePresetIndex()).name);
  out.print(" mode=");
  out.print(outputModeName(status.mode));
  out.print(" slot=");
  out.print(performance.instrumentSlot());
  out.print(" octave=");
  out.print(status.pitch.octaveShift());
  out.print(" transpose=");
  out.print(status.pitch.transpose());
  out.print(" sustain=");
  out.print(performance.sustainEnabled() ? "on" : "off");
  out.print(" effect=");
  out.print(effectName(performance.effect()));
  out.print(performance.effectEnabled() ? "(on)" : "(off)");
  out.print(" rhythm=");
  out.print(performance.rhythmPlaying() ? "on" : "off");
  out.print(performance.syncStartArmed() ? "(sync)" : "");
  out.print(" leslie=");
  out.print(performance.rotaryFast() ? "fast" : "slow");
  out.print(" pattern=");
  out.print(rhythmPatternAt(performance.rhythmPattern()).name);
  out.print(" bpm=");
  out.print(performance.tempoBpm());
  out.print(" volume=");
  out.print(performance.masterVolume());
  out.print(" mic=");
  out.print(status.micLevel);
  out.print(" cpu=");
  out.print(status.cpuLoadPercent);
  out.println("%");
}
