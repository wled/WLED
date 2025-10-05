#include "wled.h"

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

#define USERMOD_ID_CALENDAR_SCHEDULER 59

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
    AsyncClient *httpClient = nullptr;
    String httpHost = "";
    String httpPath = "";
    String responseBuffer = "";
    bool isFetching = false;
    unsigned long lastActivityTime = 0;
    static const unsigned long INACTIVITY_TIMEOUT = 30000; // 30 seconds
    static const uint16_t ACK_TIMEOUT = 9000;
    static const uint16_t RX_TIMEOUT = 9000;

    // Event tracking
    struct CalendarEvent {
      String title;
      String description;
      unsigned long startTime;    // Unix timestamp
      unsigned long endTime;      // Unix timestamp
      uint8_t presetId;           // Preset to trigger (0 = none)
      String apiCall;             // Custom API call string
      bool triggered = false;     // Has start action been triggered?
      bool endTriggered = false;  // Has end action been triggered?
    };

    static const uint8_t MAX_EVENTS = 10;
    CalendarEvent events[MAX_EVENTS];
    uint8_t eventCount = 0;

    // Event mapping configuration
    struct EventMapping {
      String eventPattern;    // Pattern to match in event title/description
      uint8_t startPreset;    // Preset to trigger at event start
      uint8_t endPreset;      // Preset to trigger at event end
      String startApiCall;    // API call at event start
      String endApiCall;      // API call at event end
    };

    static const uint8_t MAX_MAPPINGS = 5;
    EventMapping mappings[MAX_MAPPINGS];
    uint8_t mappingCount = 1; // Start with 1 for default mapping

    // String constants for config
    static const char _name[];
    static const char _enabled[];
    static const char _calendarUrl[];
    static const char _pollInterval[];

    // Helper methods
    void parseCalendarUrl();
    bool fetchCalendarEvents();
    void onHttpConnect(AsyncClient *c);
    void onHttpData(void *data, size_t len);
    void onHttpDisconnect();
    void parseICalData(String& icalData);
    unsigned long parseICalDateTime(String& dtStr);
    void checkAndTriggerEvents();
    void executeEventAction(CalendarEvent& event, bool isStart);
    void applyEventMapping(CalendarEvent& event);
    bool matchesPattern(const String& text, const String& pattern);
    void cleanupHttpClient();

  public:
    void setup() override {
      // Initialize with a default mapping
      mappings[0].eventPattern = "WLED";
      mappings[0].startPreset = 1;
      mappings[0].endPreset = 0;
      initDone = true;
    }

    void connected() override {
      if (enabled && WLED_CONNECTED && calendarUrl.length() > 0) {
        // Fetch calendar events on WiFi connection
        parseCalendarUrl();
        fetchCalendarEvents();
      }
    }

    void loop() override {
      if (!enabled || !initDone || !WLED_CONNECTED) return;

      unsigned long now = millis();

      // Check for HTTP client inactivity timeout
      if (httpClient != nullptr && (now - lastActivityTime > INACTIVITY_TIMEOUT)) {
        DEBUG_PRINTLN(F("Calendar: HTTP client inactivity timeout"));
        cleanupHttpClient();
        isFetching = false;
      }

      // Poll calendar at configured interval
      if (!isFetching && calendarUrl.length() > 0 && (now - lastPollTime > pollInterval)) {
        lastPollTime = now;
        fetchCalendarEvents();
      }

      // Check for events that should trigger
      checkAndTriggerEvents();
    }

    void addToJsonInfo(JsonObject& root) override {
      if (!enabled) return;

      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray calInfo = user.createNestedArray(FPSTR(_name));
      calInfo.add(eventCount);
      calInfo.add(F(" events"));
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
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_calendarUrl)] = calendarUrl;
      top[FPSTR(_pollInterval)] = pollInterval / 1000; // Store in seconds

      // Save event mappings
      JsonArray mappingsArr = top.createNestedArray("mappings");
      for (uint8_t i = 0; i < mappingCount; i++) {
        JsonObject mapping = mappingsArr.createNestedObject();
        mapping["pattern"] = mappings[i].eventPattern;
        mapping["startPreset"] = mappings[i].startPreset;
        mapping["endPreset"] = mappings[i].endPreset;
        mapping["startApi"] = mappings[i].startApiCall;
        mapping["endApi"] = mappings[i].endApiCall;
      }
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);
      configComplete &= getJsonValue(top[FPSTR(_calendarUrl)], calendarUrl, "");

      int pollIntervalSec = pollInterval / 1000;
      configComplete &= getJsonValue(top[FPSTR(_pollInterval)], pollIntervalSec, 300);
      pollInterval = pollIntervalSec * 1000;

      // Load event mappings
      if (top.containsKey("mappings")) {
        JsonArray mappingsArr = top["mappings"];
        mappingCount = min((uint8_t)mappingsArr.size(), MAX_MAPPINGS);
        for (uint8_t i = 0; i < mappingCount; i++) {
          JsonObject mapping = mappingsArr[i];
          getJsonValue(mapping["pattern"], mappings[i].eventPattern, "");
          getJsonValue(mapping["startPreset"], mappings[i].startPreset, (uint8_t)0);
          getJsonValue(mapping["endPreset"], mappings[i].endPreset, (uint8_t)0);
          getJsonValue(mapping["startApi"], mappings[i].startApiCall, "");
          getJsonValue(mapping["endApi"], mappings[i].endApiCall, "");
        }
      }

      if (calendarUrl.length() > 0) {
        parseCalendarUrl();
      }

      return configComplete;
    }

    void appendConfigData() override {
      oappend(F("addInfo('"));
      oappend(String(FPSTR(_name)).c_str());
      oappend(F(":calendarUrl',1,'<i>Public iCal URL from Google Calendar (HTTP only)</i>');"));

      oappend(F("addInfo('"));
      oappend(String(FPSTR(_name)).c_str());
      oappend(F(":pollInterval',1,'<i>How often to check calendar (seconds, min 60)</i>');"));
    }

    uint16_t getId() override {
      return USERMOD_ID_CALENDAR_SCHEDULER;
    }

    ~GoogleCalendarScheduler() {
      cleanupHttpClient();
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
  DEBUG_PRINTLN(httpPath);
}

void GoogleCalendarScheduler::cleanupHttpClient() {
  if (httpClient != nullptr) {
    httpClient->onDisconnect(nullptr);
    httpClient->onError(nullptr);
    httpClient->onTimeout(nullptr);
    httpClient->onData(nullptr);
    httpClient->onConnect(nullptr);
    delete httpClient;
    httpClient = nullptr;
  }
}

// Fetch calendar events using AsyncClient
bool GoogleCalendarScheduler::fetchCalendarEvents() {
  if (httpHost.length() == 0 || isFetching) {
    return false;
  }

  // Cleanup any existing client
  if (httpClient != nullptr) {
    cleanupHttpClient();
  }

  DEBUG_PRINTLN(F("Calendar: Creating HTTP client"));
  httpClient = new AsyncClient();

  if (httpClient == nullptr) {
    DEBUG_PRINTLN(F("Calendar: Failed to create HTTP client"));
    return false;
  }

  isFetching = true;
  responseBuffer = "";
  lastActivityTime = millis();

  // Set up callbacks
  httpClient->onConnect([](void *arg, AsyncClient *c) {
    GoogleCalendarScheduler *instance = (GoogleCalendarScheduler *)arg;
    instance->onHttpConnect(c);
  }, this);

  httpClient->onData([](void *arg, AsyncClient *c, void *data, size_t len) {
    GoogleCalendarScheduler *instance = (GoogleCalendarScheduler *)arg;
    instance->onHttpData(data, len);
  }, this);

  httpClient->onDisconnect([](void *arg, AsyncClient *c) {
    GoogleCalendarScheduler *instance = (GoogleCalendarScheduler *)arg;
    instance->onHttpDisconnect();
  }, this);

  httpClient->onError([](void *arg, AsyncClient *c, int8_t error) {
    DEBUG_PRINT(F("Calendar: HTTP error: "));
    DEBUG_PRINTLN(error);
    GoogleCalendarScheduler *instance = (GoogleCalendarScheduler *)arg;
    instance->cleanupHttpClient();
    instance->isFetching = false;
  }, this);

  httpClient->onTimeout([](void *arg, AsyncClient *c, uint32_t time) {
    DEBUG_PRINTLN(F("Calendar: HTTP timeout"));
    GoogleCalendarScheduler *instance = (GoogleCalendarScheduler *)arg;
    instance->cleanupHttpClient();
    instance->isFetching = false;
  }, this);

  httpClient->setAckTimeout(ACK_TIMEOUT);
  httpClient->setRxTimeout(RX_TIMEOUT);

  DEBUG_PRINT(F("Calendar: Connecting to "));
  DEBUG_PRINT(httpHost);
  DEBUG_PRINTLN(F(":80"));

  if (!httpClient->connect(httpHost.c_str(), 80)) {
    DEBUG_PRINTLN(F("Calendar: Connection failed"));
    cleanupHttpClient();
    isFetching = false;
    return false;
  }

  return true;
}

void GoogleCalendarScheduler::onHttpConnect(AsyncClient *c) {
  DEBUG_PRINTLN(F("Calendar: Connected, sending request"));
  lastActivityTime = millis();

  String request = "GET " + httpPath + " HTTP/1.1\r\n" +
                   "Host: " + httpHost + "\r\n" +
                   "Connection: close\r\n" +
                   "User-Agent: WLED-Calendar-Scheduler\r\n\r\n";

  c->write(request.c_str());
  DEBUG_PRINTLN(F("Calendar: Request sent"));
}

void GoogleCalendarScheduler::onHttpData(void *data, size_t len) {
  lastActivityTime = millis();

  char *strData = new char[len + 1];
  memcpy(strData, data, len);
  strData[len] = '\0';
  responseBuffer += String(strData);
  delete[] strData;

  DEBUG_PRINT(F("Calendar: Received "));
  DEBUG_PRINT(len);
  DEBUG_PRINTLN(F(" bytes"));
}

void GoogleCalendarScheduler::onHttpDisconnect() {
  DEBUG_PRINTLN(F("Calendar: Disconnected"));
  isFetching = false;

  // Find the body (after headers)
  int bodyPos = responseBuffer.indexOf("\r\n\r\n");
  if (bodyPos > 0) {
    String icalData = responseBuffer.substring(bodyPos + 4);
    DEBUG_PRINT(F("Calendar: Parsing iCal data, length: "));
    DEBUG_PRINTLN(icalData.length());
    parseICalData(icalData);
  }

  cleanupHttpClient();
}

// Simple iCal parser - extracts VEVENT blocks
void GoogleCalendarScheduler::parseICalData(String& icalData) {
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

    // Extract DESCRIPTION
    int descStart = eventBlock.indexOf("DESCRIPTION:");
    if (descStart >= 0) {
      int descEnd = eventBlock.indexOf("\r\n", descStart);
      if (descEnd < 0) descEnd = eventBlock.indexOf("\n", descStart);
      event.description = eventBlock.substring(descStart + 12, descEnd);
      event.description.trim();
    }

    // Extract DTSTART
    int dtStartPos = eventBlock.indexOf("DTSTART");
    if (dtStartPos >= 0) {
      int colonPos = eventBlock.indexOf(":", dtStartPos);
      int endPos = eventBlock.indexOf("\r\n", colonPos);
      if (endPos < 0) endPos = eventBlock.indexOf("\n", colonPos);
      String dtStart = eventBlock.substring(colonPos + 1, endPos);
      event.startTime = parseICalDateTime(dtStart);
    }

    // Extract DTEND
    int dtEndPos = eventBlock.indexOf("DTEND");
    if (dtEndPos >= 0) {
      int colonPos = eventBlock.indexOf(":", dtEndPos);
      int endPos = eventBlock.indexOf("\r\n", colonPos);
      if (endPos < 0) endPos = eventBlock.indexOf("\n", colonPos);
      String dtEnd = eventBlock.substring(colonPos + 1, endPos);
      event.endTime = parseICalDateTime(dtEnd);
    }

    // Reset trigger flags
    event.triggered = false;
    event.endTriggered = false;

    // Apply event mapping
    applyEventMapping(event);

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

  for (uint8_t i = 0; i < eventCount; i++) {
    CalendarEvent& event = events[i];

    // Check if event should start
    if (!event.triggered && currentTime >= event.startTime && currentTime < event.endTime) {
      executeEventAction(event, true);
      event.triggered = true;
    }

    // Check if event should end
    if (event.triggered && !event.endTriggered && currentTime >= event.endTime) {
      executeEventAction(event, false);
      event.endTriggered = true;
    }
  }
}

void GoogleCalendarScheduler::executeEventAction(CalendarEvent& event, bool isStart) {
  DEBUG_PRINT(F("Calendar: Executing "));
  DEBUG_PRINT(isStart ? F("start") : F("end"));
  DEBUG_PRINT(F(" action for: "));
  DEBUG_PRINTLN(event.title);

  if (isStart) {
    // Execute start action
    if (event.presetId > 0) {
      DEBUG_PRINT(F("Calendar: Applying preset "));
      DEBUG_PRINTLN(event.presetId);
      applyPreset(event.presetId, CALL_MODE_NOTIFICATION);
    }

    // Execute API call if configured
    if (event.apiCall.length() > 0) {
      DEBUG_PRINT(F("Calendar: Executing API call: "));
      DEBUG_PRINTLN(event.apiCall);
      handleSet(nullptr, event.apiCall, true);
    }
  } else {
    // Execute end action - could use endPreset from mapping
    // Find the mapping for this event to get endPreset
    for (uint8_t i = 0; i < mappingCount; i++) {
      if (matchesPattern(event.title, mappings[i].eventPattern) ||
          matchesPattern(event.description, mappings[i].eventPattern)) {
        if (mappings[i].endPreset > 0) {
          DEBUG_PRINT(F("Calendar: Applying end preset "));
          DEBUG_PRINTLN(mappings[i].endPreset);
          applyPreset(mappings[i].endPreset, CALL_MODE_NOTIFICATION);
        }
        if (mappings[i].endApiCall.length() > 0) {
          DEBUG_PRINT(F("Calendar: Executing end API call: "));
          DEBUG_PRINTLN(mappings[i].endApiCall);
          handleSet(nullptr, mappings[i].endApiCall, true);
        }
        break;
      }
    }
  }
}

void GoogleCalendarScheduler::applyEventMapping(CalendarEvent& event) {
  for (uint8_t i = 0; i < mappingCount; i++) {
    if (matchesPattern(event.title, mappings[i].eventPattern) ||
        matchesPattern(event.description, mappings[i].eventPattern)) {
      event.presetId = mappings[i].startPreset;
      event.apiCall = mappings[i].startApiCall;
      DEBUG_PRINT(F("Calendar: Matched pattern '"));
      DEBUG_PRINT(mappings[i].eventPattern);
      DEBUG_PRINT(F("' -> Preset "));
      DEBUG_PRINTLN(event.presetId);
      break;
    }
  }
}

bool GoogleCalendarScheduler::matchesPattern(const String& text, const String& pattern) {
  // Case-insensitive substring match
  String textLower = text;
  String patternLower = pattern;
  textLower.toLowerCase();
  patternLower.toLowerCase();
  return textLower.indexOf(patternLower) >= 0;
}

// Register the usermod
static GoogleCalendarScheduler calendarScheduler;
REGISTER_USERMOD(calendarScheduler);
