// schedule.cpp

//TODO: make schedule.json trigger loadshedule(); on upload istead of just once a min (line 50)

#include "schedule.h"
#include <WLED.h>
#include <time.h>

#define SCHEDULE_FILE "/schedule.json"

ScheduleEvent scheduleEvents[MAX_SCHEDULE_EVENTS];
uint8_t numScheduleEvents = 0;

bool isTodayInRange(uint8_t sm, uint8_t sd, uint8_t em, uint8_t ed, uint8_t cm, uint8_t cd)
{
    if (sm < em || (sm == em && sd <= ed))
    {
        return (cm > sm || (cm == sm && cd >= sd)) &&
               (cm < em || (cm == em && cd <= ed));
    }
    else
    {
        return (cm > sm || (cm == sm && cd >= sd)) ||
               (cm < em || (cm == em && cd <= ed));
    }
}


// Checks the schedule and applies any events that match the current time and date.

void checkSchedule() {
    static int lastMinute = -1;
  
    time_t now = localTime;
    if (now < 100000) return;
  
    struct tm* timeinfo = localtime(&now);
    int thisMinute = timeinfo->tm_min + timeinfo->tm_hour * 60;
  
    if (thisMinute == lastMinute) return;
    lastMinute = thisMinute;
  

    uint8_t cm = timeinfo->tm_mon + 1; // months since Jan (0-11)
    uint8_t cd = timeinfo->tm_mday;
    uint8_t wday = timeinfo->tm_wday; // days since Sunday (0-6)
    uint8_t hr = timeinfo->tm_hour;
    uint8_t min = timeinfo->tm_min;

    loadSchedule();
    DEBUG_PRINTF_P(PSTR("[Schedule] Checking schedule at %02u:%02u\n"), hr, min);

    for (uint8_t i = 0; i < numScheduleEvents; i++)
    {
        const ScheduleEvent &e = scheduleEvents[i];
        if (e.hour != hr || e.minute != min)
            continue;

        bool match = false;
        if (e.repeatMask && ((e.repeatMask >> wday) & 0x01))
            match = true;
        if (e.startMonth)
        {
            if (isTodayInRange(e.startMonth, e.startDay, e.endMonth, e.endDay, cm, cd))
                match = true;
        }

        if (match)
            applyPreset(e.presetId);
            DEBUG_PRINTF_P(PSTR("[Schedule] Applying preset %u at %02u:%02u\n"), e.presetId, hr, min);
            DEBUG_PRINTF_P(PSTR("[Schedule] Checked event %u: match=%d\n"), i, match);
    }
}

void loadSchedule()
{
    if (!WLED_FS.exists(SCHEDULE_FILE))
        return;
    File file = WLED_FS.open(SCHEDULE_FILE, "r");
    if (!file)
        return;

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, file))
    {
        file.close();
        return;
    }
    file.close();

    numScheduleEvents = 0;
    for (JsonObject e : doc.as<JsonArray>())
    {
        if (numScheduleEvents >= MAX_SCHEDULE_EVENTS)
            break;
        scheduleEvents[numScheduleEvents++] = {
            (uint8_t)e["sm"].as<int>(), (uint8_t)e["sd"].as<int>(), // start month, day
            (uint8_t)e["em"].as<int>(), (uint8_t)e["ed"].as<int>(), // end month, day
            (uint8_t)e["r"].as<int>(), (uint8_t)e["h"].as<int>(), // repeat mask, hour
            (uint8_t)e["m"].as<int>(), (uint8_t)e["p"].as<int>()}; // minute, preset
    }
    DEBUG_PRINTF_P(PSTR("[Schedule] Loaded %u schedule entries from schedule.json\n"), numScheduleEvents);

}

