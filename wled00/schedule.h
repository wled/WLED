// schedule.h
#pragma once

#include <stdint.h>

#define MAX_SCHEDULE_EVENTS 32

struct ScheduleEvent {
  uint8_t startMonth;
  uint8_t startDay;
  uint8_t endMonth;
  uint8_t endDay;
  uint8_t repeatMask;
  uint8_t hour;
  uint8_t minute;
  uint8_t presetId;
};

void loadSchedule();
void checkSchedule();
