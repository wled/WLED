#include "wled.h"
#include <WiFiClientSecure.h>

/*
 * Google Calendar Scheduler Usermod
 *
 * Triggers WLED presets, macros, or API calls based on Google Calendar events
 *
 * Features:
 * - Fetches calendar events from Google Calendar (via public iCal URL)
 * - Matches event titles/descriptions to configured actions
 * - Executes presets, macros, or API calls at event start/end times
 * - Configurable poll interval and event mappings
 */

// Forward declarations
class GoogleCalendarScheduler : public Usermod {
  private:
    // Configuration variables
    bool enabled = false;
    bool initDone = false;

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

    // Event tracking
    struct CalendarEvent {
      String title;
      String description;
      unsigned long startTime;    // Unix timestamp
      unsigned long endTime;      // Unix timestamp
      bool triggered = false;     // Has start action been triggered?
      bool endTriggered = false;  // Has end action been triggered?
    };

    static const uint8_t MAX_EVENTS = 5;
    CalendarEvent events[MAX_EVENTS];
    uint8_t eventCount = 0;

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

    // Helper methods
    void parseCalendarUrl();
    bool fetchCalendarEvents();
    void parseICalData(String& icalData);
    unsigned long parseICalDateTime(String& dtStr);
    void checkAndTriggerEvents();
    void executeEventAction(CalendarEvent& event);

  public:
    void setup() override {
      initDone = true;
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

      // Poll calendar at configured interval
      if (!isFetching && calendarUrl.length() > 0 && (now - lastPollTime > pollInterval)) {
        lastPollTime = now;
        fetchCalendarEvents();
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
    }

    void readFromJsonState(JsonObject& root) override {
      if (!initDone) return;

      JsonObject usermod = root[FPSTR(_name)];
      if (!usermod.isNull()) {
        if (usermod.containsKey("enabled")) {
          enabled = usermod["enabled"];
        }
        if (usermod.containsKey("pollNow") && usermod["pollNow"]) {
          DEBUG_PRINTLN(F("Calendar: Manual poll requested"));
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
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);
      configComplete &= getJsonValue(top[FPSTR(_calendarUrl)], calendarUrl, "");

      int pollIntervalSec = pollInterval / 1000;
      configComplete &= getJsonValue(top[FPSTR(_pollInterval)], pollIntervalSec, 300);
      pollInterval = pollIntervalSec * 1000;

      if (calendarUrl.length() > 0) {
        parseCalendarUrl();
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

      // Show active events
      if (enabled && initDone && eventCount > 0) {
        unsigned long currentTime = 0;
        bool timeValid = false;

        // Only get current time if NTP is synced
        if (toki.isSynced()) {
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

// Parse the calendar URL into host and path
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

  DEBUG_PRINT(F("Calendar: Parsed URL - Host: "));
  DEBUG_PRINT(httpHost);
  DEBUG_PRINT(F(", Path: "));
  DEBUG_PRINT(httpPath);
  DEBUG_PRINT(F(", HTTPS: "));
  DEBUG_PRINTLN(useHTTPS);
}

// Fetch calendar events using WiFiClientSecure
bool GoogleCalendarScheduler::fetchCalendarEvents() {
  if (httpHost.length() == 0 || isFetching) {
    return false;
  }

  isFetching = true;

  DEBUG_PRINT(F("Calendar: Connecting to "));
  DEBUG_PRINT(httpHost);
  DEBUG_PRINT(F(":"));
  DEBUG_PRINTLN(useHTTPS ? HTTPS_PORT : HTTP_PORT);

  WiFiClient *client;
  if (useHTTPS) {
    WiFiClientSecure *secureClient = new WiFiClientSecure();
    secureClient->setInsecure(); // Skip certificate validation
    secureClient->setTimeout(HTTP_TIMEOUT_MS);
    client = secureClient;
  } else {
    client = new WiFiClient();
    client->setTimeout(HTTP_TIMEOUT_MS);
  }

  if (!client->connect(httpHost.c_str(), useHTTPS ? HTTPS_PORT : HTTP_PORT)) {
    DEBUG_PRINTLN(F("Calendar: Connection failed"));
    delete client;
    isFetching = false;
    return false;
  }

  DEBUG_PRINTLN(F("Calendar: Connected, sending request"));

  // Send HTTP request
  client->print(String("GET ") + httpPath + " HTTP/1.1\r\n" +
                "Host: " + httpHost + "\r\n" +
                "Connection: close\r\n" +
                "User-Agent: WLED-Calendar-Scheduler\r\n\r\n");

  DEBUG_PRINTLN(F("Calendar: Request sent"));

  // Read response with buffered approach
  String responseBuffer = "";
  responseBuffer.reserve(RESPONSE_RESERVE_SIZE);

  char buffer[READ_BUFFER_SIZE];
  unsigned long timeout = millis();

  while (client->connected() && (millis() - timeout < HTTP_TIMEOUT_MS)) {
    int available = client->available();
    if (available > 0) {
      int toRead = min(available, (int)sizeof(buffer) - 1);
      int bytesRead = client->readBytes(buffer, toRead);

      if (bytesRead > 0) {
        buffer[bytesRead] = '\0';

        // Check size limit before appending
        if (responseBuffer.length() + bytesRead > MAX_RESPONSE_SIZE) {
          DEBUG_PRINTLN(F("Calendar: Response too large, truncating"));
          break;
        }

        responseBuffer += buffer;
        timeout = millis();
      }
    }
  }

  client->stop();
  delete client;

  DEBUG_PRINT(F("Calendar: Received "));
  DEBUG_PRINT(responseBuffer.length());
  DEBUG_PRINTLN(F(" bytes"));

  // Find the body (after headers)
  int bodyPos = responseBuffer.indexOf("\r\n\r\n");
  if (bodyPos > 0) {
    String icalData = responseBuffer.substring(bodyPos + 4);
    DEBUG_PRINT(F("Calendar: Parsing iCal data, length: "));
    DEBUG_PRINTLN(icalData.length());

    // Debug: Print first 200 chars of iCal data
    DEBUG_PRINT(F("Calendar: First 200 chars: "));
    DEBUG_PRINTLN(icalData.substring(0, min(200, (int)icalData.length())));

    parseICalData(icalData);
  } else {
    DEBUG_PRINTLN(F("Calendar: No body found in response"));
    DEBUG_PRINT(F("Calendar: Response buffer: "));
    DEBUG_PRINTLN(responseBuffer.substring(0, min(200, (int)responseBuffer.length())));
  }

  isFetching = false;
  return true;
}

// Simple iCal parser - extracts VEVENT blocks
void GoogleCalendarScheduler::parseICalData(String& icalData) {
  // Store old events to preserve trigger state
  CalendarEvent oldEvents[MAX_EVENTS];
  uint8_t oldEventCount = eventCount;
  for (uint8_t i = 0; i < oldEventCount; i++) {
    oldEvents[i] = events[i];
  }

  eventCount = 0;

  int pos = 0;
  while (pos < icalData.length() && eventCount < MAX_EVENTS) {
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
    }

    // Preserve trigger state if this event existed before with same start time
    bool foundMatch = false;
    for (uint8_t i = 0; i < oldEventCount; i++) {
      if (oldEvents[i].startTime == event.startTime &&
          oldEvents[i].title == event.title) {
        event.triggered = oldEvents[i].triggered;
        event.endTriggered = oldEvents[i].endTriggered;
        foundMatch = true;
        break;
      }
    }

    // Reset trigger flags only for new events
    if (!foundMatch) {
      event.triggered = false;
      event.endTriggered = false;
    }

    DEBUG_PRINT(F("Calendar: Event "));
    DEBUG_PRINT(eventCount);
    DEBUG_PRINT(F(": "));
    DEBUG_PRINT(event.title);
    DEBUG_PRINT(F(" @ "));
    DEBUG_PRINTLN(event.startTime);

    eventCount++;
    pos = eventEnd + 10;
  }

  DEBUG_PRINT(F("Calendar: Parsed "));
  DEBUG_PRINT(eventCount);
  DEBUG_PRINTLN(F(" events"));
}

// Parse iCal datetime format (YYYYMMDDTHHMMSSZ) to Unix timestamp
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

  // Validate components
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour > 23 || minute > 59 || second > 59) {
    DEBUG_PRINTLN(F("Calendar: Invalid datetime components"));
    return 0;
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

void GoogleCalendarScheduler::checkAndTriggerEvents() {
  unsigned long currentTime = toki.second(); // Use WLED's time

  // Don't trigger events if time is not synced (Unix epoch starts 1970-01-01)
  // A reasonable threshold is 1000000000 (2001-09-09)
  if (currentTime < 1000000000) {
    return;
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

void GoogleCalendarScheduler::executeEventAction(CalendarEvent& event) {
  DEBUG_PRINT(F("Calendar: Triggering event: "));
  DEBUG_PRINTLN(event.title);

  if (event.description.length() == 0) {
    DEBUG_PRINTLN(F("Calendar: No description found"));
    return;
  }

  DEBUG_PRINT(F("Calendar: Description: "));
  DEBUG_PRINTLN(event.description);

  String desc = event.description;
  desc.trim();

  // Check if it's JSON (starts with { or [)
  if (desc.startsWith("{") || desc.startsWith("[")) {
    // JSON API mode
    if (!requestJSONBufferLock(USERMOD_ID_CALENDAR_SCHEDULER)) {
      DEBUG_PRINTLN(F("Calendar: Buffer locked, skipping"));
      return;
    }

    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, desc);

    if (!error) {
      deserializeState(doc.as<JsonObject>(), CALL_MODE_NOTIFICATION);
      DEBUG_PRINTLN(F("Calendar: JSON applied"));
    } else {
      DEBUG_PRINT(F("Calendar: JSON parse error: "));
      DEBUG_PRINTLN(error.c_str());
    }

    releaseJSONBufferLock();
  } else {
    // Preset name mode - search for preset by name
    DEBUG_PRINT(F("Calendar: Looking for preset: '"));
    DEBUG_PRINT(desc);
    DEBUG_PRINT(F("' (length: "));
    DEBUG_PRINT(desc.length());
    DEBUG_PRINTLN(F(")"));

    int8_t presetId = -1;
    uint16_t presetsChecked = 0;

    // Prepare lowercase version for comparison
    String descLower = desc;
    descLower.toLowerCase();

    // Search through presets for matching name using WLED's getPresetName function
    for (uint8_t i = 1; i < 251; i++) {
      String presetName;
      if (getPresetName(i, presetName)) {
        presetsChecked++;
        presetName.trim();

        // Case-insensitive comparison
        String presetNameLower = presetName;
        presetNameLower.toLowerCase();

        if (presetNameLower == descLower) {
          presetId = i;
          DEBUG_PRINT(F("Calendar: Found preset '"));
          DEBUG_PRINT(presetName);
          DEBUG_PRINT(F("' at ID "));
          DEBUG_PRINTLN(i);
          break;
        }
      }
    }

    if (presetId > 0) {
      DEBUG_PRINT(F("Calendar: Applying preset "));
      DEBUG_PRINTLN(presetId);
      applyPreset(presetId, CALL_MODE_NOTIFICATION);
    } else {
      DEBUG_PRINT(F("Calendar: Preset not found (checked "));
      DEBUG_PRINT(presetsChecked);
      DEBUG_PRINTLN(F(" presets)"));
    }
  }
}


// Register the usermod
static GoogleCalendarScheduler calendarScheduler;
REGISTER_USERMOD(calendarScheduler);
