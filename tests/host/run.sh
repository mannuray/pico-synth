#!/bin/sh
# Host-side tests for arduino/synth_v1. Needs a C++17 compiler (g++ or clang++).
#   tests/host/run.sh            build and run everything
#   tests/host/run.sh --update   accept the current audio output as the new golden
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../arduino/synth_v1"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
CXX=${CXX:-c++}
FLAGS="-std=gnu++20 -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=undefined -fno-sanitize-recover=undefined -I$HERE/stub -I$SRC"

ENGINE="$SRC/SynthEngine.cpp $SRC/Samples.cpp $SRC/Voice.cpp $SRC/VoiceShape.cpp $SRC/Oscillator.cpp $SRC/Feedback.cpp \
        $SRC/RotarySpeaker.cpp $SRC/Effects.cpp $SRC/Presets.cpp $SRC/RhythmEngine.cpp"
CONTROL="$SRC/PerformanceController.cpp $SRC/PerformanceState.cpp $SRC/PitchSettings.cpp \
         $SRC/KeyTracker.cpp $SRC/OutputRouter.cpp $SRC/SynthEventSink.cpp"
APP="$SRC/AppController.cpp $SRC/StatusReporter.cpp $SRC/MidiEventSink.cpp"

$CXX $FLAGS "$HERE/golden.cpp" $ENGINE -o "$OUT/golden"
$CXX $FLAGS "$HERE/engine_test.cpp" $ENGINE -o "$OUT/engine_test"
$CXX $FLAGS "$HERE/rhythm_test.cpp" $SRC/RhythmEngine.cpp $SRC/Feedback.cpp $SRC/Oscillator.cpp -o "$OUT/rhythm_test"
$CXX $FLAGS "$HERE/control_test.cpp" $CONTROL $ENGINE -o "$OUT/control_test"
$CXX $FLAGS "$HERE/midi_test.cpp" $APP $CONTROL $ENGINE -o "$OUT/midi_test"

"$OUT/golden" > "$OUT/golden.txt"
if [ "$1" = "--update" ]; then
  cp "$OUT/golden.txt" "$HERE/golden_expected.txt"
  echo "golden: updated golden_expected.txt"
elif diff "$HERE/golden_expected.txt" "$OUT/golden.txt" > "$OUT/golden.diff"; then
  echo "golden: all $(wc -l < "$OUT/golden.txt" | tr -d ' ') cases match"
else
  echo "golden: audio output changed in these cases (preset, effect, rhythm):"
  grep '^>' "$OUT/golden.diff" | cut -c3-13
  echo "If the change is intended, run: tests/host/run.sh --update"
  exit 1
fi

status=0
"$OUT/engine_test" || status=1
"$OUT/rhythm_test" || status=1
"$OUT/control_test" || status=1
"$OUT/midi_test" || status=1
exit $status
