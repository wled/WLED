#include "src/dependencies/timezone/Timezone.h"
#include "wled.h"
#include "fcn_declare.h"

// WARNING: may cause errors in sunset calculations on ESP8266, see #3400
// building with `-D WLED_USE_REAL_MATH` will prevent those errors at the expense of flash and RAM

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

void checkTimers()
{
  if (lastTimerMinute != minute(localTime)) //only check once a new minute begins
  {
    lastTimerMinute = minute(localTime);

    // re-calculate sunrise and sunset just after midnight
    if (!hour(localTime) && minute(localTime)==1) calculateSunriseAndSunset();

    DEBUG_PRINTF_P(PSTR("Local time: %02d:%02d\n"), hour(localTime), minute(localTime));
    for (unsigned i = 0; i < 8; i++)
    {
      if (timerMacro[i] != 0
          && (timerWeekday[i] & 0x01) //timer is enabled
          && (timerHours[i] == hour(localTime) || timerHours[i] == 24) //if hour is set to 24, activate every hour
          && timerMinutes[i] == minute(localTime)
          && ((timerWeekday[i] >> weekdayMondayFirst()) & 0x01) //timer should activate at current day of week
          && isTodayInDateRange(((timerMonth[i] >> 4) & 0x0F), timerDay[i], timerMonth[i] & 0x0F, timerDayEnd[i])
         )
      {
        applyPreset(timerMacro[i]);
      }
    }
    // sunrise macro
    if (sunrise) {
      time_t tmp = sunrise + timerMinutes[8]*60;  // NOTE: may not be ok
      DEBUG_PRINTF_P(PSTR("Trigger time: %02d:%02d\n"), hour(tmp), minute(tmp));
      if (timerMacro[8] != 0
          && hour(tmp) == hour(localTime)
          && minute(tmp) == minute(localTime)
          && (timerWeekday[8] & 0x01) //timer is enabled
          && ((timerWeekday[8] >> weekdayMondayFirst()) & 0x01)) //timer should activate at current day of week
      {
        applyPreset(timerMacro[8]);
        DEBUG_PRINTF_P(PSTR("Sunrise macro %d triggered."),timerMacro[8]);
      }
    }
    // sunset macro
    if (sunset) {
      time_t tmp = sunset + timerMinutes[9]*60;  // NOTE: may not be ok
      DEBUG_PRINTF_P(PSTR("Trigger time: %02d:%02d\n"), hour(tmp), minute(tmp));
      if (timerMacro[9] != 0
          && hour(tmp) == hour(localTime)
          && minute(tmp) == minute(localTime)
          && (timerWeekday[9] & 0x01) //timer is enabled
          && ((timerWeekday[9] >> weekdayMondayFirst()) & 0x01)) //timer should activate at current day of week
      {
        applyPreset(timerMacro[9]);
        DEBUG_PRINTF_P(PSTR("Sunset macro %d triggered."),timerMacro[9]);
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