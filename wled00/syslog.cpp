#include "wled.h"
#ifdef WLED_ENABLE_SYSLOG

#include "syslog.h"

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

  // Trim trailing CR/LF so we don't send extra blank lines
  while (_bufferIndex > 0 && (_buffer[_bufferIndex - 1] == '\r' || _buffer[_bufferIndex - 1] == '\n')) {
    _bufferIndex--;
  }
  if (_bufferIndex == 0) return;

  // Check if the message contains only whitespace
  bool onlyWhitespace = true;
  for (size_t i = 0; i < _bufferIndex; i++) {
    if (_buffer[i] != ' ' && _buffer[i] != '\t' && _buffer[i] != '\r' && _buffer[i] != '\n') {
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
  if (buf == nullptr || size == 0) return 0;
  for (size_t i = 0; i < size; i++) {
    write(buf[i]);
  }
  return size;
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
  if (syslogHost[0] == '\0') {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Syslog host is empty");
    return 0;
  }
  if (syslogPort <= 0) {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Syslog port is invalid");
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

  // Trim trailing CR/LF to avoid blank lines
  while (size > 0 && (buf[size - 1] == '\r' || buf[size - 1] == '\n')) {
    size--;
  }
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

  if (syslogUdp.beginPacket(syslogHostIP, syslogPort) != 1) {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Failed to begin UDP packet");
    return 0;
  }
  
  // Calculate priority value
  uint8_t pri = (_facility << 3) | severity;

  // Add hostname (replacing spaces with underscores) and app name
  String cleanHostname = String(serverDescription);
  cleanHostname.replace(' ', '_');

  // Note: Only BSD protocol is currently implemented
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
  
  if (syslogUdp.endPacket() != 1) {
    _lastOperationSucceeded = false;
    _lastErrorMessage = F("Failed to send UDP packet");
    return 0;
  }
  return size;
}

SyslogPrinter Syslog;
#endif // WLED_ENABLE_SYSLOG
