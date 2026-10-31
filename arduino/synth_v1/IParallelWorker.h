#pragma once

// Runs a job alongside the caller: on the RP2040 on the second core
// (Core1Worker), on a desktop simply inline (InlineWorker). The job must only
// touch data the caller doesn't use until wait() returns.
class IParallelWorker {
public:
  using Job = void (*)(void *context);

  virtual ~IParallelWorker() = default;
  virtual void start(Job job, void *context) = 0;
  virtual void wait() = 0;
};

// Runs the job immediately; for tests and single-core builds.
class InlineWorker final : public IParallelWorker {
public:
  void start(Job job, void *context) override { job(context); }
  void wait() override {}
};
