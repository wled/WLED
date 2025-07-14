// schedule.h
// Defines the schedule event data structure and declares schedule-related functions

#pragma once

#include <stdint.h>

// Maximum number of schedule events supported
#define MAX_SCHEDULE_EVENTS 32

// Structure representing one scheduled event
struct ScheduleEvent {
  uint8_t startMonth;  // Starting month of the schedule event (1-12)
  uint8_t startDay;    // Starting day of the schedule event (1-31)
  uint8_t endMonth;    // Ending month of the schedule event (1-12)
  uint8_t endDay;      // Ending day of the schedule event (1-31)
  uint8_t repeatMask;  // Bitmask indicating repeat days of the week (bit0=Sunday ... bit6=Saturday)
  uint8_t hour;        // Hour of day when event triggers (0-23)
  uint8_t minute;      // Minute of hour when event triggers (0-59)
  uint8_t presetId;    // Preset number to apply when event triggers (1-250)
};

// Loads the schedule from the JSON file; returns true if successful
bool loadSchedule();

// Checks current time against schedule and applies any matching preset
void checkSchedule();