#ifndef WLED_SYSLOG_H
#define WLED_SYSLOG_H
#include <WString.h>
#include <WiFiUdp.h>

// Buffer management
#ifndef SYSLOG_BUFFER_SIZE
  #define SYSLOG_BUFFER_SIZE 128
#endif

// Syslog facility codes - commented out but preserved
/*
#define SYSLOG_KERN     	0  // kernel messages
#define SYSLOG_USER     	1  // user-level messages
#define SYSLOG_DAEMON   	3  // system daemons
#define SYSLOG_SYSLOG   	5  // messages generated internally by syslogd
*/
#define SYSLOG_LOCAL0   	16 // local use 0
/*
#define SYSLOG_LOCAL1   	17 // local use 1
#define SYSLOG_LOCAL2   	18 // local use 2
#define SYSLOG_LOCAL3   	19 // local use 3
#define SYSLOG_LOCAL4   	20 // local use 4
#define SYSLOG_LOCAL5   	21 // local use 5
#define SYSLOG_LOCAL6   	22 // local use 6
#define SYSLOG_LOCAL7   	23 // local use 7
*/

// Syslog severity levels - commented out but preserved
/*
#define SYSLOG_EMERG    0  // Emergency: system is unusable
#define SYSLOG_ALERT    1  // Alert: action must be taken immediately
#define SYSLOG_CRIT     2  // Critical: critical conditions
#define SYSLOG_ERR      3  // Error: error conditions
#define SYSLOG_WARNING  4  // Warning: warning conditions
#define SYSLOG_NOTICE   5  // Notice: normal but significant condition
#define SYSLOG_INFO     6  // Informational: informational messages
*/
#define SYSLOG_DEBUG    7  // Debug: debug-level messages

// Syslog protocol formats - commented out but preserved
#define SYSLOG_PROTO_BSD       0  // Legacy BSD format (RFC 3164)
/*
#define SYSLOG_PROTO_RFC5424   1  // Modern syslog format (RFC 5424)
#define SYSLOG_PROTO_RAW       2  // Raw text (like original NetDebug)
*/

class SyslogPrinter : public Print {
  private:
    WiFiUDP syslogUdp; // needs to be here otherwise UDP messages get truncated upon destruction
    IPAddress syslogHostIP;
    bool resolveHostname();
    bool _lastOperationSucceeded;
    String _lastErrorMessage;

    // Syslog configuration - using globals from wled.h
    uint8_t _facility;  // Fixed to SYSLOG_LOCAL0
    uint8_t _severity;  // Fixed to SYSLOG_DEBUG
    uint8_t _protocol;  // Fixed to SYSLOG_PROTO_BSD
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