// schedule.h
// Defines the schedule event data structure and declares schedule-related functions

#pragma once

#include <stdint.h>

// Maximum number of schedule events supported
#define MAX_SCHEDULE_EVENTS 32

struct ScheduleEvent {
  uint8_t startMonth : 4;  // 0–12 (0 means no date range)
  uint8_t startDay   : 5;  // 0–31

  uint8_t endMonth   : 4;  // 0–12
  uint8_t endDay     : 5;  // 0–31

  uint8_t repeatMask : 7;  // bitmask for days of week (Sun=bit0 .. Sat=bit6)

  uint8_t hour       : 5;  // 0–23
  uint8_t minute     : 6;  // 0–59

  uint8_t presetId;        // 1–250
};

// Loads the schedule from the JSON file; returns true if successful
bool loadSchedule();

// Checks current time against schedule and applies any matching preset
void checkSchedule();

// Checks if the current date is within the specified range
bool isTodayInRange(uint8_t sm, uint8_t sd, uint8_t em, uint8_t ed, uint8_t cm, uint8_t cd);