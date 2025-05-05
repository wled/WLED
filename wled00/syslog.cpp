#include "wled.h"
#ifdef WLED_ENABLE_SYSLOG

#include "syslog.h"

static const __FlashStringHelper* protoNames[] = { F("BSD"), F("RFC5424"), F("RAW") };

const char* getFacilityName(uint8_t code) {
  switch (code) {
    case 0:  return "KERN";
    case 1:  return "USER";
    case 3:  return "DAEMON";
    case 5:  return "SYSLOG";
    case 16: return "LCL0";
    case 17: return "LCL1";
    case 18: return "LCL2";
    case 19: return "LCL3";
    case 20: return "LCL4";
    case 21: return "LCL5";
    case 22: return "LCL6";
    case 23: return "LCL7";
    default: return "UNKNOWN";
  }
}

static const char* severityNames[] = {
  "EMERG","ALERT","CRIT","ERR","WARNING","NOTICE","INFO","DEBUG"
};

SyslogPrinter::SyslogPrinter() : 
  _facility(SYSLOG_LOCAL0), 
  _severity(SYSLOG_DEBUG), 
  _protocol(SYSLOG_PROTO_BSD),
  _appName("WLED"),
  _bufferIndex(0),
  _lastOperationSucceeded(true),
  _lastErrorMessage("") {}

void SyslogPrinter::begin(const char* host, uint16_t port,
					  uint8_t facility, uint8_t severity, uint8_t protocol) {

  DEBUG_PRINTF_P(PSTR("===== WLED SYSLOG CONFIGURATION =====\n"));
  DEBUG_PRINTF_P(PSTR(" Hostname:  %s\n"), syslogHost);
  DEBUG_PRINTF_P(PSTR(" Cached IP:  %s\n"), syslogHostIP.toString().c_str());
  DEBUG_PRINTF_P(PSTR(" Port:       %u\n"), (unsigned)syslogPort);
  DEBUG_PRINTF_P(PSTR(" Protocol:   %u (%s)\n"),
    protocol,
    protocol <= 2 ? (const char*)protoNames[protocol] : "UNKNOWN"
  );
  DEBUG_PRINTF_P(PSTR(" Facility:   %u (%s)\n"),
                 (unsigned)facility,
                 getFacilityName(facility));
  DEBUG_PRINTF_P(PSTR(" Severity:   %u (%s)\n"),
                 (unsigned)severity,
                 (severity <  sizeof(severityNames)/sizeof(severityNames[0]))
                   ? severityNames[severity]
                   : PSTR("UNKNOWN"));
  DEBUG_PRINTF_P(PSTR("======================================\n"));
  
  strlcpy(syslogHost, host, sizeof(syslogHost));
  syslogPort = port;
  _facility = facility;
  _severity = severity;
  _protocol = protocol;
  
  // clear any cached IP so resolveHostname() will run next write()  
  syslogHostIP = IPAddress(0,0,0,0);
}

void SyslogPrinter::setAppName(const String &appName) {
  _appName = appName;
}

bool SyslogPrinter::resolveHostname() {
  if (!WLED_CONNECTED || !syslogEnabled) return false;
  
  // If we already have an IP or can parse the hostname as an IP, use that
  if (syslogHostIP || syslogHostIP.fromString(syslogHost)) {
    return true;
  }
  
  // Otherwise resolve the hostname
  #ifdef ESP8266
    WiFi.hostByName(syslogHost, syslogHostIP, 750);
  #else
    #ifdef WLED_USE_ETHERNET
      ETH.hostByName(syslogHost, syslogHostIP);
    #else
      WiFi.hostByName(syslogHost, syslogHostIP);
    #endif
  #endif
  
  return syslogHostIP != IPAddress(0, 0, 0, 0);
}

void SyslogPrinter::flushBuffer() {
  if (_bufferIndex == 0) return;
  
   // Trim any trailing CR so syslog server won’t show “#015”
  if (_bufferIndex > 0 && _buffer[_bufferIndex-1] == '\r') _bufferIndex--;
  
  // Null-terminate
  _buffer[_bufferIndex] = '\0';
  
  // Send the buffer with default severity
  write((const uint8_t*)_buffer, _bufferIndex, _severity);
  
  // Reset buffer index
  _bufferIndex = 0;
}

size_t SyslogPrinter::write(uint8_t c) {
  // Store in buffer regardless of connection status
  if (_bufferIndex < sizeof(_buffer) - 1) {
    _buffer[_bufferIndex++] = c;
  }
  
  // If newline or buffer full, flush
  if (c == '\n' || _bufferIndex >= sizeof(_buffer) - 1) {
    flushBuffer();
  }
  
  return 1;
}

size_t SyslogPrinter::write(const uint8_t *buf, size_t size) {
  return write(buf, size, _severity);
}

size_t SyslogPrinter::write(const uint8_t *buf, size_t size, uint8_t severity) {
  _lastOperationSucceeded = true;
  if (!WLED_CONNECTED) {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Network not connected");
    return 0;
  }
  if (buf == nullptr) {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Null buffer provided");
    return 0;
  }
  if (!syslogEnabled) {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Syslog is disabled");
    return 0;
  }
  if (!resolveHostname()) {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Failed to resolve hostname");
    return 0;
  }
  
  syslogUdp.beginPacket(syslogHostIP, syslogPort);
  
  // Calculate priority value
  uint8_t pri = (_facility << 3) | severity;
  
  // Handle different syslog protocol formats
  switch (_protocol) {
    case SYSLOG_PROTO_BSD:	  
      // RFC 3164 format: <PRI>TIMESTAMP HOSTNAME APP-NAME: MESSAGE
      syslogUdp.printf("<%d>", pri);
      
      if (ntpEnabled && ntpConnected) {
        // Month abbreviation
        const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        
        syslogUdp.printf("%s %2d %02d:%02d:%02d ", 
                        months[month(localTime) - 1], 
                        day(localTime),
                        hour(localTime), 
                        minute(localTime), 
                        second(localTime));
      } else {
        // No valid time available
        syslogUdp.print("Jan 01 00:00:00 ");
      }
      
      // Add hostname and app name
      syslogUdp.print(serverDescription);
      syslogUdp.print(" ");
      syslogUdp.print(_appName);
      syslogUdp.print(": ");
      
      // Add message content
      size = syslogUdp.write(buf, size);
      break;
      
    case SYSLOG_PROTO_RFC5424:	  
      // RFC 5424 format: <PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID STRUCTURED-DATA MSG
      syslogUdp.printf("<%d>1 ", pri); // Version is always 1
      
      if (ntpEnabled && ntpConnected) {
        syslogUdp.printf("%04d-%02d-%02dT%02d:%02d:%02dZ ",
                        year(localTime),
                        month(localTime),
                        day(localTime),
                        hour(localTime),
                        minute(localTime),
                        second(localTime));
      } else {
        // No valid time available
        syslogUdp.print("1970-01-01T00:00:00Z ");
      }
      
      // Add hostname, app name, and other fields (using - for empty fields)
      syslogUdp.print(serverDescription);
      syslogUdp.print(" ");
      syslogUdp.print(_appName);
      syslogUdp.print(" - - - "); // PROCID, MSGID, and STRUCTURED-DATA are empty
      
      // Add message content
      size = syslogUdp.write(buf, size);
      break;
      
    case SYSLOG_PROTO_RAW:
    default:
      // Just send the raw message (like original NetDebug)
      size = syslogUdp.write(buf, size);
      break;
  }
  
  syslogUdp.endPacket();
  return size;
}

SyslogPrinter Syslog;
#endif // WLED_ENABLE_SYSLOG