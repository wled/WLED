// schedule.cpp
// Handles reading, parsing, and checking the preset schedule from schedule.json

#include "schedule.h"
#include "wled.h"
#include <time.h>

#define SCHEDULE_FILE "/schedule.json"

// Array to hold scheduled events, max size defined in schedule.h
ScheduleEvent scheduleEvents[MAX_SCHEDULE_EVENTS];
uint8_t numScheduleEvents = 0; // Current count of loaded schedule entries

// Helper function to check if current date (cm, cd) is within the event's start and end date range
bool isTodayInRange(uint8_t sm, uint8_t sd, uint8_t em, uint8_t ed, uint8_t cm, uint8_t cd)
{
    // Handles ranges that wrap over the year end, e.g., Dec to Jan
    if (sm < em || (sm == em && sd <= ed))
    {
        // Normal range within a year
        return (cm > sm || (cm == sm && cd >= sd)) &&
               (cm < em || (cm == em && cd <= ed));
    }
    else
    {
        // Range wraps year-end (e.g., Nov 20 to Feb 10)
        return (cm > sm || (cm == sm && cd >= sd)) ||
               (cm < em || (cm == em && cd <= ed));
    }
}

// Checks current time against schedule entries and applies matching presets
void checkSchedule() {
    static int lastMinute = -1; // To avoid multiple triggers within the same minute

    time_t now = localTime;
    if (now < 100000) return; // Invalid or uninitialized time guard

    struct tm* timeinfo = localtime(&now);

    int thisMinute = timeinfo->tm_min + timeinfo->tm_hour * 60;
    if (thisMinute == lastMinute) return; // Already checked this minute
    lastMinute = thisMinute;

    // Extract date/time components for easier use
    uint8_t cm = timeinfo->tm_mon + 1; // Month [1-12]
    uint8_t cd = timeinfo->tm_mday;    // Day of month [1-31]
    uint8_t wday = timeinfo->tm_wday;  // Weekday [0-6], Sunday=0
    uint8_t hr = timeinfo->tm_hour;    // Hour [0-23]
    uint8_t min = timeinfo->tm_min;    // Minute [0-59]

    DEBUG_PRINTF_P(PSTR("[Schedule] Checking schedule at %02u:%02u\n"), hr, min);

    // Iterate through all scheduled events
    for (uint8_t i = 0; i < numScheduleEvents; i++)
    {
        const ScheduleEvent &e = scheduleEvents[i];

        // Skip if hour or minute doesn't match current time
        if (e.hour != hr || e.minute != min)
            continue;

        bool match = false;

        // Check if repeat mask matches current weekday (bitmask with Sunday=LSB)
        if (e.repeatMask && ((e.repeatMask >> wday) & 0x01))
            match = true;

        // Otherwise check if current date is within start and end date range
        if (e.startMonth)
        {
            if (isTodayInRange(e.startMonth, e.startDay, e.endMonth, e.endDay, cm, cd))
                match = true;
        }

        // If match, apply preset and print debug
        if (match)
        {
            applyPreset(e.presetId);
            DEBUG_PRINTF_P(PSTR("[Schedule] Applying preset %u at %02u:%02u\n"), e.presetId, hr, min);
        }
    }
}

// Loads schedule events from the schedule JSON file
// Returns true if successful, false on error or missing file
bool loadSchedule() {
    if (!WLED_FS.exists(SCHEDULE_FILE)) return false;

    // Acquire JSON buffer lock to prevent concurrent access
    if (!requestJSONBufferLock(7)) return false;

    File file = WLED_FS.open(SCHEDULE_FILE, "r");
    if (!file) {
        releaseJSONBufferLock();
        return false;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();  // Always close file before releasing lock

    if (error) {
        DEBUG_PRINTF_P(PSTR("[Schedule] JSON parse failed: %s\n"), error.c_str());
        releaseJSONBufferLock();
        return false;
    }

    numScheduleEvents = 0;
    for (JsonObject e : doc.as<JsonArray>()) {
        if (numScheduleEvents >= MAX_SCHEDULE_EVENTS) break;

        // Read and validate fields with type safety
        int sm = e["sm"].as<int>();
        int sd = e["sd"].as<int>();
        int em = e["em"].as<int>();
        int ed = e["ed"].as<int>();
        int r  = e["r"].as<int>();
        int h  = e["h"].as<int>();
        int m  = e["m"].as<int>();
        int p  = e["p"].as<int>();

        // Validate ranges to prevent bad data
        if (sm < 1 || sm > 12 || em < 1 || em > 12 ||
            sd < 1 || sd > 31 || ed < 1 || ed > 31 ||
            h  < 0 || h  > 23 || m  < 0 || m  > 59 ||
            r  < 0 || r  > 127|| p  < 1 || p  > 250) {
            DEBUG_PRINTF_P(PSTR("[Schedule] Invalid values in event %u, skipping\n"), numScheduleEvents);
            continue;
        }

        // Store event in the array
        scheduleEvents[numScheduleEvents++] = {
            (uint8_t)sm, (uint8_t)sd,
            (uint8_t)em, (uint8_t)ed,
            (uint8_t)r,  (uint8_t)h,
            (uint8_t)m,  (uint8_t)p
        };
    }

    DEBUG_PRINTF_P(PSTR("[Schedule] Loaded %u schedule entries from schedule.json\n"), numScheduleEvents);

    // Release JSON buffer lock after finishing
    releaseJSONBufferLock();

    return true;
}
