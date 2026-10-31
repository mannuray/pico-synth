#include "AppController.h"
#include "ArduinoPlatform.h"
#include "Core1Worker.h"
#include "Feedback.h"
#include "I2SAudioOutput.h"
#include <MIDIUSB.h>
#include "MatrixScanner.h"
#include "MicInput.h"
#include "MidiEventSink.h"
#include "SynthEngine.h"
#include "SynthEventSink.h"

namespace {
ArduinoClock systemClock;
UsbMidiHostLink hostLink;
BuiltinStatusLed statusLed;
MatrixScanner matrixScanner;
MicInput micInput;
I2SAudioOutput audioOutput;
FeedbackSounds feedback;
Core1Worker core1Worker;
SynthEngine synth(audioOutput, feedback, core1Worker);  // half the voices render on core 1
SynthEventSink synthSink(synth);
MidiEventSink midiSink;
OutputRouter router(synthSink, midiSink, OutputMode::SynthOnly);
PerformanceController performance(router, feedback, Serial);
AppController app(matrixScanner, micInput, synth, router, performance, hostLink, statusLed, systemClock, Serial);
}  // namespace

void setup() {
  MidiUSB.setName("Synth Pico MIDI");
  MidiUSB.begin();

  Serial.begin(config::kSerialBaud);
  delay(1200);

  if (!synth.begin()) {
    Serial.println("synth begin failed");
    while (true) {
      delay(100);
    }
  }

  matrixScanner.begin();
  micInput.begin();
  statusLed.begin(systemClock.nowMs());
  app.begin();

  Serial.println();
  Serial.println("synth_v1");
  Serial.println("I2S: GP18 BCLK, GP19 LRCLK, GP20 DIN");
  Serial.println("ADC: GP27");
  app.printStatus();
}

void loop() {
  app.tick();
}

// Core 1 renders half of the voices for each audio block, on request.
void setup1() {}

void loop1() {
  core1Worker.serviceFromCore1();
}
