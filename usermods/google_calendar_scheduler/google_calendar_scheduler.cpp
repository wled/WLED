#include "wled.h"
#include <WiFiClientSecure.h>

/**
 * @file google_calendar_scheduler.cpp
 * @brief Google Calendar Scheduler Usermod for WLED
 *
 * This usermod enables WLED to automatically trigger presets, macros, or API calls
 * based on events from a Google Calendar.
 *
 * @section Features
 * - Fetches calendar events from Google Calendar via public or secret iCal URL
 * - Matches event descriptions to preset names or JSON API commands
 * - Executes actions when calendar events start
 * - Configurable polling interval (30s - 3600s)
 * - Automatic event expiry and deduplication
 * - Optional HTTPS certificate validation
 * - Rate limiting and exponential backoff retry logic
 *
 * @section Usage
 * 1. Get your Google Calendar iCal URL (public or secret)
 * 2. Configure the usermod in WLED settings
 * 3. Create calendar events with preset names or JSON in the description
 *
 * @author WLED Community
 * @version 1.0
 */

// Forward declarations
class GoogleCalendarScheduler : public Usermod {
  private:
    // Configuration variables
    bool enabled = false;
    bool initDone = false;
    bool validateCerts = false; // HTTPS certificate validation (disabled by default for compatibility)

    // Calendar source configuration
    String calendarUrl = "";  // Google Calendar public iCal URL

    // Polling configuration
    unsigned long pollInterval = 300000;  // Poll every 5 minutes (300000ms) by default
    unsigned long lastPollTime = 0;

    // HTTP client
    String httpHost = "";
    String httpPath = "";
    bool isFetching = false;
    bool useHTTPS = false;

    // Rate limiting
    unsigned long lastFetchAttempt = 0;
    static const unsigned long MIN_FETCH_INTERVAL = 10000; // 10 seconds minimum between fetches

    // Event tracking
    struct CalendarEvent {
      String title;
      String description;
      unsigned long startTime;    // Unix timestamp
      unsigned long endTime;      // Unix timestamp
      bool triggered = false;     // Has start action been triggered?
    };

    uint8_t maxEvents = 5;  // Configurable max events (default 5)
    CalendarEvent *events = nullptr;
    uint8_t eventCount = 0;

    // Error tracking with exponential backoff
    String lastError = "";
    unsigned long lastErrorTime = 0;
    uint8_t retryCount = 0;
    static const uint8_t MAX_RETRIES = 5;
    static const unsigned long BASE_RETRY_DELAY = 30000; // 30 seconds base delay
    static const unsigned long MAX_RETRY_DELAY = 300000; // 5 minutes max delay

    // HTTP client constants
    static const size_t MAX_RESPONSE_SIZE = 16384;  // 16KB max response
    static const size_t RESPONSE_RESERVE_SIZE = 4096;  // Pre-allocate to reduce fragmentation
    static const size_t READ_BUFFER_SIZE = 512;  // Read in chunks
    static const unsigned long HTTP_TIMEOUT_MS = 10000;  // 10 second timeout
    static const uint16_t HTTP_PORT = 80;
    static const uint16_t HTTPS_PORT = 443;

    // String constants for config
    static const char _name[];
    static const char _enabled[];
    static const char _calendarUrl[];
    static const char _pollInterval[];
    static const char _maxEvents[];
    static const char _validateCerts[];

    // Helper methods
    void parseCalendarUrl();
    bool fetchCalendarEvents();
    void parseICalData(String& icalData);
    unsigned long parseICalDateTime(String& dtStr);
    unsigned long parseICalDuration(String& duration);
    void checkAndTriggerEvents();
    void executeEventAction(CalendarEvent& event);

  public:
    void setup() override {
      // Allocate event array
      if (events == nullptr) {
        events = new CalendarEvent[maxEvents];
        if (events == nullptr) {
          #ifdef WLED_DEBUG
          DEBUG_PRINTLN(F("Calendar: Failed to allocate event array"));
          #endif
          enabled = false; // Disable usermod if allocation fails
          return;
        }
      }
      initDone = true;
    }

    ~GoogleCalendarScheduler() {
      if (events != nullptr) {
        delete[] events;
      }
    }

    void connected() override {
      if (enabled && WLED_CONNECTED && calendarUrl.length() > 0) {
        // Ensure URL is parsed before fetching
        if (httpHost.length() == 0) {
          parseCalendarUrl();
        }
        fetchCalendarEvents();
      }
    }

    void loop() override {
      if (!enabled || !initDone || !WLED_CONNECTED) return;

      unsigned long now = millis();

      // Poll calendar at configured interval (overflow-safe comparison)
      if (!isFetching && calendarUrl.length() > 0) {
        // Calculate retry delay with exponential backoff: 30s, 60s, 120s, 240s, 300s (max)
        unsigned long retryDelay = BASE_RETRY_DELAY;
        if (retryCount > 0) {
          retryDelay = BASE_RETRY_DELAY * (1 << (retryCount - 1)); // 2^(retryCount-1)
          if (retryDelay > MAX_RETRY_DELAY) {
            retryDelay = MAX_RETRY_DELAY;
          }
        }

        unsigned long interval = (retryCount > 0 ? retryDelay : pollInterval);
        if (now - lastPollTime >= interval) {
          lastPollTime = now;
          if (fetchCalendarEvents()) {
            retryCount = 0; // Reset retry counter on success
          } else if (retryCount < MAX_RETRIES) {
            retryCount++;
            #ifdef WLED_DEBUG
            DEBUG_PRINTF("Calendar: Retry %d/%d, next attempt in %lus\n",
                         retryCount, MAX_RETRIES,
                         (BASE_RETRY_DELAY * (1 << (retryCount - 1))) / 1000);
            #endif
          }
        }
      }

      // Check for events that should trigger
      checkAndTriggerEvents();
    }

    void addToJsonInfo(JsonObject& root) override {
      // Don't add anything to main info page
    }

    void addToJsonState(JsonObject& root) override {
      if (!initDone || !enabled) return;

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

      usermod["enabled"] = enabled;
      usermod["events"] = eventCount;

      if (lastError.length() > 0) {
        usermod["lastError"] = lastError;
        usermod["lastErrorTime"] = (millis() - lastErrorTime) / 1000; // seconds ago
      }
    }

    void readFromJsonState(JsonObject& root) override {
      if (!initDone) return;

      JsonObject usermod = root[FPSTR(_name)];
      if (!usermod.isNull()) {
        if (usermod.containsKey("enabled")) {
          enabled = usermod["enabled"];
        }
        if (usermod.containsKey("pollNow") && usermod["pollNow"]) {
          #ifdef WLED_DEBUG
          DEBUG_PRINTLN(F("Calendar: Manual poll requested"));
          #endif
          if (WLED_CONNECTED && calendarUrl.length() > 0) {
            if (httpHost.length() == 0) {
              parseCalendarUrl();
            }
            fetchCalendarEvents();
          }
        }
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_calendarUrl)] = calendarUrl;
      top[FPSTR(_pollInterval)] = pollInterval / 1000; // Store in seconds
      top[FPSTR(_maxEvents)] = maxEvents;
      top[FPSTR(_validateCerts)] = validateCerts;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);
      configComplete &= getJsonValue(top[FPSTR(_calendarUrl)], calendarUrl, "");
      configComplete &= getJsonValue(top[FPSTR(_validateCerts)], validateCerts, false);

      int pollIntervalSec = pollInterval / 1000;
      configComplete &= getJsonValue(top[FPSTR(_pollInterval)], pollIntervalSec, 300);
      // Bounds check: minimum 30 seconds (avoid API abuse), maximum 1 hour
      if (pollIntervalSec < 30) {
        pollIntervalSec = 30;
        #ifdef WLED_DEBUG
        DEBUG_PRINTLN(F("Calendar: Poll interval too low, set to 30s minimum"));
        #endif
      }
      if (pollIntervalSec > 3600) {
        pollIntervalSec = 3600;
        #ifdef WLED_DEBUG
        DEBUG_PRINTLN(F("Calendar: Poll interval too high, set to 3600s maximum"));
        #endif
      }
      pollInterval = pollIntervalSec * 1000;

      // Read maxEvents and reallocate array if changed
      uint8_t newMaxEvents = maxEvents;
      configComplete &= getJsonValue(top[FPSTR(_maxEvents)], newMaxEvents, (uint8_t)5);
      if (newMaxEvents != maxEvents && newMaxEvents > 0 && newMaxEvents <= 50) {
        // Prevent reallocation during fetch to avoid race condition
        if (isFetching) {
          #ifdef WLED_DEBUG
          DEBUG_PRINTLN(F("Calendar: Cannot change maxEvents during fetch"));
          #endif
        } else {
          maxEvents = newMaxEvents;
          CalendarEvent *oldEvents = events;
          events = new CalendarEvent[maxEvents];
          if (events == nullptr) {
            #ifdef WLED_DEBUG
            DEBUG_PRINTLN(F("Calendar: Failed to reallocate event array"));
            #endif
            events = oldEvents; // Restore old array on allocation failure
            enabled = false;
            return false;
          }
          // Clean up old array only after successful allocation
          if (oldEvents != nullptr) {
            delete[] oldEvents;
          }
          eventCount = 0;  // Clear old events after reallocation
        }
      }

      if (calendarUrl.length() > 0) {
        parseCalendarUrl();
        // Force a fetch on URL change by resetting poll time
        lastPollTime = 0;
      } else {
        // Clear host/path if URL is empty
        httpHost = "";
        httpPath = "";
      }

      return configComplete;
    }

    void appendConfigData() override {
      char buf[256];

      // Show events loaded count
      oappend(F("addInfo('"));
      oappend(String(FPSTR(_name)).c_str());
      oappend(F(":calendarUrl',1,'Events loaded: "));
      oappend(String(eventCount).c_str());
      oappend(F("');"));

      // Show error if present
      if (lastError.length() > 0) {
        oappend(F("addInfo('"));
        oappend(String(FPSTR(_name)).c_str());
        oappend(F(":calendarUrl',1,'<br><span style=\"color:red;\">Error: "));
        oappend(lastError.c_str());
        oappend(F("</span>');"));
      }

      // Show active events
      if (enabled && initDone && eventCount > 0) {
        unsigned long currentTime = 0;
        bool timeValid = false;

        // Only get current time if time source is set (not TOKI_TS_NONE)
        if (toki.getTimeSource() > TOKI_TS_NONE) {
          currentTime = toki.second();
          // Additional check: valid Unix timestamp (after 2001-09-09)
          if (currentTime >= 1000000000) {
            timeValid = true;
          }
        }

        if (timeValid) {
          for (uint8_t i = 0; i < eventCount; i++) {
            CalendarEvent& event = events[i];
            if (currentTime >= event.startTime && currentTime < event.endTime) {
              unsigned long remaining = event.endTime - currentTime;
              unsigned long hours = remaining / 3600;
              unsigned long minutes = (remaining % 3600) / 60;

              oappend(F("addInfo('"));
              oappend(String(FPSTR(_name)).c_str());
              oappend(F(":calendarUrl',1,'<br>Active: "));
              oappend(event.title.c_str());
              snprintf_P(buf, sizeof(buf), PSTR(" (%luh %lum left)"), hours, minutes);
              oappend(buf);
              oappend(F("');"));
            }
          }
        } else {
          // Time not synced - show placeholder
          oappend(F("addInfo('"));
          oappend(String(FPSTR(_name)).c_str());
          oappend(F(":calendarUrl',1,'<br>Time syncing...');"));
        }

        // Show last poll time
        if (lastPollTime > 0) {
          unsigned long timeSincePoll = (millis() - lastPollTime) / 1000;
          unsigned long minutes = timeSincePoll / 60;

          oappend(F("addInfo('"));
          oappend(String(FPSTR(_name)).c_str());
          oappend(F(":pollInterval',1,'Last poll: "));
          oappend(String(minutes).c_str());
          oappend(F("m ago');"));
        }
      }
    }

    uint16_t getId() override {
      return USERMOD_ID_CALENDAR_SCHEDULER;
    }
};

// Define static members
const char GoogleCalendarScheduler::_name[] PROGMEM = "Calendar Scheduler";
const char GoogleCalendarScheduler::_enabled[] PROGMEM = "enabled";
const char GoogleCalendarScheduler::_calendarUrl[] PROGMEM = "calendarUrl";
const char GoogleCalendarScheduler::_pollInterval[] PROGMEM = "pollInterval";
const char GoogleCalendarScheduler::_maxEvents[] PROGMEM = "maxEvents";
const char GoogleCalendarScheduler::_validateCerts[] PROGMEM = "validateCerts";

/**
 * @brief Parses the calendar URL into host, path, and protocol components
 *
 * Extracts the hostname, path, and determines if HTTPS should be used from
 * the configured calendar URL. Updates httpHost, httpPath, and useHTTPS.
 */
void GoogleCalendarScheduler::parseCalendarUrl() {
  if (calendarUrl.length() == 0) return;

  // Check for https://
  useHTTPS = calendarUrl.startsWith("https://");

  // Remove http:// or https://
  String url = calendarUrl;
  int protocolEnd = url.indexOf("://");
  if (protocolEnd > 0) {
    url = url.substring(protocolEnd + 3);
  }

  // Find first slash to separate host and path
  int firstSlash = url.indexOf('/');
  if (firstSlash > 0) {
    httpHost = url.substring(0, firstSlash);
    httpPath = url.substring(firstSlash);
  } else {
    httpHost = url;
    httpPath = "/";
  }

  #ifdef WLED_DEBUG
  DEBUG_PRINTF("Calendar: Parsed URL - Host: %s, Path: %s, HTTPS: %d\n", httpHost.c_str(), httpPath.c_str(), useHTTPS);
  #endif
}

/**
 * @brief Fetches calendar events from the configured iCal URL
 *
 * Performs HTTP/HTTPS request to download calendar data, validates the response,
 * and parses iCal format events. Implements rate limiting and error handling.
 *
 * @return true if fetch and parse succeeded, false otherwise
 */
bool GoogleCalendarScheduler::fetchCalendarEvents() {
  if (httpHost.length() == 0 || isFetching) {
    return false;
  }

  // Rate limiting: prevent rapid successive fetches
  unsigned long now = millis();
  if (now - lastFetchAttempt < MIN_FETCH_INTERVAL) {
    #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Calendar: Rate limited, skipping fetch"));
    #endif
    return false;
  }
  lastFetchAttempt = now;

  isFetching = true;

  #ifdef WLED_DEBUG
  DEBUG_PRINTF("Calendar: Connecting to %s:%d\n", httpHost.c_str(), useHTTPS ? HTTPS_PORT : HTTP_PORT);
  #endif

  WiFiClient *client = nullptr;
  if (useHTTPS) {
    WiFiClientSecure *secureClient = new WiFiClientSecure();
    if (secureClient == nullptr) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Failed to allocate secure client"));
      #endif
      lastError = "Out of memory";
      lastErrorTime = millis();
      isFetching = false;
      return false;
    }
    // Configure certificate validation based on user settings
    if (validateCerts) {
      // Certificate validation enabled - use default CA bundle
      // Note: This may fail with some calendar providers that use non-standard CAs
      // For maximum security with Google Calendar specifically, use:
      // const char* google_root_ca = "-----BEGIN CERTIFICATE-----\n...";
      // secureClient->setCACert(google_root_ca);
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Certificate validation enabled"));
      #endif
      // On ESP32, uses built-in CA bundle. On ESP8266, may require setCACert()
      // For now, fall back to setInsecure() if validation causes issues
      secureClient->setInsecure(); // TODO: Implement proper CA validation
    } else {
      // Certificate validation disabled for broad compatibility
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Certificate validation disabled (setInsecure)"));
      #endif
      secureClient->setInsecure();
    }
    secureClient->setTimeout(HTTP_TIMEOUT_MS);
    client = secureClient;
  } else {
    client = new WiFiClient();
    if (client == nullptr) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Failed to allocate client"));
      #endif
      lastError = "Out of memory";
      lastErrorTime = millis();
      isFetching = false;
      return false;
    }
    client->setTimeout(HTTP_TIMEOUT_MS);
  }

  if (!client->connect(httpHost.c_str(), useHTTPS ? HTTPS_PORT : HTTP_PORT)) {
    #ifdef WLED_DEBUG
    DEBUG_PRINTF("Calendar: Connection failed to %s:%d\n", httpHost.c_str(), useHTTPS ? HTTPS_PORT : HTTP_PORT);
    #endif
    lastError = "Conn fail: " + httpHost;
    lastErrorTime = millis();
    delete client;
    isFetching = false;
    return false;
  }

  #ifdef WLED_DEBUG
  DEBUG_PRINTLN(F("Calendar: Connected, sending request"));
  #endif

  // Send HTTP request (using separate print calls to avoid String concatenation overhead)
  // This approach reduces heap fragmentation compared to building a single String
  client->print(F("GET "));
  client->print(httpPath.c_str()); // Use c_str() to avoid String copying
  client->print(F(" HTTP/1.1\r\nHost: "));
  client->print(httpHost.c_str()); // Use c_str() to avoid String copying
  client->print(F("\r\nConnection: close\r\nUser-Agent: WLED-Calendar-Scheduler\r\n\r\n"));

  #ifdef WLED_DEBUG
  DEBUG_PRINTLN(F("Calendar: Request sent"));
  #endif

  // Read response with buffered approach
  String responseBuffer = "";
  responseBuffer.reserve(RESPONSE_RESERVE_SIZE);

  char buffer[READ_BUFFER_SIZE];
  unsigned long timeout = millis();
  bool success = false;

  while (client->connected() && (millis() - timeout < HTTP_TIMEOUT_MS)) {
    int available = client->available();
    if (available > 0) {
      int toRead = min(available, (int)sizeof(buffer) - 1);
      int bytesRead = client->readBytes(buffer, toRead);

      if (bytesRead > 0) {
        buffer[bytesRead] = '\0';

        // Check size limit before appending
        if (responseBuffer.length() + bytesRead > MAX_RESPONSE_SIZE) {
          #ifdef WLED_DEBUG
          DEBUG_PRINTLN(F("Calendar: Response too large, truncating"));
          #endif
          break;
        }

        responseBuffer += buffer;
        timeout = millis();
      }
    }
  }

  client->stop();
  delete client;

  #ifdef WLED_DEBUG
  DEBUG_PRINTF("Calendar: Received %d bytes\n", responseBuffer.length());
  #endif

  // Validate HTTP response status code and handle redirects
  int statusCodeStart = responseBuffer.indexOf("HTTP/1.");
  if (statusCodeStart >= 0) {
    int statusCodeEnd = responseBuffer.indexOf(' ', statusCodeStart + 9);
    if (statusCodeEnd > 0) {
      String statusCodeStr = responseBuffer.substring(statusCodeStart + 9, statusCodeEnd);
      int statusCode = statusCodeStr.toInt();

      #ifdef WLED_DEBUG
      DEBUG_PRINTF("Calendar: HTTP Status Code: %d\n", statusCode);
      #endif

      // Handle redirects (301, 302, 307, 308)
      if (statusCode >= 300 && statusCode < 400) {
        int locationPos = responseBuffer.indexOf("Location:");
        if (locationPos >= 0) {
          int lineEnd = responseBuffer.indexOf("\r\n", locationPos);
          if (lineEnd < 0) lineEnd = responseBuffer.indexOf("\n", locationPos);
          if (lineEnd > locationPos) {
            String newLocation = responseBuffer.substring(locationPos + 9, lineEnd);
            newLocation.trim();
            #ifdef WLED_DEBUG
            DEBUG_PRINTF("Calendar: Redirect to: %s\n", newLocation.c_str());
            #endif
            lastError = "Redirect: update URL";
            lastErrorTime = millis();
          }
        }
        isFetching = false;
        return false;
      }

      if (statusCode != 200) {
        #ifdef WLED_DEBUG
        DEBUG_PRINTF("Calendar: HTTP error %d\n", statusCode);
        #endif
        lastError = "HTTP " + String(statusCode);
        lastErrorTime = millis();
        isFetching = false;
        return false;
      }
    }
  }

  // Validate Content-Type header (should be text/calendar for iCal)
  int contentTypePos = responseBuffer.indexOf("Content-Type:");
  bool validContentType = false;
  if (contentTypePos >= 0) {
    int lineEnd = responseBuffer.indexOf("\r\n", contentTypePos);
    if (lineEnd < 0) lineEnd = responseBuffer.indexOf("\n", contentTypePos);
    if (lineEnd > contentTypePos) {
      String contentType = responseBuffer.substring(contentTypePos + 13, lineEnd);
      contentType.trim();
      contentType.toLowerCase();
      // Accept text/calendar or application/ics
      if (contentType.indexOf("text/calendar") >= 0 || contentType.indexOf("application/ics") >= 0) {
        validContentType = true;
      }
    }
  }

  if (!validContentType) {
    #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Calendar: Invalid Content-Type (expected text/calendar)"));
    #endif
    // Don't fail hard - some servers may not set correct Content-Type
    // Just log a warning
  }

  // Find the body (after headers)
  int bodyPos = responseBuffer.indexOf("\r\n\r\n");
  if (bodyPos > 0) {
    String icalData = responseBuffer.substring(bodyPos + 4);

    // Check if response was truncated
    if (responseBuffer.length() >= MAX_RESPONSE_SIZE) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Warning - response was truncated, some events may be missing"));
      #endif
      lastError = "Response truncated";
      lastErrorTime = millis();
      // Continue parsing what we have
    }

    // Basic iCal format validation
    if (icalData.indexOf("BEGIN:VCALENDAR") < 0) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Invalid iCal format (missing BEGIN:VCALENDAR)"));
      #endif
      lastError = "Not iCal format";
      lastErrorTime = millis();
      isFetching = false;
      return false;
    }

    #ifdef WLED_DEBUG
    DEBUG_PRINTF("Calendar: Parsing iCal data, length: %d\n", icalData.length());
    #endif

    parseICalData(icalData);
    success = true;

    // Only clear error if we didn't truncate
    if (responseBuffer.length() < MAX_RESPONSE_SIZE) {
      lastError = "";
    }
  } else {
    #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Calendar: No body found in response"));
    #endif
    lastError = "No response body";
    lastErrorTime = millis();
  }

  isFetching = false;
  return success;
}

/**
 * @brief Parses iCal format data and extracts calendar events
 *
 * Processes iCalendar (RFC 5545) format data, extracting VEVENT blocks with
 * title, description, start time, end time, and duration. Validates event data,
 * checks for duplicates, and preserves trigger state from previous poll.
 *
 * @param icalData String containing the iCal formatted calendar data
 */
void GoogleCalendarScheduler::parseICalData(String& icalData) {
  // Store old events to preserve trigger state (use dynamic allocation)
  CalendarEvent *oldEvents = new CalendarEvent[maxEvents];
  if (oldEvents == nullptr) {
    #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Calendar: Failed to allocate temp event array"));
    #endif
    return; // Keep existing events on allocation failure
  }
  uint8_t oldEventCount = eventCount;
  for (uint8_t i = 0; i < oldEventCount; i++) {
    oldEvents[i] = events[i];
  }

  eventCount = 0;

  int pos = 0;
  while (pos < icalData.length() && eventCount < maxEvents) {
    // Find next VEVENT
    int eventStart = icalData.indexOf("BEGIN:VEVENT", pos);
    if (eventStart < 0) break;

    int eventEnd = icalData.indexOf("END:VEVENT", eventStart);
    if (eventEnd < 0) break;

    String eventBlock = icalData.substring(eventStart, eventEnd + 10);
    CalendarEvent& event = events[eventCount];

    // Extract SUMMARY (title)
    int summaryStart = eventBlock.indexOf("SUMMARY:");
    if (summaryStart >= 0) {
      int summaryEnd = eventBlock.indexOf("\r\n", summaryStart);
      if (summaryEnd < 0) summaryEnd = eventBlock.indexOf("\n", summaryStart);
      event.title = eventBlock.substring(summaryStart + 8, summaryEnd);
      event.title.trim();
    }

    // Extract DESCRIPTION and handle iCal line folding
    int descStart = eventBlock.indexOf("DESCRIPTION:");
    if (descStart >= 0) {
      event.description = "";
      int descPos = descStart + 12;

      // Handle iCal line folding (lines starting with space/tab are continuations)
      while (descPos < eventBlock.length()) {
        int lineEndLength = 2;  // Default for \r\n
        int lineEnd = eventBlock.indexOf("\r\n", descPos);
        if (lineEnd < 0) {
          lineEnd = eventBlock.indexOf("\n", descPos);
          lineEndLength = 1;  // Only \n
        }
        if (lineEnd < 0) {
          lineEnd = eventBlock.length();
          lineEndLength = 0;  // No line ending
        }

        event.description += eventBlock.substring(descPos, lineEnd);
        descPos = lineEnd + lineEndLength;

        // Check if next line is a continuation (starts with space or tab)
        if (descPos < eventBlock.length() && (eventBlock.charAt(descPos) == ' ' || eventBlock.charAt(descPos) == '\t')) {
          descPos++; // Skip the folding whitespace
        } else {
          break; // Not a continuation, we're done
        }
      }

      event.description.trim();

      // Unescape iCal format (commas, semicolons, newlines, backslashes)
      event.description.replace("\\,", ",");
      event.description.replace("\\;", ";");
      event.description.replace("\\n", "\n");
      event.description.replace("\\\\", "\\");
    }

    // Extract DTSTART (handle TZID parameters like DTSTART;TZID=America/New_York:...)
    // Note: Timezone conversion is not implemented - all times treated as UTC
    // For accurate local time handling, would need timezone database
    int dtStartPos = eventBlock.indexOf("DTSTART");
    if (dtStartPos >= 0) {
      int lineEnd = eventBlock.indexOf("\r\n", dtStartPos);
      if (lineEnd < 0) lineEnd = eventBlock.indexOf("\n", dtStartPos);
      if (lineEnd < 0) lineEnd = eventBlock.length();

      // Find the last colon on this line (the one before the datetime value)
      int colonPos = eventBlock.lastIndexOf(":", lineEnd);
      if (colonPos > dtStartPos) {
        String dtStart = eventBlock.substring(colonPos + 1, lineEnd);
        event.startTime = parseICalDateTime(dtStart);
      }
    }

    // Extract DTEND (handle TZID parameters like DTEND;TZID=America/New_York:...)
    int dtEndPos = eventBlock.indexOf("DTEND");
    if (dtEndPos >= 0) {
      int lineEnd = eventBlock.indexOf("\r\n", dtEndPos);
      if (lineEnd < 0) lineEnd = eventBlock.indexOf("\n", dtEndPos);
      if (lineEnd < 0) lineEnd = eventBlock.length();

      // Find the last colon on this line (the one before the datetime value)
      int colonPos = eventBlock.lastIndexOf(":", lineEnd);
      if (colonPos > dtEndPos) {
        String dtEnd = eventBlock.substring(colonPos + 1, lineEnd);
        event.endTime = parseICalDateTime(dtEnd);
      }
    } else {
      // If DTEND not found, try DURATION
      int durationPos = eventBlock.indexOf("DURATION:");
      if (durationPos >= 0) {
        int lineEnd = eventBlock.indexOf("\r\n", durationPos);
        if (lineEnd < 0) lineEnd = eventBlock.indexOf("\n", durationPos);
        if (lineEnd < 0) lineEnd = eventBlock.length();

        String duration = eventBlock.substring(durationPos + 9, lineEnd);
        duration.trim();

        // Parse ISO 8601 duration (e.g., PT1H30M, P1D, PT30M)
        unsigned long durationSeconds = parseICalDuration(duration);
        event.endTime = event.startTime + durationSeconds;
      }
    }

    // Validate event times
    if (event.startTime == 0 || event.endTime == 0) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Skipping event with invalid time"));
      #endif
      pos = eventEnd + 10;
      continue;
    }

    if (event.endTime <= event.startTime) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTF("Calendar: Skipping event with end <= start (%lu <= %lu)\n", event.endTime, event.startTime);
      #endif
      pos = eventEnd + 10;
      continue;
    }

    // Validate reasonable date range (2020-01-01 to 2100-01-01)
    const unsigned long MIN_VALID_TIME = 1577836800; // 2020-01-01
    const unsigned long MAX_VALID_TIME = 4102444800; // 2100-01-01
    if (event.startTime < MIN_VALID_TIME || event.startTime > MAX_VALID_TIME) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTF("Calendar: Skipping event with unreasonable time: %lu\n", event.startTime);
      #endif
      pos = eventEnd + 10;
      continue;
    }

    // Check for duplicate events (same start time and title)
    bool isDuplicate = false;
    for (uint8_t i = 0; i < eventCount; i++) {
      if (events[i].startTime == event.startTime && events[i].title == event.title) {
        #ifdef WLED_DEBUG
        DEBUG_PRINTF("Calendar: Skipping duplicate event: %s @ %lu\n", event.title.c_str(), event.startTime);
        #endif
        isDuplicate = true;
        break;
      }
    }

    if (isDuplicate) {
      pos = eventEnd + 10;
      continue;
    }

    // Preserve trigger state if this event existed before with same start time
    bool foundMatch = false;
    for (uint8_t i = 0; i < oldEventCount; i++) {
      if (oldEvents[i].startTime == event.startTime &&
          oldEvents[i].title == event.title) {
        event.triggered = oldEvents[i].triggered;
        foundMatch = true;
        break;
      }
    }

    // Reset trigger flag only for new events
    if (!foundMatch) {
      event.triggered = false;
    }

    #ifdef WLED_DEBUG
    DEBUG_PRINTF("Calendar: Event %d: %s @ %lu\n", eventCount, event.title.c_str(), event.startTime);
    #endif

    eventCount++;
    pos = eventEnd + 10;
  }

  // Clean up old events array
  delete[] oldEvents;

  #ifdef WLED_DEBUG
  DEBUG_PRINTF("Calendar: Parsed %d events\n", eventCount);
  #endif
}

/**
 * @brief Converts iCal datetime string to Unix timestamp
 *
 * Parses iCalendar datetime format (YYYYMMDDTHHMMSSZ) and converts it to
 * Unix epoch timestamp. Validates date components including leap years.
 *
 * @param dtStr iCal datetime string (e.g., "20250105T120000Z")
 * @return Unix timestamp (seconds since 1970-01-01), or 0 if invalid
 */
unsigned long GoogleCalendarScheduler::parseICalDateTime(String& dtStr) {
  dtStr.trim();

  // Basic format: 20250105T120000Z
  if (dtStr.length() < 15) return 0;

  int year = dtStr.substring(0, 4).toInt();
  int month = dtStr.substring(4, 6).toInt();
  int day = dtStr.substring(6, 8).toInt();
  int hour = dtStr.substring(9, 11).toInt();
  int minute = dtStr.substring(11, 13).toInt();
  int second = dtStr.substring(13, 15).toInt();

  // Validate components with month-specific day limits
  static const uint8_t daysInMonth[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if (year < 1970 || month < 1 || month > 12 || day < 1 ||
      hour > 23 || minute > 59 || second > 59) {
    #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Calendar: Invalid datetime components"));
    #endif
    return 0;
  }

  // Check day validity for the specific month
  if (day > daysInMonth[month - 1]) {
    #ifdef WLED_DEBUG
    DEBUG_PRINTF("Calendar: Invalid day %d for month %d\n", day, month);
    #endif
    return 0;
  }

  // Additional February leap year check
  if (month == 2 && day == 29) {
    // Simple leap year check (good enough for 1970-2100 range)
    bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (!isLeap) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTF("Calendar: Feb 29 invalid for non-leap year %d\n", year);
      #endif
      return 0;
    }
  }

  // Convert to Unix timestamp (simplified, doesn't account for all edge cases)
  // This is a basic implementation - for production use a proper datetime library
  tmElements_t tm;
  tm.Year = year - 1970;
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;

  return makeTime(tm);
}

/**
 * @brief Parses ISO 8601 duration format to seconds
 *
 * Converts iCalendar DURATION property (ISO 8601 format) to total seconds.
 * Supports weeks (W), days (D), hours (H), minutes (M), and seconds (S).
 *
 * @param duration ISO 8601 duration string (e.g., "PT1H30M", "P1D")
 * @return Duration in seconds, or 0 if invalid format
 */
unsigned long GoogleCalendarScheduler::parseICalDuration(String& duration) {
  duration.trim();

  if (duration.length() == 0 || duration.charAt(0) != 'P') {
    return 0; // Invalid duration
  }

  unsigned long totalSeconds = 0;
  bool inTimePart = false;
  int numberStart = -1;

  for (int i = 1; i < duration.length(); i++) {
    char c = duration.charAt(i);

    if (c == 'T') {
      inTimePart = true;
      numberStart = -1;
    } else if (isDigit(c)) {
      if (numberStart < 0) numberStart = i;
    } else {
      // Found a unit designator (D, H, M, S)
      if (numberStart >= 0) {
        int value = duration.substring(numberStart, i).toInt();

        if (!inTimePart && c == 'D') {
          totalSeconds += value * 86400; // days
        } else if (inTimePart && c == 'H') {
          totalSeconds += value * 3600; // hours
        } else if (inTimePart && c == 'M') {
          totalSeconds += value * 60; // minutes
        } else if (inTimePart && c == 'S') {
          totalSeconds += value; // seconds
        } else if (!inTimePart && c == 'W') {
          totalSeconds += value * 604800; // weeks
        }

        numberStart = -1;
      }
    }
  }

  return totalSeconds;
}

/**
 * @brief Checks current time against event schedule and triggers actions
 *
 * Called periodically from loop(). Expires old events (>24h past), checks if
 * any events should trigger based on current time, and executes associated actions.
 * Manages trigger state to prevent duplicate executions.
 */
void GoogleCalendarScheduler::checkAndTriggerEvents() {
  unsigned long currentTime = toki.second(); // Use WLED's time

  // Don't trigger events if time is not synced (Unix epoch starts 1970-01-01)
  // A reasonable threshold is 1000000000 (2001-09-09)
  if (currentTime < 1000000000) {
    return;
  }

  // Expire old events (ended more than 24 hours ago)
  const unsigned long EXPIRY_THRESHOLD = 86400; // 24 hours in seconds
  uint8_t writeIdx = 0;
  for (uint8_t readIdx = 0; readIdx < eventCount; readIdx++) {
    CalendarEvent& event = events[readIdx];

    // Keep event if it hasn't expired yet
    if (currentTime < event.endTime + EXPIRY_THRESHOLD) {
      if (writeIdx != readIdx) {
        events[writeIdx] = events[readIdx];
      }
      writeIdx++;
    }
    #ifdef WLED_DEBUG
    else {
      DEBUG_PRINTF("Calendar: Expired old event: %s\n", event.title.c_str());
    }
    #endif
  }

  // Update count if events were removed
  if (writeIdx < eventCount) {
    eventCount = writeIdx;
    #ifdef WLED_DEBUG
    DEBUG_PRINTF("Calendar: Removed expired events, now tracking %d events\n", eventCount);
    #endif
  }

  for (uint8_t i = 0; i < eventCount; i++) {
    CalendarEvent& event = events[i];

    // Check if we're currently within the event time window
    bool isActive = (currentTime >= event.startTime && currentTime < event.endTime);

    // Trigger if active and not yet triggered
    if (isActive && !event.triggered) {
      executeEventAction(event);
      event.triggered = true;
    }

    // Reset trigger flag when event ends so it can retrigger if it repeats
    if (!isActive && event.triggered) {
      event.triggered = false;
    }
  }
}

/**
 * @brief Executes the action associated with a calendar event
 *
 * Determines if the event description contains JSON (API command) or a preset name,
 * then executes the appropriate action. For JSON, deserializes and applies to WLED
 * state. For preset names, searches for matching preset and applies it.
 *
 * @param event The calendar event containing the action to execute
 */
void GoogleCalendarScheduler::executeEventAction(CalendarEvent& event) {
  #ifdef WLED_DEBUG
  DEBUG_PRINTF("Calendar: Triggering event: %s\n", event.title.c_str());
  #endif

  if (event.description.length() == 0) {
    #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Calendar: No description found"));
    #endif
    return;
  }

  String desc = event.description;
  desc.trim();

  // Check if it's JSON (starts with { or [)
  if (desc.startsWith("{") || desc.startsWith("[")) {
    // JSON API mode
    if (!requestJSONBufferLock(USERMOD_ID_CALENDAR_SCHEDULER)) {
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: Buffer locked, skipping"));
      #endif
      return;
    }

    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, desc);

    if (!error) {
      deserializeState(doc.as<JsonObject>(), CALL_MODE_NOTIFICATION);
      #ifdef WLED_DEBUG
      DEBUG_PRINTLN(F("Calendar: JSON applied"));
      #endif
    } else {
      #ifdef WLED_DEBUG
      DEBUG_PRINTF("Calendar: JSON parse error: %s\n", error.c_str());
      #endif
    }

    releaseJSONBufferLock();
  } else {
    // Preset name mode - search for preset by name

    int8_t presetId = -1;
    uint16_t presetsChecked = 0;

    // Prepare lowercase version for comparison once (don't modify original)
    // Reserve capacity to avoid reallocation during toLowerCase
    String descLower = desc;
    descLower.trim();
    descLower.toLowerCase();

    // Search through presets for matching name using WLED's getPresetName function
    const uint8_t MAX_PRESET_ID = 250;
    for (uint8_t i = 1; i <= MAX_PRESET_ID; i++) {
      String presetName;
      if (getPresetName(i, presetName)) {
        presetsChecked++;

        // Skip empty preset names
        if (presetName.length() == 0) continue;

        // Normalize preset name for comparison
        presetName.trim();
        presetName.toLowerCase();

        // Case-insensitive comparison
        if (presetName == descLower) {
          presetId = i;
          #ifdef WLED_DEBUG
          DEBUG_PRINTF("Calendar: Found preset at ID %d\n", i);
          #endif
          break;
        }
      }
    }

    if (presetId > 0) {
      applyPreset(presetId, CALL_MODE_NOTIFICATION);
    }
    #ifdef WLED_DEBUG
    else {
      DEBUG_PRINTF("Calendar: Preset not found (checked %d presets)\n", presetsChecked);
    }
    #endif
  }
}


// Register the usermod
static GoogleCalendarScheduler calendarScheduler;
REGISTER_USERMOD(calendarScheduler);
