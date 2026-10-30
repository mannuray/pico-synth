#pragma once
// Minimal Arduino API so the platform-independent firmware code builds on a desktop.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>

using std::min;
using std::max;
typedef uint8_t pin_size_t;

template <typename T, typename L, typename H>
inline T constrain(T x, L lo, H hi) { return x < lo ? static_cast<T>(lo) : (x > hi ? static_cast<T>(hi) : x); }
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
inline uint32_t micros() { return 0; }
inline uint32_t millis() { return 0; }

// Collects printed text in `text`.
class Print {
public:
  std::string text;
  void print(const char *s) { text += s; }
  template <typename T> void print(T v) { text += std::to_string(static_cast<long>(v)); }
  template <typename T> void println(T v) { print(v); text += "\n"; }
  void println() { text += "\n"; }
};
