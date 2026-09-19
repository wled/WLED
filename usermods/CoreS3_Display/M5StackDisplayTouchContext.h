#pragma once

#include "M5StackDisplayTouchState.h"

// Explicit input boundary for the Touch state-machine entry points.
//
// The contexts do not own data. They only reference the existing
// M5StackTouchRuntimeState and the current hit-test snapshot/time.
// This prepares Press/Hold/Release for later physical extraction
// without changing the state-machine algorithm.
struct M5StackTouchFrameContext {
  M5StackTouchRuntimeState& state;
  const M5StackTouchHitState& hit;
  unsigned long now;
};

struct M5StackTouchReleaseContext {
  M5StackTouchRuntimeState& state;
  unsigned long now;
};
