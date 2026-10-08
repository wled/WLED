#pragma once
/*
 * log_buffer.h — PSRAM-backed ring buffer for capturing log output.
 *
 * When a device has PSRAM (BOARD_HAS_PSRAM), a 32 KB ring buffer is
 * allocated at runtime (after psramFound() confirms the hardware).
 * Log entries written via WLED_LOG() / WLED_LOGF() are stored there and
 * exposed through the /json/log HTTP endpoint and the /log web UI page,
 * so end-users can retrieve diagnostic information without serial access
 * or custom builds.
 *
 * Two thin helpers are provided:
 *
 *   LogBuffer   – bare ring buffer over a ps_malloc'd char array.
 *   LogPrint    – Arduino Print subclass that writes into a LogBuffer.
 *
 * Thread / ISR safety: writes use a portMUX spinlock so the buffer
 * remains coherent when both the main loop and the AsyncWebServer task
 * call WLED_LOG() concurrently.
 */

#include <Arduino.h>
#include <Print.h>

class LogBuffer {
public:
  // Default capacity stored in PSRAM.  Raising this value is safe as long as
  // the device has enough PSRAM; it has no effect when PSRAM is absent.
  static constexpr size_t CAPACITY = 32 * 1024; // 32 KB

  LogBuffer()
    : _buf(nullptr), _head(0), _used(0)
  {
    _mux = portMUX_INITIALIZER_UNLOCKED;
  }

  // Allocate the PSRAM backing store.  Call once after psramFound() == true.
  // Returns true on success.
  bool init() {
    _buf = static_cast<char*>(ps_malloc(CAPACITY));
    return _buf != nullptr;
  }

  bool available() const { return _buf != nullptr; }
  size_t size()    const { return _used; }

  // Write raw bytes into the ring buffer (overwrites oldest data when full).
  void write(const char* data, size_t len) {
    if (!_buf || !len) return;
    portENTER_CRITICAL(&_mux);
    for (size_t i = 0; i < len; i++) {
      _buf[_head] = data[i];
      _head = (_head + 1) % CAPACITY;
      if (_used < CAPACITY) _used++;
    }
    portEXIT_CRITICAL(&_mux);
  }

  // Stream the ring-buffer contents (oldest byte first) into any Print sink.
  // Returns the number of bytes emitted.
  size_t printTo(Print& out) const {
    if (!_buf || !_used) return 0;
    if (_used < CAPACITY) {
      // Buffer has not wrapped yet — data starts at index 0.
      out.write(reinterpret_cast<const uint8_t*>(_buf), _used);
      return _used;
    }
    // Buffer has wrapped — oldest byte is at _head.
    out.write(reinterpret_cast<const uint8_t*>(_buf + _head), CAPACITY - _head);
    out.write(reinterpret_cast<const uint8_t*>(_buf),         _head);
    return CAPACITY;
  }

  void clear() {
    portENTER_CRITICAL(&_mux);
    _head = 0;
    _used = 0;
    portEXIT_CRITICAL(&_mux);
  }

private:
  char*           _buf;   // PSRAM allocation (nullptr until init())
  size_t          _head;  // Next write position (= oldest byte when full)
  size_t          _used;  // Bytes written, capped at CAPACITY
  portMUX_TYPE    _mux;   // Spinlock for concurrent write protection
};


// Arduino Print subclass that forwards write() calls into a LogBuffer.
// Use this as the target of DEBUGOUT to capture all debug output.
class LogPrint : public Print {
public:
  explicit LogPrint(LogBuffer& log) : _log(log) {}

  size_t write(uint8_t c) override {
    char ch = static_cast<char>(c);
    _log.write(&ch, 1);
    return 1;
  }

  size_t write(const uint8_t* buf, size_t size) override {
    _log.write(reinterpret_cast<const char*>(buf), size);
    return size;
  }

  LogBuffer& buffer() { return _log; }

private:
  LogBuffer& _log;
};


extern LogBuffer wledLogBuffer;  // PSRAM ring buffer (allocated in wled.cpp)
extern LogPrint  wledLog;        // Print wrapper around wledLogBuffer
