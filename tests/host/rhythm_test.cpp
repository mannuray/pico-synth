// RhythmEngine: pattern data is well formed, hits land on the grid (with swing),
// fills and crashes come on the right bars, and patterns recall their tempo.
#include "RhythmEngine.h"
#include "catalog.h"
#include "check.h"
#include <math.h>
#include <string.h>
#include <vector>

static bool validLane(const char *lane, size_t length, const char *allowed) {
  if (lane == nullptr) return true;
  if (strlen(lane) != length) return false;
  for (const char *c = lane; *c; ++c) {
    if (strchr(allowed, *c) == nullptr) return false;
  }
  return true;
}

// Every lane has the pattern's length (a whole number of bars), fill lanes are
// one bar long, and only known step characters are used.
static void patternsAreWellFormed() {
  for (uint8_t i = 0; i < kRhythmPatternCount; ++i) {
    const RhythmPattern &p = rhythmPatternAt(i);
    const char *first = p.kick ? p.kick : p.hat;
    CHECK(first != nullptr);
    const size_t length = strlen(first);
    CHECK(p.stepsPerBar == 12 || p.stepsPerBar == 16);
    CHECK(p.stepsPerBeat == 3 || p.stepsPerBeat == 4);
    CHECK(p.stepsPerBeat == 4 || p.swingPercent == 50);  // triplet grids are already swung
    CHECK(length % p.stepsPerBar == 0);
    CHECK(p.swingPercent >= 50 && p.swingPercent <= 75);
    CHECK(p.defaultBpm >= config::kTempoMinBpm && p.defaultBpm <= config::kTempoMaxBpm);
    for (const char *lane : {p.kick, p.snare, p.hat, p.openHat, p.perc}) CHECK(validLane(lane, length, "Xxg."));
    CHECK(validLane(p.tom, length, "HhMmLl."));
    for (const char *lane : {p.fillKick, p.fillSnare, p.fillHat}) CHECK(validLane(lane, p.stepsPerBar, "Xxg."));
    CHECK(validLane(p.fillTom, p.stepsPerBar, "HhMmLl."));
  }
}

static std::vector<int32_t> renderPattern(uint8_t pattern, uint8_t bpm, double seconds) {
  RhythmEngine rhythm;
  rhythm.begin(config::kSampleRate);
  rhythm.setBpm(bpm);
  rhythm.setPattern(pattern);
  rhythm.start();
  std::vector<int32_t> out(static_cast<size_t>(seconds * config::kSampleRate));
  for (int32_t &s : out) s = rhythm.renderSample();
  return out;
}

// Samples from `at` until the high-frequency content (first difference) first
// exceeds `threshold`: finds a hat or snare onset even over a ringing kick.
static long transientAfter(const std::vector<int32_t> &x, size_t at, size_t window, int32_t threshold) {
  for (size_t i = at + 1; i < at + window && i < x.size(); ++i) {
    if (abs(x[i] - x[i - 1]) > threshold) return static_cast<long>(i - at);
  }
  return -1;
}

// Energy near 50 Hz (the kick's fundamental) over 50 ms from `at` (Goertzel).
static double kickEnergy(const std::vector<int32_t> &x, size_t at) {
  const double w = 2 * M_PI * 50.0 / config::kSampleRate;
  double s1 = 0, s2 = 0;
  for (size_t i = at; i < at + 2205 && i < x.size(); ++i) {
    const double s0 = x[i] + 2 * cos(w) * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return s1 * s1 + s2 * s2 - 2 * cos(w) * s1 * s2;
}

// Samples from `at` until the signal first exceeds `threshold`.
static long onsetAfter(const std::vector<int32_t> &x, size_t at, size_t window, int32_t threshold) {
  for (size_t i = at; i < at + window && i < x.size(); ++i) {
    if (abs(x[i]) > threshold) return static_cast<long>(i - at);
  }
  return -1;
}

// Rock's beat-1 kick lands within the humanization window (<= 64 samples);
// Samba's off-beat 16ths are pushed late by its swing (54% => 8% of a step pair).
static void hitsLandOnTheGrid() {
  const auto rock = renderPattern(findPattern("Rock"), 112, 3.0);
  const long first = onsetAfter(rock, 0, 2000, 2000);
  CHECK(first >= 0 && first <= 64);

  const uint8_t bpm = 100;
  const double step = config::kSampleRate * 60.0 / (bpm * 4);
  const auto samba = renderPattern(findPattern("Samba"), bpm, 3.0);
  // Step 2 (even, straight) and step 1 (odd, swung) both carry a hat.
  const long straight = transientAfter(samba, static_cast<size_t>(2 * step), static_cast<size_t>(step), 300);
  CHECK(straight >= 0 && straight <= 64);
  const long expectedSwing = static_cast<long>(step * 2 * 0.04);
  const long swung = transientAfter(samba, static_cast<size_t>(step), static_cast<size_t>(step), 300);
  CHECK(swung >= expectedSwing && swung <= expectedSwing + 64);
}

// Blues runs on a triplet grid: 3 steps per beat, kick on beats 1 and 3.
static void tripletGridKeepsTheBeat() {
  const uint8_t bpm = 96;
  const size_t beat = static_cast<size_t>(config::kSampleRate * 60.0 / bpm);
  const auto blues = renderPattern(findPattern("Blues"), bpm, 3.0);
  // Kick on beat 3; beat 2 has only the snare (whose noise carries a little low end).
  CHECK(kickEnergy(blues, 2 * beat) > 4 * kickEnergy(blues, beat));
}

// A crash rings through the gaps between the other hits: high-frequency
// energy (first difference) of the quietest 512-sample window in the first
// 0.6 s of a bar. Near zero without a crash, since the kit's other hits are short.
static double crashEnergy(const std::vector<int32_t> &x, size_t start) {
  double quietest = 1e300;
  for (size_t a = start; a + 512 <= start + 26460 && a + 512 <= x.size(); a += 256) {
    double e = 0;
    for (size_t i = a + 1; i < a + 512; ++i) {
      const double d = x[i] - x[i - 1];
      e += d * d;
    }
    quietest = fmin(quietest, e);
  }
  return quietest;
}

// Rock: a crash marks bar 5 (after the bar-4 fill); bar 1 has none.
static void crashFollowsFill() {
  const uint8_t bpm = 112;
  const size_t bar = static_cast<size_t>(config::kSampleRate * 60.0 / bpm * 4);
  const auto rock = renderPattern(findPattern("Rock"), bpm, 5.0 * bar / config::kSampleRate + 0.5);
  CHECK(crashEnergy(rock, 4 * bar) > 1e7 + 100 * crashEnergy(rock, 0));
}

// Waltz is in 3/4: its bar is 12 sixteenths, so beat 1 recurs every 3 beats.
static void waltzIsInThree() {
  const uint8_t bpm = 90;
  const size_t beat = static_cast<size_t>(config::kSampleRate * 60.0 / bpm);
  const auto waltz = renderPattern(findPattern("Waltz"), bpm, 3.0);
  CHECK(kickEnergy(waltz, 3 * beat) > 10 * kickEnergy(waltz, 4 * beat));  // kick on the next bar's 1, not on beat 2
}

// Choosing another pattern while playing takes effect at the next bar line,
// not mid-bar; re-selecting the current pattern doesn't restart it.
// Twist -> Rock: Rock kicks on step 10 (the "and" of 3), where Twist has only a hat.
static void patternChangesOnTheBarLine() {
  const uint8_t bpm = 100;
  const size_t step = static_cast<size_t>(config::kSampleRate * 60.0 / (bpm * 4));
  RhythmEngine rhythm;
  rhythm.begin(config::kSampleRate);
  rhythm.setBpm(bpm);
  rhythm.setPattern(findPattern("Twist"));
  rhythm.start();
  std::vector<int32_t> out;
  for (size_t i = 0; i < 5 * step; ++i) out.push_back(rhythm.renderSample());
  rhythm.setPattern(findPattern("Twist"));  // same pattern again: no restart
  rhythm.setPattern(findPattern("Rock"));   // chosen during step 5
  for (size_t i = 5 * step; i < 28 * step; ++i) out.push_back(rhythm.renderSample());
  const double downbeatKick = kickEnergy(out, 0);
  CHECK(kickEnergy(out, 10 * step) * 10 < downbeatKick);  // still Twist for the rest of bar 1
  CHECK(kickEnergy(out, 26 * step) * 4 > downbeatKick);   // Rock from bar 2 (16 + 10)
}

// Intro: the fill bar comes first and its crash lands on bar 2's downbeat.
// Ending: finish the bar, play the fill bar, crash, then stop.
// Fill on demand: next bar is a fill, crash on the bar after.
static void introEndingAndFill() {
  const uint8_t bpm = 112;
  const size_t bar = static_cast<size_t>(config::kSampleRate * 60.0 / bpm * 4);
  auto run = [&](auto &&control, size_t bars) {
    RhythmEngine rhythm;
    rhythm.begin(config::kSampleRate);
    rhythm.setBpm(bpm);
    rhythm.setPattern(findPattern("Rock"));
    std::vector<int32_t> out;
    control(rhythm, out, bars);
    return out;
  };

  const auto intro = run([&](RhythmEngine &r, std::vector<int32_t> &out, size_t bars) {
    r.start(true);
    for (size_t i = 0; i < bars * bar; ++i) out.push_back(r.renderSample());
  }, 3);
  CHECK(crashEnergy(intro, bar) > 1e7 + 100 * crashEnergy(intro, 0));  // crash on bar 2, not bar 1

  bool stoppedAfter = false;
  const auto ending = run([&](RhythmEngine &r, std::vector<int32_t> &out, size_t) {
    r.start(false);
    for (size_t i = 0; i < bar / 2; ++i) out.push_back(r.renderSample());
    r.stop(true);                                       // mid-bar 1
    for (size_t i = bar / 2; i < 4 * bar; ++i) out.push_back(r.renderSample());
    stoppedAfter = !r.playing();
  }, 4);
  CHECK(stoppedAfter);
  CHECK(crashEnergy(ending, 2 * bar) > 1e7 + 100 * crashEnergy(ending, bar));  // bar 2 = fill, crash at bar 3
  CHECK(kickEnergy(ending, 3 * bar) * 20 < kickEnergy(ending, 0));     // nothing after the ending

  const auto fill = run([&](RhythmEngine &r, std::vector<int32_t> &out, size_t) {
    r.start(false);
    for (size_t i = 0; i < bar / 2; ++i) out.push_back(r.renderSample());
    r.requestFill();                                    // bar 2 becomes the fill
    for (size_t i = bar / 2; i < 3 * bar + bar / 4; ++i) out.push_back(r.renderSample());
  }, 4);
  CHECK(crashEnergy(fill, 2 * bar) > 1e7 + 100 * crashEnergy(fill, bar));      // crash at bar 3
}

int main() {
  patternsAreWellFormed();
  hitsLandOnTheGrid();
  crashFollowsFill();
  waltzIsInThree();
  patternChangesOnTheBarLine();
  tripletGridKeepsTheBeat();
  introEndingAndFill();
  return finish("rhythm_test");
}
