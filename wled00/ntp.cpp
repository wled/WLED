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

#define TZ_INIT 255
byte tzCurrent = TZ_INIT; //uninitialized

// workaround to put all strings into PROGMEM
static const char _utc[]    PROGMEM = "UTC";
static const char _gmt[]    PROGMEM = "GMT/BST";
static const char _cet[]    PROGMEM = "CET/CEST";
static const char _eet[]    PROGMEM = "EET/EEST";
static const char _us_est[] PROGMEM = "US-EST/EDT";
static const char _us_cst[] PROGMEM = "US-CST/CDT";
static const char _us_mst[] PROGMEM = "US-MST/MDT";
static const char _us_az[]  PROGMEM = "US-AZ";
static const char _us_pst[] PROGMEM = "US-PST/PDT";
static const char _cst[]    PROGMEM = "CST (AWST, PHST)";
static const char _jst[]    PROGMEM = "JST (KST)";
static const char _aest[]   PROGMEM = "AEST/AEDT";
static const char _nzst[]   PROGMEM = "NZST/NZDT";
static const char _nkst[]   PROGMEM = "North Korea";
static const char _ist[]    PROGMEM = "IST (India)";
static const char _ca_sk[]  PROGMEM = "CA-Saskatchewan";
static const char _acst[]   PROGMEM = "ACST";
static const char _acst2[]  PROGMEM = "ACST/ACDT";
static const char _hst[]    PROGMEM = "HST (Hawaii)";
static const char _novt[]   PROGMEM = "NOVT (Novosibirsk)";
static const char _akst[]   PROGMEM = "AKST/AKDT (Anchorage)";
static const char _mxcst[]  PROGMEM = "MX-CST";
static const char _pkt[]    PROGMEM = "PKT (Pakistan)";
static const char _brt[]    PROGMEM = "BRT (Brasília)";
static const char _awst[]   PROGMEM = "AWST (Perth)";

// WARNING: Changing the order of entries in this table will change the meaning of stored timezone indices in settings!
// Add new timezones only at the end of the list to preserve compatibility!
using tz_data = std::tuple<const char*, const TimeChangeRule, const TimeChangeRule>;
static const tz_data TZ_TABLE[] PROGMEM = {
  tz_data{
    _utc,
    {Last, Sun, Mar, 1, 0}, // UTC
    {Last, Sun, Mar, 1, 0}  // Same
  },
  tz_data{
    _gmt,
    {Last, Sun, Mar, 1, 60},      //British Summer Time
    {Last, Sun, Oct, 2, 0}       //Standard Time
  },
  tz_data{
    _cet,
    {Last, Sun, Mar, 2, 120},     //Central European Summer Time
    {Last, Sun, Oct, 3, 60}      //Central European Standard Time
  },
  tz_data{
    _eet,
    {Last, Sun, Mar, 3, 180},     //East European Summer Time
    {Last, Sun, Oct, 4, 120}     //East European Standard Time
  },
  tz_data{
    _us_est,
    {Second, Sun, Mar, 2, -240},  //EDT = UTC - 4 hours
    {First,  Sun, Nov, 2, -300}  //EST = UTC - 5 hours
  },
  tz_data{
    _us_cst,
    {Second, Sun, Mar, 2, -300},  //CDT = UTC - 5 hours
    {First,  Sun, Nov, 2, -360}  //CST = UTC - 6 hours
  },
  tz_data{
    _us_mst,
    {Second, Sun, Mar, 2, -360},  //MDT = UTC - 6 hours
    {First,  Sun, Nov, 2, -420}  //MST = UTC - 7 hours
  },
  tz_data{
    _us_az,
    {First,  Sun, Nov, 2, -420},  //MST = UTC - 7 hours
    {First,  Sun, Nov, 2, -420}  //MST = UTC - 7 hours
  },
  tz_data{
    _us_pst,
    {Second, Sun, Mar, 2, -420},  //PDT = UTC - 7 hours
    {First,  Sun, Nov, 2, -480}  //PST = UTC - 8 hours
  },
  tz_data{
    _cst,
    {Last, Sun, Mar, 1, 480},     //CST = UTC + 8 hours
    {Last, Sun, Mar, 1, 480}
  },
  tz_data{
    _jst,
    {Last, Sun, Mar, 1, 540},     //JST = UTC + 9 hours
    {Last, Sun, Mar, 1, 540}
  },
  tz_data{
    _aest,
    {First,  Sun, Oct, 2, 660},   //AEDT = UTC + 11 hours
    {First,  Sun, Apr, 3, 600}   //AEST = UTC + 10 hours
  },
  tz_data{
    _nzst,
    {Last,   Sun, Sep, 2, 780},   //NZDT = UTC + 13 hours
    {First,  Sun, Apr, 3, 720}   //NZST = UTC + 12 hours
  },
  tz_data{
    _nkst,
    {Last, Sun, Mar, 1, 510},     //Pyongyang Time = UTC + 8.5 hours
    {Last, Sun, Mar, 1, 510}
  },
  tz_data{
    _ist,
    {Last, Sun, Mar, 1, 330},     //India Standard Time = UTC + 5.5 hours
    {Last, Sun, Mar, 1, 330}
  },
  tz_data{
    _ca_sk,
    {First,  Sun, Nov, 2, -360},  //CST = UTC - 6 hours
    {First,  Sun, Nov, 2, -360}
  },
  tz_data{
    _acst,
    {First, Sun, Apr, 3, 570},   //ACST = UTC + 9.5 hours
    {First, Sun, Apr, 3, 570}
  },
  tz_data{
    _acst2,
    {First, Sun, Oct, 2, 630},   //ACDT = UTC + 10.5 hours
    {First, Sun, Apr, 3, 570}   //ACST = UTC + 9.5 hours
  },
  tz_data{
    _hst,
    {Last, Sun, Mar, 1, -600},   //HST =  UTC - 10 hours
    {Last, Sun, Mar, 1, -600}
  },
  tz_data{
    _novt,
    {Last, Sun, Mar, 1, 420},     //CST = UTC + 7 hours
    {Last, Sun, Mar, 1, 420}
  },
  tz_data{
    _akst,
    {Second, Sun, Mar, 2, -480},  //AKDT = UTC - 8 hours
    {First, Sun, Nov, 2, -540}   //AKST = UTC - 9 hours
  },
  tz_data{
    _mxcst,
    {First, Sun, Apr, 2, -360},  //CST = UTC - 6 hours
    {First, Sun, Apr, 2, -360}
  },
  tz_data{
    _pkt,
    {Last, Sun, Mar, 1, 300},     //Pakistan Standard Time = UTC + 5 hours
    {Last, Sun, Mar, 1, 300}
  },
  tz_data{
    _brt,
    {Last, Sun, Mar, 1, -180},    //Brasília Standard Time = UTC - 3 hours
    {Last, Sun, Mar, 1, -180}
  },
  tz_data{
    _awst,
    {Last, Sun, Mar, 1, 480},     //AWST = UTC + 8 hours
    {Last, Sun, Mar, 1, 480}      //AWST = UTC + 8 hours (no DST)
  }
};

#ifndef WLED_NO_CUSTOM_TIMEZONE
/*
bool posixTimezone(TimeChangeRule &dst, TimeChangeRule &std) {
  // example: "Europe/Ljubljana":"CET-1CEST,M3.5.0,M10.5.0/3"
}
*/
static const char __weeks[] PROGMEM = "LasFirSecThiFou";
static char _week[4];
static const char* weekShortStr(uint8_t dow) {
   if (dow > 4) dow = 0;
   uint8_t index = dow*3;
   for (int i=0; i < 3; i++) _week[i] = pgm_read_byte(&(__weeks[index + i]));
   _week[3] = 0;
   return _week;
}

bool jsonTimezone(TimeChangeRule &dst, TimeChangeRule &std) {
  // example: {"dst":{"w":0,"dow":1,"m":3,"h":1,"off":60},"std":{"w":0,"dow":1,"m":10,"h":1,"off":0}}
  // example: {"dst":{"w":"Last","dow":"Sun","m":"Mar","h":1,"off":60},"std":{"w":"Last","dow":"Sun","m":"Oct","h":1,"off":0}}
  bool success = false;
  File f;
  f = WLED_FS.open("timezone.json", "r");
  if (f) {
    StaticJsonDocument<64> filter;
    filter["dst"] = true;
    filter["std"] = true;
    StaticJsonDocument<256> d;
    if (deserializeJson(d, f, DeserializationOption::Filter(filter)) == DeserializationError::Ok) {
      JsonObject o = d.as<JsonObject>();
      if (!(o[F("dst")].isNull() || o[F("std")].isNull())) {
        if (o["dst"]["w"].is<const char*>())
          for (int i=0; i<5; i++) {
            if (strncmp(weekShortStr(i), o["dst"]["w"].as<const char*>(), 3) == 0) {
              dst.week = i;
              break;
            }
          }
        else dst.week = o["dst"]["w"] | 0;
        if (o["dst"]["dow"].is<const char*>())
          for (int i=0; i<7; i++) {
            if (strncmp(dayShortStr(i+1), o["dst"]["dow"].as<const char*>(), 3) == 0) {
              dst.dow = i+1;
              break;
            }
          }
        else dst.dow = o["dst"]["dow"] | 1;
        if (o["dst"]["m"].is<const char*>())
          for (int i=0; i<12; i++) {
            if (strncmp(monthShortStr(i+1), o["dst"]["m"].as<const char*>(), 3) == 0) {
              dst.month = i+1;
              break;
            }
          }
        else dst.month = o["dst"]["m"] | 3;
        dst.hour   = o["dst"]["h"]   | 1;
        dst.offset = o["dst"]["off"] | 0;

        if (o["std"]["w"].is<const char*>())
          for (int i=0; i<5; i++) {
            if (strncmp(weekShortStr(i), o["std"]["w"].as<const char*>(), 3) == 0) {
              std.week = i;
              break;
            }
          }
        else std.week = o["std"]["w"] | 0;
        if (o["std"]["dow"].is<const char*>())
          for (int i=0; i<7; i++) {
            if (strncmp(dayShortStr(i+1), o["std"]["dow"].as<const char*>(), 3) == 0) {
              std.dow = i+1;
              break;
            }
          }
        else std.dow = o["std"]["dow"] | 1;
        if (o["std"]["m"].is<const char*>())
          for (int i=0; i<12; i++) {
            if (strncmp(monthShortStr(i+1), o["std"]["m"].as<const char*>(), 3) == 0) {
              std.month = i+1;
              break;
            }
          }
        else std.month = o["std"]["m"] | 3;
        std.hour   = o["std"]["h"]   | 1;
        std.offset = o["std"]["off"] | 0;

        success = true;
      }
    }
    f.close();
  }
  return success;
}
#endif

void updateTimezone() {
  bool customTZ = false;
  delete tz;
  TimeChangeRule tcrDaylight, tcrStandard;
  auto tz_table_entry = currentTimezone;
  if (tz_table_entry >= countof(TZ_TABLE)) {
    tz_table_entry = 0;
    #ifndef WLED_NO_CUSTOM_TIMEZONE
    customTZ = jsonTimezone(tcrDaylight, tcrStandard);
    #endif
  }
  tzCurrent = currentTimezone;
  if (!customTZ) {
    memcpy_P(&tcrDaylight, &std::get<1>(TZ_TABLE[tz_table_entry]), sizeof(tcrDaylight));
    memcpy_P(&tcrStandard, &std::get<2>(TZ_TABLE[tz_table_entry]), sizeof(tcrStandard));
  }
  tz = new Timezone(tcrDaylight, tcrStandard);
}

String getTZNamesJSONString() {
  String names = "[";
  names.reserve(512); // prevent heap fragmentation by allocating needed space upfront
  for (size_t i = 0; i < countof(TZ_TABLE); i++) {
    // the following is shorter code than sprintf()
    names += '"';
    names += FPSTR(std::get<0>(TZ_TABLE[i]));
    names += '"';
    names += ',';
  }
  #ifndef WLED_NO_CUSTOM_TIMEZONE
  names += F("\"Custom (JSON)\"]");
  #else
  names.setCharAt(names.length()-1, ']'); // replace last comma with bracket
  #endif
  return names;
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
