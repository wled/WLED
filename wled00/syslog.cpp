#include "wled.h"
#ifdef WLED_ENABLE_SYSLOG

#include "syslog.h"

// Comment out but preserve the protocol names
/*
static const char* const protoNames[] PROGMEM = {
  PSTR("BSD"),
  PSTR("RFC5424"),
  PSTR("RAW"),
  PSTR("UNKNOWN")
};
static const uint8_t protoCount = sizeof(protoNames)/sizeof(*protoNames);
*/

// Comment out but preserve facility names
/*
//  We fill invalid entries with PSTR("UNKNOWN") so we can index 0..24 safely
static const char* const facilityNames[] PROGMEM = {
  PSTR("KERN"),   PSTR("USER"),   PSTR("UNKNOWN"), PSTR("DAEMON"),
  PSTR("UNKNOWN"),PSTR("SYSLOG"), PSTR("UNKNOWN"), PSTR("UNKNOWN"),
  PSTR("UNKNOWN"),PSTR("UNKNOWN"),PSTR("UNKNOWN"), PSTR("UNKNOWN"),
  PSTR("UNKNOWN"),PSTR("UNKNOWN"),PSTR("UNKNOWN"), PSTR("UNKNOWN"),
  PSTR("LCL0"),   PSTR("LCL1"),   PSTR("LCL2"),    PSTR("LCL3"),
  PSTR("LCL4"),   PSTR("LCL5"),   PSTR("LCL6"),    PSTR("LCL7"),
  PSTR("UNKNOWN")  // catch-all at index 24
};
static const uint8_t facCount   = sizeof(facilityNames)/sizeof(*facilityNames);
*/

// Comment out but preserve severity names
/* 
static const char* const severityNames[] PROGMEM = {
  PSTR("EMERG"), PSTR("ALERT"), PSTR("CRIT"),  PSTR("ERR"),
  PSTR("WARN"),  PSTR("NOTE"),  PSTR("INFO"),  PSTR("DEBUG"),
  PSTR("UNKNOWN")
};
static const uint8_t sevCount = sizeof(severityNames) / sizeof(*severityNames);
*/

SyslogPrinter::SyslogPrinter() : 
  _lastOperationSucceeded(true),
  _lastErrorMessage(""),
  _facility(SYSLOG_LOCAL0), 
  _severity(SYSLOG_DEBUG), 
  _protocol(SYSLOG_PROTO_BSD),
  _appName("WLED"),
  _bufferIndex(0) {}

void SyslogPrinter::begin(const char* host, uint16_t port,
					  uint8_t facility, uint8_t severity, uint8_t protocol) {

  DEBUG_PRINTF_P(PSTR("===== WLED SYSLOG CONFIGURATION =====\n"));
  DEBUG_PRINTF_P(PSTR(" Hostname:  %s\n"), host);
  DEBUG_PRINTF_P(PSTR(" Cached IP: %s\n"), syslogHostIP.toString().c_str());
  DEBUG_PRINTF_P(PSTR(" Port:      %u\n"), (unsigned)port);

  /*
  // Protocol
  uint8_t pidx = protocol < (protoCount - 1) ? protocol : (protoCount - 1);
  const char* pstr = (const char*)pgm_read_ptr(&protoNames[pidx]);
  DEBUG_PRINTF_P(PSTR(" Protocol:  %u (%s)\n"), (unsigned)protocol, pstr);

  // — Facility
  uint8_t fidx = facility < (facCount - 1) ? facility : (facCount - 1);
  const char* fstr = (const char*)pgm_read_ptr(&facilityNames[fidx]);
  DEBUG_PRINTF_P(PSTR(" Facility:  %u (%s)\n"), (unsigned)facility, fstr);

  // Severity
  uint8_t idx = severity < sevCount-1 ? severity : sevCount-1;
  const char* sevStr = (const char*)pgm_read_ptr(&severityNames[idx]);
  DEBUG_PRINTF_P(PSTR(" Severity:  %u (%s)\n"), (unsigned)severity, sevStr);
  */

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

  // Skip pure "#015" lines
  if (_bufferIndex == 4 && memcmp(_buffer, "#015", 4) == 0) {
    _bufferIndex = 0;
    return;
  }

  // Check if the message contains only whitespace
  bool onlyWhitespace = true;
  for (size_t i = 0; i < _bufferIndex; i++) {
    if (_buffer[i] != ' ' && _buffer[i] != '\t') {
      onlyWhitespace = false;
      break;
    }
  }
  if (onlyWhitespace) {
    _bufferIndex = 0;
    return;
  }

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
  
  // If newline or buffer full, flush buffer
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

  // Check for special case - literal "#015" string
  if (size >= 4 && buf[0] == '#' && buf[1] == '0' && buf[2] == '1' && buf[3] == '5') {
    return size; // Skip sending this message
  }

  // Skip empty messages
  if (size == 0) return 0;

  // Check if the message contains only whitespace
  bool onlyWhitespace = true;
  for (size_t i = 0; i < size; i++) {
    if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r' && buf[i] != '\n') {
      onlyWhitespace = false;
      break;
    }
  }
  if (onlyWhitespace) return size; // Skip sending this message

  syslogUdp.beginPacket(syslogHostIP, syslogPort);
  
  // Calculate priority value
  uint8_t pri = (_facility << 3) | severity;

  // Add hostname (replacing spaces with underscores) and app name
  String cleanHostname = String(serverDescription);
  cleanHostname.replace(' ', '_');

  // Handle different syslog protocol formats
  // switch (_protocol) {
    // case SYSLOG_PROTO_BSD:
      // RFC 3164 format: <PRI>TIMESTAMP HOSTNAME APP-NAME: MESSAGE
      syslogUdp.printf("<%d>", pri);
      
      if (ntpEnabled && ntpConnected) {
        // Month abbreviation
        static const char* const months[] = {
          "Jan","Feb","Mar","Apr","May","Jun",
          "Jul","Aug","Sep","Oct","Nov","Dec"
        };

        syslogUdp.printf("%s %2d %02d:%02d:%02d ", 
                        months[month(localTime) - 1], 
                        day(localTime),
                        hour(localTime), 
                        minute(localTime), 
                        second(localTime));
      } else {
        // No valid time available
        syslogUdp.print(F("Jan 01 00:00:00 "));
      }
      
      // Add hostname and app name
      syslogUdp.print(cleanHostname);
      syslogUdp.print(" ");
      syslogUdp.print(_appName);
      syslogUdp.print(": ");
      
      // Add message content
      size = syslogUdp.write(buf, size);
      /* 
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
        syslogUdp.print(F("1970-01-01T00:00:00Z "));
      }
      
      // Add hostname, app name, and other fields (using - for empty fields)
      syslogUdp.print(cleanHostname);
      syslogUdp.print(" ");
      syslogUdp.print(_appName);
      syslogUdp.print(F(" - - - ")); // PROCID, MSGID, and STRUCTURED-DATA are empty
      
      // Add message content
      size = syslogUdp.write(buf, size);
      break;
      
    case SYSLOG_PROTO_RAW:
    default:
      // Just send the raw message (like original NetDebug)
      size = syslogUdp.write(buf, size);
      break;
  }
  */
  
  syslogUdp.endPacket();
  return size;
}

SyslogPrinter Syslog;
#endif // WLED_ENABLE_SYSLOG