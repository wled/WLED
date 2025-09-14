#include "handler_queue.h"
#include <deque>
#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)

static StaticSemaphore_t handlerQueueMutexBuffer;
static SemaphoreHandle_t handlerQueueMutex = xSemaphoreCreateMutexStatic(&handlerQueueMutexBuffer);
struct guard_type {
  SemaphoreHandle_t _mtx;
  guard_type(SemaphoreHandle_t m) : _mtx(m) {
    xSemaphoreTake(_mtx, portMAX_DELAY);  // todo: error check
  }
  ~guard_type() {    
    xSemaphoreGive(_mtx);
  }
};
#define guard() const guard_type guard(handlerQueueMutex)
#else
#define guard()
#endif

static std::deque<std::function<void()>> handler_queue;

// Enqueue a function on the main task
bool HandlerQueue::callOnMainTask(std::function<void()> func) {
  guard();
  handler_queue.push_back(std::move(func));
  return true;  // TODO: queue limit
}

// Run the next task in the queue, if any
void HandlerQueue::runNext() {
  std::function<void()> f;
  {
    guard();
    if (handler_queue.size()) {
      f = std::move(handler_queue.front());
      handler_queue.pop_front();
    }
  }
  if (f) {
    auto t1 = millis();    
    f();
    auto t2 = millis();
    Serial.printf("Handler took: %d\n", t2-t1);
  } 
}

