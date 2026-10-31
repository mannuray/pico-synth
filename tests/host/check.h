#pragma once
#include <stdio.h>

#include "Feedback.h"

// Feedback sounds for engines that don't test them (idle unless cued).
static FeedbackSounds gFeedback;

static int gFailures = 0;
#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
      ++gFailures;                                                    \
    }                                                                 \
  } while (0)

static int finish(const char *name) {
  printf(gFailures ? "%s: %d FAILURES\n" : "%s: all passed\n", name, gFailures);
  return gFailures != 0;
}
