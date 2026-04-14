#include "AppController.h"
#include "I2SAudioOutput.h"
#include <MIDIUSB.h>
#include "MidiEventSink.h"
#include "SerialDebugSink.h"
#include "SynthEventSink.h"

namespace {
MatrixScanner matrixScanner;
MicInput micInput;
I2SAudioOutput audioOutput;
SynthEngine synth(audioOutput);
SynthEventSink synthSink(synth);
MidiEventSink midiSink;
SerialDebugSink debugSink;
AppController app(matrixScanner, micInput, synth, synthSink, midiSink, debugSink);
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

  app.begin();

  Serial.println();
  Serial.println("synth_v1");
  Serial.println("I2S: GP18 BCLK, GP19 LRCLK, GP20 DIN");
  Serial.println("ADC: GP26");
  app.printStatus();
}

void loop() {
  app.tick();
}
