// handler_queue.h
// deferred execution handler queue.  Used for making access to WLED globals safe.

#include <functional>

namespace HandlerQueue {
  
  // Enqueue a function on the main task
  bool callOnMainTask(std::function<void()> func);

  // Run the next task in the queue, if any
  void runNext();
}
