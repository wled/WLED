#ifndef WLED_SYSLOG_H
#define WLED_SYSLOG_H
#include <WString.h>
#include <WiFiUdp.h>
#ifdef ARDUINO_ARCH_ESP32
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
#endif

// Line buffering: output is collected per task (ESP32) or per context
// (ESP8266: loop vs. network callbacks) until a newline, so every UDP packet
// carries exactly one complete line and concurrent writers can't mix lines.
// Longer lines are truncated and end in "...", never split across packets.
#ifndef SYSLOG_BUFFER_SIZE
  #ifdef ESP8266
    #define SYSLOG_BUFFER_SIZE 256
  #else
    #define SYSLOG_BUFFER_SIZE 512
  #endif
#endif
#ifndef SYSLOG_LINE_SLOTS
  #ifdef ESP8266
    #define SYSLOG_LINE_SLOTS 2
  #else
    #define SYSLOG_LINE_SLOTS 4
  #endif
#endif

#define SYSLOG_LOCAL0   	16 // local use 0
#define SYSLOG_DEBUG    7  // Debug: debug-level messages

// Syslog protocol formats - commented out but preserved
#define SYSLOG_PROTO_BSD       0  // Legacy BSD format (RFC 3164)

// Minimum time between blocking hostByName() retries while resolution keeps
// failing, so a misconfigured/unreachable syslog host doesn't stall the main
// loop on every single debug line.
#ifndef SYSLOG_RESOLVE_RETRY_MS
  #define SYSLOG_RESOLVE_RETRY_MS 10000UL
#endif

// How long a task waits for another task's log output before dropping its own.
#ifndef SYSLOG_LOCK_TIMEOUT_MS
  #define SYSLOG_LOCK_TIMEOUT_MS 50
#endif

class SyslogPrinter : public Print {
  private:
    WiFiUDP syslogUdp; // needs to be here otherwise UDP messages get truncated upon destruction
    IPAddress syslogHostIP;
    unsigned long _lastResolveAttempt = 0; // backoff so a failing DNS lookup isn't retried on every log line
    bool _hasAttemptedResolve = false;     // ensures the very first attempt isn't delayed by the backoff above
    bool resolveHostname();
    bool _lastOperationSucceeded;
    String _lastErrorMessage;

    // Syslog configuration - using globals from wled.h
    uint8_t _facility;  // Internal copy of syslogFacility (from wled.h), fixed to SYSLOG_LOCAL0
    uint8_t _severity;  // Internal copy of syslogSeverity (from wled.h), fixed to SYSLOG_DEBUG
    uint8_t _protocol;  // Internal copy of syslogProtocol (from wled.h), fixed to SYSLOG_PROTO_BSD
    String _appName;

    // Cache of the space-to-underscore-cleaned serverDescription, so every
    // single packet send doesn't need a fresh heap allocation - only
    // recomputed when serverDescription actually changes.
    String _cachedHostname;
    String _cachedSourceDescription;
    const String& cleanedHostname();

    // One partial line per writer. owner == nullptr marks a free slot.
    struct LineSlot {
      const void* owner;
      unsigned long lastWrite;
      size_t len;
      bool truncated;
      char buf[SYSLOG_BUFFER_SIZE];
    };
    LineSlot _slots[SYSLOG_LINE_SLOTS];

    static const void* currentWriter();
    LineSlot* slotFor(const void* owner);
    static void appendChar(LineSlot* slot, uint8_t c);
    void flushSlot(LineSlot* slot);
    size_t sendPacket(const uint8_t *buf, size_t size, uint8_t severity);

    // Debug output comes from the main loop and from async network tasks,
    // which share the line slots and syslogUdp.
    bool lock();
    void unlock();
    #ifdef ARDUINO_ARCH_ESP32
      StaticSemaphore_t _lockStorage;
      SemaphoreHandle_t _lock;
    #endif

  public:
    SyslogPrinter();
	void begin(const char* host, uint16_t port,
               uint8_t facility = SYSLOG_LOCAL0,
               uint8_t severity = SYSLOG_DEBUG,
               uint8_t protocol = SYSLOG_PROTO_BSD);
    void setAppName(const String &appName);

    // Print interface implementation
    virtual size_t write(uint8_t c);
    virtual size_t write(const uint8_t *buf, size_t size);

    // Severity override for specific messages
    size_t write(const uint8_t *buf, size_t size, uint8_t severity);

    // Error handling
    bool lastOperationSucceeded() const { return _lastOperationSucceeded; }
    String getLastErrorMessage() const { return _lastErrorMessage; }
};

// Default instance
extern SyslogPrinter Syslog;

#endif