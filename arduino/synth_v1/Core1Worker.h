#pragma once

#include "IParallelWorker.h"

// Runs jobs on the RP2040's second core. The sketch's loop1() must call
// serviceFromCore1(); start()/wait() hand jobs over through the cores' FIFO.
class Core1Worker final : public IParallelWorker {
public:
  void start(Job job, void *context) override;
  void wait() override;
  void serviceFromCore1();

private:
  volatile Job job_ = nullptr;
  void *volatile context_ = nullptr;
};
