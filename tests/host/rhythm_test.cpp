// RhythmEngine: pattern data is well formed, hits land on the grid (with swing),
// fills and crashes come on the right bars, and patterns recall their tempo.
#include "RhythmEngine.h"
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
  rhythm.setPlaying(true);
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

// Kick on beat 1 of Pop lands within the humanization window (<= 64 samples);
// Funk's off-beat 16ths are pushed late by the swing (56% => 12% of a step pair).
static void hitsLandOnTheGrid() {
  const auto pop = renderPattern(0, 100, 3.0);
  const long first = onsetAfter(pop, 0, 2000, 2000);
  CHECK(first >= 0 && first <= 64);

  const uint8_t bpm = 96;
  const double step = config::kSampleRate * 60.0 / (bpm * 4);
  const auto funk = renderPattern(4, bpm, 3.0);
  // Step 2 (an even 16th, straight) carries a kick; step 1 (odd, swung) only a ghost hat.
  const long straight = onsetAfter(funk, static_cast<size_t>(2 * step), static_cast<size_t>(step), 2000);
  CHECK(straight >= 0 && straight <= 64);
  const long expectedSwing = static_cast<long>(step * 2 * 0.06);
  const long swung = transientAfter(funk, static_cast<size_t>(step), static_cast<size_t>(step), 300);
  CHECK(swung >= expectedSwing && swung <= expectedSwing + 64);
}

// High-frequency energy in the first 0.2 s of a bar (crash = lots).
static double crashEnergy(const std::vector<int32_t> &x, size_t start) {
  double e = 0;
  for (size_t i = start + 1; i < start + 8820 && i < x.size(); ++i) {
    const double d = x[i] - x[i - 1];  // first difference ~ high-pass
    e += d * d;
  }
  return e;
}

// Rock: a crash marks bar 5 (after the bar-4 fill); bar 1 has none.
static void crashFollowsFill() {
  const uint8_t bpm = 112;
  const size_t bar = static_cast<size_t>(config::kSampleRate * 60.0 / bpm * 4);
  const auto rock = renderPattern(1, bpm, 5.0 * bar / config::kSampleRate + 0.5);
  CHECK(crashEnergy(rock, 4 * bar) > 4 * crashEnergy(rock, 0));
}

// Waltz is in 3/4: its bar is 12 sixteenths, so beat 1 recurs every 3 beats.
static void waltzIsInThree() {
  const uint8_t bpm = 90;
  const size_t beat = static_cast<size_t>(config::kSampleRate * 60.0 / bpm);
  const auto waltz = renderPattern(6, bpm, 3.0);
  CHECK(kickEnergy(waltz, 3 * beat) > 10 * kickEnergy(waltz, 4 * beat));  // kick on the next bar's 1, not on beat 2
}

// Choosing another pattern while playing takes effect at the next bar line,
// not mid-bar; re-selecting the current pattern doesn't restart it.
// Pop -> Rock: Rock kicks on step 10 (the "and" of 3), where Pop has only a hat.
static void patternChangesOnTheBarLine() {
  const uint8_t bpm = 100;
  const size_t step = static_cast<size_t>(config::kSampleRate * 60.0 / (bpm * 4));
  RhythmEngine rhythm;
  rhythm.begin(config::kSampleRate);
  rhythm.setBpm(bpm);
  rhythm.setPattern(0);  // Pop
  rhythm.setPlaying(true);
  std::vector<int32_t> out;
  for (size_t i = 0; i < 5 * step; ++i) out.push_back(rhythm.renderSample());
  rhythm.setPattern(0);  // same pattern again: no restart
  rhythm.setPattern(1);  // Rock, chosen during step 5
  for (size_t i = 5 * step; i < 28 * step; ++i) out.push_back(rhythm.renderSample());
  const double downbeatKick = kickEnergy(out, 0);
  CHECK(kickEnergy(out, 10 * step) * 10 < downbeatKick);  // still Pop for the rest of bar 1
  CHECK(kickEnergy(out, 26 * step) * 4 > downbeatKick);   // Rock from bar 2 (16 + 10)
}

int main() {
  patternsAreWellFormed();
  hitsLandOnTheGrid();
  crashFollowsFill();
  waltzIsInThree();
  patternChangesOnTheBarLine();
  return finish("rhythm_test");
}
