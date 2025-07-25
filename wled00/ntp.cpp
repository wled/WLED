#include "src/dependencies/timezone/Timezone.h"
#include "wled.h"
#include "fcn_declare.h"
#include <vector>

// WARNING: may cause errors in sunset calculations on ESP8266, see #3400
// building with `-D WLED_USE_REAL_MATH` will prevent those errors at the expense of flash and RAM

// Dynamic timer storage
std::vector<Timer> timers;

/*
 * Acquires time from NTP server
 */
//#define WLED_DEBUG_NTP
#define NTP_SYNC_INTERVAL 42000UL //Get fresh NTP time about twice per day

Timezone* tz;

#define TZ_UTC                  0
#define TZ_UK                   1
#define TZ_EUROPE_CENTRAL       2
#define TZ_EUROPE_EASTERN       3
#define TZ_US_EASTERN           4
#define TZ_US_CENTRAL           5
#define TZ_US_MOUNTAIN          6
#define TZ_US_ARIZONA           7
#define TZ_US_PACIFIC           8
#define TZ_CHINA                9
#define TZ_JAPAN               10
#define TZ_AUSTRALIA_EASTERN   11
#define TZ_NEW_ZEALAND         12
#define TZ_NORTH_KOREA         13
#define TZ_INDIA               14
#define TZ_SASKACHEWAN         15
#define TZ_AUSTRALIA_NORTHERN  16
#define TZ_AUSTRALIA_SOUTHERN  17
#define TZ_HAWAII              18
#define TZ_NOVOSIBIRSK         19
#define TZ_ANCHORAGE           20
#define TZ_MX_CENTRAL          21
#define TZ_PAKISTAN            22
#define TZ_BRASILIA            23

#define TZ_COUNT               24
#define TZ_INIT               255

byte tzCurrent = TZ_INIT; //uninitialized

/* C++11 form -- static std::array<std::pair<TimeChangeRule, TimeChangeRule>, TZ_COUNT> TZ_TABLE PROGMEM = {{ */
static const std::pair<TimeChangeRule, TimeChangeRule> TZ_TABLE[] PROGMEM = {
    /* TZ_UTC */ {
      {Last, Sun, Mar, 1, 0}, // UTC
      {Last, Sun, Mar, 1, 0}  // Same
    },
    /* TZ_UK */ {
      {Last, Sun, Mar, 1, 60},      //British Summer Time
      {Last, Sun, Oct, 2, 0}       //Standard Time
    },
    /* TZ_EUROPE_CENTRAL */ {
      {Last, Sun, Mar, 2, 120},     //Central European Summer Time
      {Last, Sun, Oct, 3, 60}      //Central European Standard Time
    },
    /* TZ_EUROPE_EASTERN */ {
      {Last, Sun, Mar, 3, 180},     //East European Summer Time
      {Last, Sun, Oct, 4, 120}     //East European Standard Time
    },
    /* TZ_US_EASTERN */ {
      {Second, Sun, Mar, 2, -240},  //EDT = UTC - 4 hours
      {First,  Sun, Nov, 2, -300}  //EST = UTC - 5 hours
    },
    /* TZ_US_CENTRAL */ {
      {Second, Sun, Mar, 2, -300},  //CDT = UTC - 5 hours
      {First,  Sun, Nov, 2, -360}  //CST = UTC - 6 hours
    },
    /* TZ_US_MOUNTAIN */ {
      {Second, Sun, Mar, 2, -360},  //MDT = UTC - 6 hours
      {First,  Sun, Nov, 2, -420}  //MST = UTC - 7 hours
    },
    /* TZ_US_ARIZONA */ {
      {First,  Sun, Nov, 2, -420},  //MST = UTC - 7 hours
      {First,  Sun, Nov, 2, -420}  //MST = UTC - 7 hours
    },
    /* TZ_US_PACIFIC */ {
      {Second, Sun, Mar, 2, -420},  //PDT = UTC - 7 hours
      {First,  Sun, Nov, 2, -480}  //PST = UTC - 8 hours
    },
    /* TZ_CHINA */ {
      {Last, Sun, Mar, 1, 480},     //CST = UTC + 8 hours
      {Last, Sun, Mar, 1, 480}
    },
    /* TZ_JAPAN */ {
      {Last, Sun, Mar, 1, 540},     //JST = UTC + 9 hours
      {Last, Sun, Mar, 1, 540}
    },
    /* TZ_AUSTRALIA_EASTERN */ {
      {First,  Sun, Oct, 2, 660},   //AEDT = UTC + 11 hours
      {First,  Sun, Apr, 3, 600}   //AEST = UTC + 10 hours
    },
    /* TZ_NEW_ZEALAND */ {
      {Last,   Sun, Sep, 2, 780},   //NZDT = UTC + 13 hours
      {First,  Sun, Apr, 3, 720}   //NZST = UTC + 12 hours
    },
    /* TZ_NORTH_KOREA */ {
      {Last, Sun, Mar, 1, 510},     //Pyongyang Time = UTC + 8.5 hours
      {Last, Sun, Mar, 1, 510}
    },
    /* TZ_INDIA */ {
      {Last, Sun, Mar, 1, 330},     //India Standard Time = UTC + 5.5 hours
      {Last, Sun, Mar, 1, 330}
    },
    /* TZ_SASKACHEWAN */ {
      {First,  Sun, Nov, 2, -360},  //CST = UTC - 6 hours
      {First,  Sun, Nov, 2, -360}
    },
    /* TZ_AUSTRALIA_NORTHERN */ {
      {First, Sun, Apr, 3, 570},   //ACST = UTC + 9.5 hours
      {First, Sun, Apr, 3, 570}
    },
    /* TZ_AUSTRALIA_SOUTHERN */ {
      {First, Sun, Oct, 2, 630},   //ACDT = UTC + 10.5 hours
      {First, Sun, Apr, 3, 570}   //ACST = UTC + 9.5 hours
    },
    /* TZ_HAWAII */ {
      {Last, Sun, Mar, 1, -600},   //HST =  UTC - 10 hours
      {Last, Sun, Mar, 1, -600}
    },
    /* TZ_NOVOSIBIRSK */ {
      {Last, Sun, Mar, 1, 420},     //CST = UTC + 7 hours
      {Last, Sun, Mar, 1, 420}
    },
    /* TZ_ANCHORAGE */ {
      {Second, Sun, Mar, 2, -480},  //AKDT = UTC - 8 hours
      {First, Sun, Nov, 2, -540}   //AKST = UTC - 9 hours
    },
     /* TZ_MX_CENTRAL */ {
      {First, Sun, Apr, 2, -360},  //CST = UTC - 6 hours
      {First, Sun, Apr, 2, -360}
    },
    /* TZ_PAKISTAN */ {
      {Last, Sun, Mar, 1, 300},     //Pakistan Standard Time = UTC + 5 hours
      {Last, Sun, Mar, 1, 300}
    },
    /* TZ_BRASILIA */ {
      {Last, Sun, Mar, 1, -180},    //Brasília Standard Time = UTC - 3 hours
      {Last, Sun, Mar, 1, -180}
    }
};

void updateTimezone() {
  delete tz;
  TimeChangeRule tcrDaylight, tcrStandard;
  auto tz_table_entry = currentTimezone;
  if (tz_table_entry >= TZ_COUNT) {
    tz_table_entry = 0;
  }
  tzCurrent = currentTimezone;
  memcpy_P(&tcrDaylight, &TZ_TABLE[tz_table_entry].first, sizeof(tcrDaylight));
  memcpy_P(&tcrStandard, &TZ_TABLE[tz_table_entry].second, sizeof(tcrStandard));

  tz = new Timezone(tcrDaylight, tcrStandard);
}

void handleTime() {
  handleNetworkTime();

  toki.millisecond();
  toki.setTick();

  if (toki.isTick()) //true only in the first loop after a new second started
  {
    #ifdef WLED_DEBUG_NTP
    Serial.print(F("TICK! "));
    toki.printTime(toki.getTime());
    #endif
    updateLocalTime();
    checkTimers();
    checkCountdown();
  }
}

void handleNetworkTime()
{
  if (ntpEnabled && ntpConnected && millis() - ntpLastSyncTime > (1000*NTP_SYNC_INTERVAL) && WLED_CONNECTED)
  {
    if (millis() - ntpPacketSentTime > 10000)
    {
      #ifdef ARDUINO_ARCH_ESP32   // I had problems using udp.flush() on 8266
      while (ntpUdp.parsePacket() > 0) ntpUdp.flush(); // flush any existing packets
      #endif
      sendNTPPacket();
      ntpPacketSentTime = millis();
    }
    if (checkNTPResponse())
    {
      ntpLastSyncTime = millis();
    }
  }
}

void sendNTPPacket()
{
  if (!ntpServerIP.fromString(ntpServerName)) //see if server is IP or domain
  {
    #ifdef ESP8266
    WiFi.hostByName(ntpServerName, ntpServerIP, 750);
    #else
    WiFi.hostByName(ntpServerName, ntpServerIP);
    #endif
  }

  DEBUG_PRINTLN(F("send NTP"));
  byte pbuf[NTP_PACKET_SIZE];
  memset(pbuf, 0, NTP_PACKET_SIZE);

  pbuf[0] = 0b11100011;   // LI, Version, Mode
  pbuf[1] = 0;     // Stratum, or type of clock
  pbuf[2] = 6;     // Polling Interval
  pbuf[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  pbuf[12]  = 49;
  pbuf[13]  = 0x4E;
  pbuf[14]  = 49;
  pbuf[15]  = 52;

  ntpUdp.beginPacket(ntpServerIP, 123); //NTP requests are to port 123
  ntpUdp.write(pbuf, NTP_PACKET_SIZE);
  ntpUdp.endPacket();
}

static bool isValidNtpResponse(const byte* ntpPacket) {
  // Perform a few validity checks on the packet
  //   based on https://github.com/taranais/NTPClient/blob/master/NTPClient.cpp
  if((ntpPacket[0] & 0b11000000) == 0b11000000) return false; //reject LI=UNSYNC
  // if((ntpPacket[0] & 0b00111000) >> 3 < 0b100) return false; //reject Version < 4
  if((ntpPacket[0] & 0b00000111) != 0b100)      return false; //reject Mode != Server
  if((ntpPacket[1] < 1) || (ntpPacket[1] > 15)) return false; //reject invalid Stratum
  if( ntpPacket[16] == 0 && ntpPacket[17] == 0 && 
      ntpPacket[18] == 0 && ntpPacket[19] == 0 &&
      ntpPacket[20] == 0 && ntpPacket[21] == 0 &&
      ntpPacket[22] == 0 && ntpPacket[23] == 0)               //reject ReferenceTimestamp == 0
    return false;

  return true;
}

bool checkNTPResponse()
{
  int cb = ntpUdp.parsePacket();
  if (cb < NTP_MIN_PACKET_SIZE) {
    #ifdef ARDUINO_ARCH_ESP32   // I had problems using udp.flush() on 8266
    if (cb > 0) ntpUdp.flush();  // this avoids memory leaks on esp32
    #endif
    return false;
  }

  uint32_t ntpPacketReceivedTime = millis();
  DEBUG_PRINTF_P(PSTR("NTP recv, l=%d\n"), cb);
  byte pbuf[NTP_PACKET_SIZE];
  ntpUdp.read(pbuf, NTP_PACKET_SIZE); // read the packet into the buffer
  if (!isValidNtpResponse(pbuf)) return false;  // verify we have a valid response to client

  Toki::Time arrived  = toki.fromNTP(pbuf + 32);
  Toki::Time departed = toki.fromNTP(pbuf + 40);
  if (departed.sec == 0) return false;
  //basic half roundtrip estimation
  uint32_t serverDelay = toki.msDifference(arrived, departed);
  uint32_t offset = (ntpPacketReceivedTime - ntpPacketSentTime - serverDelay) >> 1;
  #ifdef WLED_DEBUG_NTP
  //the time the packet departed the NTP server
  toki.printTime(departed);
  #endif

  toki.adjust(departed, offset);
  toki.setTime(departed, TOKI_TS_NTP);

  #ifdef WLED_DEBUG_NTP
  Serial.print("Arrived: ");
  toki.printTime(arrived);
  Serial.print("Time: ");
  toki.printTime(departed);
  Serial.print("Roundtrip: ");
  Serial.println(ntpPacketReceivedTime - ntpPacketSentTime);
  Serial.print("Offset: ");
  Serial.println(offset);
  Serial.print("Serverdelay: ");
  Serial.println(serverDelay);
  #endif

  if (countdownTime - toki.second() > 0) countdownOverTriggered = false;
  // if time changed re-calculate sunrise/sunset
  updateLocalTime();
  calculateSunriseAndSunset();
  return true;
}

void updateLocalTime()
{
  if (currentTimezone != tzCurrent) updateTimezone();
  unsigned long tmc = toki.second()+ utcOffsetSecs;
  localTime = tz->toLocal(tmc);
}

void getTimeString(char* out)
{
  updateLocalTime();
  byte hr = hour(localTime);
  if (useAMPM)
  {
    if (hr > 11) hr -= 12;
    if (hr == 0) hr  = 12;
  }
  sprintf_P(out,PSTR("%i-%i-%i, %02d:%02d:%02d"),year(localTime), month(localTime), day(localTime), hr, minute(localTime), second(localTime));
  if (useAMPM)
  {
    strcat_P(out,PSTR(" "));
    strcat(out,(hour(localTime) > 11)? "PM":"AM");
  }
}

void setCountdown()
{
  if (currentTimezone != tzCurrent) updateTimezone();
  countdownTime = tz->toUTC(getUnixTime(countdownHour, countdownMin, countdownSec, countdownDay, countdownMonth, countdownYear));
  if (countdownTime - toki.second() > 0) countdownOverTriggered = false;
}

//returns true if countdown just over
bool checkCountdown()
{
  unsigned long n = toki.second();
  if (countdownMode) localTime = countdownTime - n + utcOffsetSecs;
  if (n > countdownTime) {
    if (countdownMode) localTime = n - countdownTime + utcOffsetSecs;
    if (!countdownOverTriggered)
    {
      if (macroCountdown != 0) applyPreset(macroCountdown);
      countdownOverTriggered = true;
      return true;
    }
  }
  return false;
}

byte weekdayMondayFirst()
{
  byte wd = weekday(localTime) -1;
  if (wd == 0) wd = 7;
  return wd;
}

bool isTodayInDateRange(byte monthStart, byte dayStart, byte monthEnd, byte dayEnd)
{
	if (monthStart == 0 || dayStart == 0) return true;
	if (monthEnd == 0) monthEnd = monthStart;
	if (dayEnd == 0) dayEnd = 31;
	byte d = day(localTime);
	byte m = month(localTime);

	if (monthStart < monthEnd) {
		if (m > monthStart && m < monthEnd) return true;
		if (m == monthStart) return (d >= dayStart);
		if (m == monthEnd) return (d <= dayEnd);
		return false;
	}
	if (monthEnd < monthStart) { //range spans change of year
		if (m > monthStart || m < monthEnd) return true;
		if (m == monthStart) return (d >= dayStart);
		if (m == monthEnd) return (d <= dayEnd);
		return false;
	}

	//start month and end month are the same
	if (dayEnd < dayStart) return (m != monthStart || (d <= dayEnd || d >= dayStart)); //all year, except the designated days in this month
	return (m == monthStart && d >= dayStart && d <= dayEnd); //just the designated days this month
}

/*
 * Timer Management Functions
 */

// Add a timer to the vector with validation
void addTimer(uint8_t preset, uint8_t hour, int8_t minute, uint8_t weekdays,
              uint8_t monthStart, uint8_t monthEnd, 
              uint8_t dayStart, uint8_t dayEnd) {
  // Prevent unbounded memory growth by enforcing timer limit
  if (timers.size() >= maxTimePresets) {
    DEBUG_PRINTLN(F("Error: Maximum number of timers reached"));
    return;
  }
  
  // Validate hour (0-23 for regular timers, or special values for sunrise/sunset)
  if (hour > 23 && hour != TIMER_HOUR_SUNSET && hour != TIMER_HOUR_SUNRISE) {
    DEBUG_PRINTLN(F("Error: Invalid hour value"));
    return;
  }
  
  // Validate minute based on timer type
  if (hour < TIMER_HOUR_SUNSET) { // Regular timer
    if (minute < 0 || minute > 59) {
      DEBUG_PRINTLN(F("Error: Invalid minute value for regular timer"));
      return;
    }
  } else { // Sunrise/sunset offset
    if (minute < -59 || minute > 59) {
      DEBUG_PRINTLN(F("Error: Invalid minute offset for sunrise/sunset"));
      return;
    }
  }
  
  // Validate weekdays (7-bit bitmask: 0-127)
  weekdays &= 0x7F; // Ensure only valid bits are set
  
  // Validate month range
  monthStart = constrain(monthStart, 1, 12);
  monthEnd = constrain(monthEnd, 1, 12);
  
  // Validate day range
  dayStart = constrain(dayStart, 1, 31);
  dayEnd = constrain(dayEnd, 1, 31);
  
  // All validation passed, add the timer
  Timer newTimer(preset, hour, minute, weekdays, monthStart, monthEnd, dayStart, dayEnd);
  timers.push_back(newTimer);
  syncTimersToArrays();
}

// Clear all timers
void clearTimers() {
  timers.clear();
  syncTimersToArrays();
}

// Legacy array size constants
const uint8_t LEGACY_TIMER_ARRAY_SIZE = 10;      // Size of main timer arrays
const uint8_t LEGACY_DATE_ARRAY_SIZE = 8;       // Size of date-related arrays
const uint8_t LEGACY_REGULAR_TIMER_MAX = 8;     // Maximum number of regular timers (indices 0-7)
const uint8_t LEGACY_SUNRISE_INDEX = 8;         // Reserved index for sunrise timer
const uint8_t LEGACY_SUNSET_INDEX = 9;          // Reserved index for sunset timer

// Sync vector timers to legacy arrays for backward compatibility
void syncTimersToArrays() {
  // Clear legacy arrays
  memset(timerMacro, 0, LEGACY_TIMER_ARRAY_SIZE);
  memset(timerHours, 0, LEGACY_TIMER_ARRAY_SIZE);
  memset(timerMinutes, 0, LEGACY_TIMER_ARRAY_SIZE);
  memset(timerWeekday, 0, LEGACY_TIMER_ARRAY_SIZE);
  memset(timerMonth, 0, LEGACY_DATE_ARRAY_SIZE);
  memset(timerDay, 0, LEGACY_DATE_ARRAY_SIZE);
  memset(timerDayEnd, 0, LEGACY_DATE_ARRAY_SIZE);
  
  uint8_t regularTimerCount = 0;
  bool sunriseTimerSynced = false;
  bool sunsetTimerSynced = false;
  bool regularTimersDropped = false;
  
  for (const auto& timer : timers) {
    if (timer.isSunrise()) {
      if (!sunriseTimerSynced) {
        // Sunrise timer goes to reserved index
        timerMacro[LEGACY_SUNRISE_INDEX] = timer.preset;
        timerHours[LEGACY_SUNRISE_INDEX] = timer.hour;
        timerMinutes[LEGACY_SUNRISE_INDEX] = timer.minute;
        timerWeekday[LEGACY_SUNRISE_INDEX] = timer.weekdays;
        sunriseTimerSynced = true;
      } else {
        DEBUG_PRINTLN(F("Warning: Multiple sunrise timers found, only first one synced to legacy arrays"));
      }
    } else if (timer.isSunset()) {
      if (!sunsetTimerSynced) {
        // Sunset timer goes to reserved index
        timerMacro[LEGACY_SUNSET_INDEX] = timer.preset;
        timerHours[LEGACY_SUNSET_INDEX] = timer.hour;
        timerMinutes[LEGACY_SUNSET_INDEX] = timer.minute;
        timerWeekday[LEGACY_SUNSET_INDEX] = timer.weekdays;
        sunsetTimerSynced = true;
      } else {
        DEBUG_PRINTLN(F("Warning: Multiple sunset timers found, only first one synced to legacy arrays"));
      }
    } else if (timer.isRegular()) {
      if (regularTimerCount < LEGACY_REGULAR_TIMER_MAX) {
        // Regular timers go to indices 0-7
        timerMacro[regularTimerCount] = timer.preset;
        timerHours[regularTimerCount] = timer.hour;
        timerMinutes[regularTimerCount] = timer.minute;
        timerWeekday[regularTimerCount] = timer.weekdays;
        
        // Date range info (only for regular timers)
        timerMonth[regularTimerCount] = (timer.monthStart << 4) | (timer.monthEnd & 0x0F);
        timerDay[regularTimerCount] = timer.dayStart;
        timerDayEnd[regularTimerCount] = timer.dayEnd;
        
        regularTimerCount++;
      } else {
        if (!regularTimersDropped) {
          DEBUG_PRINTLN(F("Warning: Too many regular timers for legacy arrays, some timers not synced"));
          regularTimersDropped = true; // Only print warning once
        }
      }
    }
  }
  
  if (regularTimerCount == LEGACY_REGULAR_TIMER_MAX && timers.size() > LEGACY_TIMER_ARRAY_SIZE) {
    DEBUG_PRINTF("Timer sync: %d regular timers synced, %d total timers in vector\n", 
                 regularTimerCount, timers.size());
  }
}

// Get timer count for different types
uint8_t getTimerCount() {
  return timers.size();
}

uint8_t getRegularTimerCount() {
  uint8_t count = 0;
  for (const auto& timer : timers) {
    if (timer.isRegular()) count++;
  }
  return count;
}

bool hasSunriseTimer() {
  for (const auto& timer : timers) {
    if (timer.isSunrise()) return true;
  }
  return false;
}

bool hasSunsetTimer() {
  for (const auto& timer : timers) {
    if (timer.isSunset()) return true;
  }
  return false;
}

void checkTimers()
{
  if (lastTimerMinute != minute(localTime)) //only check once a new minute begins
  {
    lastTimerMinute = minute(localTime);

    // re-calculate sunrise and sunset just after midnight
    if (!hour(localTime) && minute(localTime)==1) calculateSunriseAndSunset();

    DEBUG_PRINTF_P(PSTR("Local time: %02d:%02d\n"), hour(localTime), minute(localTime));
    
    // Check all timers in the vector
    for (const auto& timer : timers) {
      if (!timer.isEnabled() || timer.preset == 0) continue;
      
      bool shouldTrigger = false;
      
      if (timer.isRegular()) {
        // Regular timer logic
        shouldTrigger = (timer.hour == hour(localTime) || timer.hour == 24) // 24 = every hour
                     && timer.minute == minute(localTime)
                     && ((timer.weekdays >> weekdayMondayFirst()) & 0x01) // weekday check
                     && isTodayInDateRange(timer.monthStart, timer.dayStart, timer.monthEnd, timer.dayEnd);
      }
      else if (timer.isSunrise() && sunrise) {
        // Sunrise timer logic
        time_t triggerTime = sunrise + timer.minute * 60; // minute is offset for sunrise/sunset
        shouldTrigger = hour(triggerTime) == hour(localTime)
                     && minute(triggerTime) == minute(localTime)
                     && ((timer.weekdays >> weekdayMondayFirst()) & 0x01);
        
        if (shouldTrigger) {
          DEBUG_PRINTF_P(PSTR("Sunrise timer triggered at %02d:%02d\n"), hour(triggerTime), minute(triggerTime));
        }
      }
      else if (timer.isSunset() && sunset) {
        // Sunset timer logic
        time_t triggerTime = sunset + timer.minute * 60; // minute is offset for sunrise/sunset
        shouldTrigger = hour(triggerTime) == hour(localTime)
                     && minute(triggerTime) == minute(localTime)
                     && ((timer.weekdays >> weekdayMondayFirst()) & 0x01);
        
        if (shouldTrigger) {
          DEBUG_PRINTF_P(PSTR("Sunset timer triggered at %02d:%02d\n"), hour(triggerTime), minute(triggerTime));
        }
      }
      
      if (shouldTrigger) {
        DEBUG_PRINTF_P(PSTR("Timer triggered: preset %d\n"), timer.preset);
        applyPreset(timer.preset);
      }
    }
  }
}

#define ZENITH -0.83
// get sunrise (or sunset) time (in minutes) for a given day at a given geo location. Returns >= INT16_MAX in case of "no sunset"
static int getSunriseUTC(int year, int month, int day, float lat, float lon, bool sunset=false) {
  //1. first calculate the day of the year
  float N1 = 275 * month / 9;
  float N2 = (month + 9) / 12;
  float N3 = (1.0f + floor_t((year - 4 * floor_t(year / 4) + 2.0f) / 3.0f));
  float N = N1 - (N2 * N3) + day - 30.0f;

  //2. convert the longitude to hour value and calculate an approximate time
  float lngHour = lon / 15.0f;
  float t = N + (((sunset ? 18 : 6) - lngHour) / 24);

  //3. calculate the Sun's mean anomaly
  float M = (0.9856f * t) - 3.289f;

  //4. calculate the Sun's true longitude
  float L = fmod_t(M + (1.916f * sin_t(DEG_TO_RAD*M)) + (0.02f * sin_t(2*DEG_TO_RAD*M)) + 282.634f, 360.0f);

  //5a. calculate the Sun's right ascension
  float RA = fmod_t(RAD_TO_DEG*atan_t(0.91764f * tan_t(DEG_TO_RAD*L)), 360.0f);

  //5b. right ascension value needs to be in the same quadrant as L
  float Lquadrant  = floor_t( L/90) * 90;
  float RAquadrant = floor_t(RA/90) * 90;
  RA = RA + (Lquadrant - RAquadrant);

  //5c. right ascension value needs to be converted into hours
  RA /= 15.0f;

  //6. calculate the Sun's declination
  float sinDec = 0.39782f * sin_t(DEG_TO_RAD*L);
  float cosDec = cos_t(asin_t(sinDec));

  //7a. calculate the Sun's local hour angle
  float cosH = (sin_t(DEG_TO_RAD*ZENITH) - (sinDec * sin_t(DEG_TO_RAD*lat))) / (cosDec * cos_t(DEG_TO_RAD*lat));
  if ((cosH > 1.0f) && !sunset) return INT16_MAX;  // the sun never rises on this location (on the specified date)
  if ((cosH < -1.0f) && sunset) return INT16_MAX;  // the sun never sets on this location (on the specified date)

  //7b. finish calculating H and convert into hours
  float H = sunset ? RAD_TO_DEG*acos_t(cosH) : 360 - RAD_TO_DEG*acos_t(cosH);
  H /= 15.0f;

  //8. calculate local mean time of rising/setting
  float T = H + RA - (0.06571f * t) - 6.622f;

  //9. adjust back to UTC
  float UT = fmod_t(T - lngHour, 24.0f);

  // return in minutes from midnight
	return UT*60;
}

#define SUNSET_MAX (24*60) // 1day = max expected absolute value for sun offset in minutes 
// calculate sunrise and sunset (if longitude and latitude are set)
void calculateSunriseAndSunset() {
  if ((int)(longitude*10.) || (int)(latitude*10.)) {
    struct tm tim_0;
    tim_0.tm_year = year(localTime)-1900;
    tim_0.tm_mon = month(localTime)-1;
    tim_0.tm_mday = day(localTime);
    tim_0.tm_sec = 0;
    tim_0.tm_isdst = 0;

    // Due to limited accuracy, its possible to get a bad sunrise/sunset displayed as "00:00" (see issue #3601)
    // So in case of invalid result, we try to use the sunset/sunrise of previous day. Max 3 days back, this worked well in all cases I tried.
    // When latitude = 66,6 (N or S), the functions sometimes returns 2147483647, so this "unexpected large" is another condition for retry
    int minUTC = 0;
    int retryCount = 0;
    do {
      time_t theDay = localTime - retryCount * 86400; // one day back = 86400 seconds
      minUTC = getSunriseUTC(year(theDay), month(theDay), day(theDay), latitude, longitude, false);
      DEBUG_PRINTF_P(PSTR("* sunrise (minutes from UTC) = %d\n"), minUTC);
      retryCount ++;
    } while ((abs(minUTC) > SUNSET_MAX)  && (retryCount <= 3));

    if (abs(minUTC) <= SUNSET_MAX) {
      // there is a sunrise
      if (minUTC < 0) minUTC += 24*60; // add a day if negative
      tim_0.tm_hour = minUTC / 60;
      tim_0.tm_min = minUTC % 60;
      sunrise = tz->toLocal(mktime(&tim_0) + utcOffsetSecs);
      DEBUG_PRINTF_P(PSTR("Sunrise: %02d:%02d\n"), hour(sunrise), minute(sunrise));
    } else {
      sunrise = 0;
    }

    retryCount = 0;
    do {
      time_t theDay = localTime - retryCount * 86400; // one day back = 86400 seconds
      minUTC = getSunriseUTC(year(theDay), month(theDay), day(theDay), latitude, longitude, true);
      DEBUG_PRINTF_P(PSTR("* sunset  (minutes from UTC) = %d\n"), minUTC);
      retryCount ++;
    } while ((abs(minUTC) > SUNSET_MAX)  && (retryCount <= 3));

    if (abs(minUTC) <= SUNSET_MAX) {
      // there is a sunset
      if (minUTC < 0) minUTC += 24*60; // add a day if negative
      tim_0.tm_hour = minUTC / 60;
      tim_0.tm_min = minUTC % 60;
      sunset = tz->toLocal(mktime(&tim_0) + utcOffsetSecs);
      DEBUG_PRINTF_P(PSTR("Sunset: %02d:%02d\n"), hour(sunset), minute(sunset));
    } else {
      sunset = 0;
    }
  }
}

//time from JSON and HTTP API
void setTimeFromAPI(uint32_t timein) {
  if (timein == 0 || timein == UINT32_MAX) return;
  uint32_t prev = toki.second();
  //only apply if more accurate or there is a significant difference to the "more accurate" time source
  uint32_t diff = (timein > prev) ? timein - prev : prev - timein;
  if (toki.getTimeSource() > TOKI_TS_JSON && diff < 60U) return;

  toki.setTime(timein, TOKI_NO_MS_ACCURACY, TOKI_TS_JSON);
  if (diff >= 60U) {
    updateLocalTime();
    calculateSunriseAndSunset();
  }
  if (presetsModifiedTime == 0) presetsModifiedTime = timein;
}