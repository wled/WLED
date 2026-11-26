#include "wled.h"
#ifdef WLED_ENABLE_SYSLOG

#include "syslog.h"
#ifdef ESP8266
  #include <coredecls.h> // can_yield()
#endif

SyslogPrinter::SyslogPrinter() : 
  _lastOperationSucceeded(true),
  _lastErrorMessage(""),
  _facility(SYSLOG_LOCAL0), 
  _severity(SYSLOG_DEBUG), 
  _protocol(SYSLOG_PROTO_BSD),
  _appName("WLED") {
  memset(_slots, 0, sizeof(_slots));
  #ifdef ARDUINO_ARCH_ESP32
    _lock = xSemaphoreCreateRecursiveMutexStatic(&_lockStorage);
  #endif
}

bool SyslogPrinter::lock() {
  #ifdef ARDUINO_ARCH_ESP32
    if (xPortInIsrContext()) return false;
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) return true;
    return xSemaphoreTakeRecursive(_lock, pdMS_TO_TICKS(SYSLOG_LOCK_TIMEOUT_MS)) == pdTRUE;
  #else
    return true;
  #endif
}

void SyslogPrinter::unlock() {
  #ifdef ARDUINO_ARCH_ESP32
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) xSemaphoreGiveRecursive(_lock);
  #endif
}

void SyslogPrinter::begin(const char* host, uint16_t port,
					  uint8_t facility, uint8_t severity, uint8_t protocol) {

  DEBUG_PRINTF_P(PSTR("===== WLED SYSLOG CONFIGURATION =====\n"));
  DEBUG_PRINTF_P(PSTR(" Hostname:  %s\n"), host);
  DEBUG_PRINTF_P(PSTR(" Cached IP: %s\n"), syslogHostIP.toString().c_str());
  DEBUG_PRINTF_P(PSTR(" Port:      %u\n"), (unsigned)port);
  DEBUG_PRINTF_P(PSTR("======================================\n"));

  // Settings are saved from the web server task while other tasks may be
  // sending, so change the config only while holding the send lock. If the
  // lock times out, apply anyway: a lost settings change is worse.
  bool locked = lock();
  if (host != syslogHost) strlcpy(syslogHost, host, sizeof(syslogHost));
  syslogPort = port;
  _facility = facility;
  _severity = severity;
  _protocol = protocol;

  // clear any cached IP so resolveHostname() will run next write()  
  syslogHostIP = IPAddress(0,0,0,0);
  if (locked) unlock();
}

void SyslogPrinter::setAppName(const String &appName) {
  if (_appName == appName) return; // avoid reallocating a String another task may be printing
  bool locked = lock();
  _appName = appName;
  if (locked) unlock();
}

bool SyslogPrinter::resolveHostname() {
  if (!WLED_CONNECTED || !syslogEnabled) return false;
  
  // If we already have an IP or can parse the hostname as an IP, use that
  if (syslogHostIP || syslogHostIP.fromString(syslogHost)) {
    return true;
  }

  // Avoid hammering the (blocking) DNS resolver on every single log line
  // when resolution keeps failing (misconfigured/unreachable host) - this
  // is the hot path for the entire firmware's debug output, and repeated
  // blocking calls here can stall the main loop badly enough to trip the
  // watchdog.
  #ifdef ESP8266
    // hostByName() yields, which crashes when called from a network callback
    // (SYS context). Only the loop context may resolve; callbacks wait for it.
    if (!can_yield()) return false;
  #endif

  unsigned long now = millis();
  if (_hasAttemptedResolve && (now - _lastResolveAttempt < SYSLOG_RESOLVE_RETRY_MS)) {
    return false;
  }
  _hasAttemptedResolve = true;
  _lastResolveAttempt = now;

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

const String& SyslogPrinter::cleanedHostname() {
  if (_cachedSourceDescription != serverDescription) {
    _cachedSourceDescription = serverDescription;
    _cachedHostname = _cachedSourceDescription;
    _cachedHostname.replace(' ', '_');
  }
  return _cachedHostname;
}

// Identifies who is writing, so each writer gets its own line buffer.
const void* SyslogPrinter::currentWriter() {
  #ifdef ARDUINO_ARCH_ESP32
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return (const void*)1;
    return xTaskGetCurrentTaskHandle();
  #else
    // ESP8266 has no tasks, but network callbacks (SYS context) can run while
    // the loop has an unfinished line.
    return can_yield() ? (const void*)1 : (const void*)2;
  #endif
}

// Caller holds the lock. Returns the writer's slot, or claims a free one.
SyslogPrinter::LineSlot* SyslogPrinter::slotFor(const void* owner) {
  LineSlot* freeSlot = nullptr;
  LineSlot* oldest = nullptr;
  for (size_t i = 0; i < SYSLOG_LINE_SLOTS; i++) {
    LineSlot* s = &_slots[i];
    if (s->owner == owner) return s;
    if (s->owner == nullptr) {
      if (!freeSlot) freeSlot = s;
    } else if (!oldest || (long)(s->lastWrite - oldest->lastWrite) < 0) {
      oldest = s;
    }
  }
  if (!freeSlot) {
    // More concurrent writers than slots: send the stalest partial line as is
    // and give its slot to the new writer.
    flushSlot(oldest);
    freeSlot = oldest;
  }
  freeSlot->owner = owner;
  freeSlot->len = 0;
  freeSlot->truncated = false;
  return freeSlot;
}

// Caller holds the lock. Sends the collected line (if any) and frees the slot.
void SyslogPrinter::flushSlot(LineSlot* slot) {
  size_t len = slot->len;
  if (slot->truncated && len >= 3) memcpy(slot->buf + len - 3, "...", 3);
  slot->owner = nullptr;
  slot->len = 0;
  slot->truncated = false;

  // Skip lines that are empty or whitespace only
  bool onlyWhitespace = true;
  for (size_t i = 0; i < len; i++) {
    if (slot->buf[i] != ' ' && slot->buf[i] != '\t') {
      onlyWhitespace = false;
      break;
    }
  }
  if (onlyWhitespace) return;

  sendPacket((const uint8_t*)slot->buf, len, _severity);
}

// Caller holds the lock.
void SyslogPrinter::appendChar(LineSlot* slot, uint8_t c) {
  slot->lastWrite = millis();
  if (slot->len < sizeof(slot->buf)) slot->buf[slot->len++] = c;
  else slot->truncated = true; // keep the start of the line, drop the rest
}

size_t SyslogPrinter::write(uint8_t c) {
  return write(&c, 1);
}

size_t SyslogPrinter::write(const uint8_t *buf, size_t size) {
  if (buf == nullptr || size == 0) return 0;
  if (!lock()) return size;
  const void* owner = currentWriter();
  LineSlot* slot = nullptr;
  for (size_t i = 0; i < SYSLOG_LINE_SLOTS; i++) {
    if (_slots[i].owner == owner) { slot = &_slots[i]; break; }
  }
  for (size_t i = 0; i < size; i++) {
    uint8_t c = buf[i];
    if (c == '\n') {
      // only a newline ends a line, so each packet is one complete line
      if (slot) flushSlot(slot);
      slot = nullptr;
    } else if (c != '\r') { // println() ends with "\r\n"; a CR would show as "#015"
      if (!slot) slot = slotFor(owner);
      appendChar(slot, c);
    }
  }
  unlock();
  return size;
}

size_t SyslogPrinter::write(const uint8_t *buf, size_t size, uint8_t severity) {
  if (!lock()) return 0;
  size_t sent = sendPacket(buf, size, severity);
  unlock();
  return sent;
}

size_t SyslogPrinter::sendPacket(const uint8_t *buf, size_t size, uint8_t severity) {
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

  // Check for special case - literal "#015" artifact line (exact match only,
  // matching flushBuffer()'s check - a longer message that merely starts
  // with these 4 bytes is real content and must not be dropped).
  if (size == 4 && buf[0] == '#' && buf[1] == '0' && buf[2] == '1' && buf[3] == '5') {
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
  syslogUdp.print(cleanedHostname());
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
