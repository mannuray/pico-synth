#include "Core1Worker.h"
#include "AudioHot.h"

#include <Arduino.h>

void AUDIO_HOT(Core1Worker::start)(Job job, void *context) {
  job_ = job;
  context_ = context;
  __sync_synchronize();  // job and its inputs visible before core 1 wakes
  rp2040.fifo.push(1);
}

void AUDIO_HOT(Core1Worker::wait)() {
  rp2040.fifo.pop();
  __sync_synchronize();  // core 1's results visible to this core
}

void AUDIO_HOT(Core1Worker::serviceFromCore1)() {
  rp2040.fifo.pop();
  __sync_synchronize();
  job_(context_);
  __sync_synchronize();
  rp2040.fifo.push(1);
}
