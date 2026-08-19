#ifndef WLED_SYSLOG_H
#define WLED_SYSLOG_H
#include <WString.h>
#include <WiFiUdp.h>

// Buffer management
#ifndef SYSLOG_BUFFER_SIZE
  #define SYSLOG_BUFFER_SIZE 128
#endif

#define SYSLOG_LOCAL0   	16 // local use 0
#define SYSLOG_DEBUG    7  // Debug: debug-level messages

// Syslog protocol formats - commented out but preserved
#define SYSLOG_PROTO_BSD       0  // Legacy BSD format (RFC 3164)

class SyslogPrinter : public Print {
  private:
    WiFiUDP syslogUdp; // needs to be here otherwise UDP messages get truncated upon destruction
    IPAddress syslogHostIP;
    bool resolveHostname();
    bool _lastOperationSucceeded;
    String _lastErrorMessage;

    // Syslog configuration - using globals from wled.h
    uint8_t _facility;  // Internal copy of syslogFacility (from wled.h), fixed to SYSLOG_LOCAL0
    uint8_t _severity;  // Internal copy of syslogSeverity (from wled.h), fixed to SYSLOG_DEBUG
    uint8_t _protocol;  // Internal copy of syslogProtocol (from wled.h), fixed to SYSLOG_PROTO_BSD
    String _appName;

    char _buffer[SYSLOG_BUFFER_SIZE]; // Buffer for collecting characters

    size_t _bufferIndex;
    void flushBuffer();

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