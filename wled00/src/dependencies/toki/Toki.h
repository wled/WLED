/*
  Toki.h - Minimal millisecond accurate timekeeping.

  LICENSE
  The MIT License (MIT)
  Copyright (c) 2021 Christian Schwinne
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <Arduino.h> 

#define YEARS_70 2208988800UL

#define TOKI_NO_MS_ACCURACY 1000

//Time source. Sub-100 is second accuracy, higher ms accuracy. Higher value generally means more accurate
#define TOKI_TS_NONE      0 //unsynced (e.g. just after boot)
#define TOKI_TS_UDP       5 //synced via UDP from an instance whose time source is unsynced
#define TOKI_TS_BAD      10 //synced from a time source less than about +- 2s accurate
#define TOKI_TS_UDP_SEC  20 //synced via UDP from an instance whose time source is set from RTC/JSON
#define TOKI_TS_SEC      40 //general second-accurate time source
#define TOKI_TS_RTC      60 //second-accurate real time clock
#define TOKI_TS_JSON     70 //synced second-accurate from a client via JSON-API

#define TOKI_TS_UDP_NTP 110 //synced via UDP from an instance whose time source is NTP
#define TOKI_TS_MS      120 //general better-than-second accuracy time source
#define TOKI_TS_NTP     150 //NTP time, simple round trip estimation. Depending on network, mostly +- 50ms accurate
#define TOKI_TS_NTP_P   170 //NTP time with multi-step sync, higher accuracy. Not implemented in WLED

class Toki {
  typedef enum {
    inactive, marked, active
  } TickT;

  public: 
  typedef struct {
    uint32_t sec;
    uint16_t ms;
  } Time;

  private:
    uint32_t fullSecondMillis = 0;
    uint32_t unix = 0;
    TickT tick = TickT::inactive;
    uint8_t timeSrc = TOKI_TS_NONE;

  public:
    void setTime(Time t, uint8_t timeSource = TOKI_TS_MS) {
      fullSecondMillis = millis() - t.ms;
      unix = t.sec;
      timeSrc = timeSource;
    }

    void setTime(uint32_t sec, uint16_t ms=TOKI_NO_MS_ACCURACY, uint8_t timeSource = TOKI_TS_MS) {
      if (ms >= TOKI_NO_MS_ACCURACY) {
        ms = millisecond(); //just keep current ms if not provided
        if (timeSource > 99) timeSource = TOKI_TS_SEC; //lies
      }
      Time t = {sec, ms};
      setTime(t, timeSource);
    }

    Time fromNTP(byte *timestamp) { //ntp timestamp is 8 bytes, 4 bytes second and 4 bytes sub-second fraction
      unsigned long highWord = word(timestamp[0], timestamp[1]);
      unsigned long lowWord = word(timestamp[2], timestamp[3]);
    
      unsigned long unix = highWord << 16 | lowWord;
      if (!unix) return {0,0};
      unix -= YEARS_70; //NTP begins 1900, Unix 1970

      unsigned long frac = word(timestamp[4], timestamp[5]); //65536ths of a second
      frac = (frac*1000) >> 16; //convert to ms
      return {unix, (uint16_t)frac};
    }

    uint16_t millisecond() {
      uint32_t ms = millis() - fullSecondMillis;
      while (ms > 999) {
        ms -= 1000;
        fullSecondMillis += 1000;
        unix++;
        if (tick == TickT::inactive) tick = TickT::marked; //marked, will be active on next loop
      }
      return ms;
    }

    uint32_t second() {
      millisecond();
      return unix;
    }

    //gets the absolute difference between two timestamps in milliseconds
    uint32_t msDifference(const Time &t0, const Time &t1) {
      bool t1BiggerSec = (t1.sec > t0.sec);
      uint32_t secDiff = (t1BiggerSec) ? t1.sec - t0.sec : t0.sec - t1.sec;
      uint32_t t0ms = t0.ms, t1ms = t1.ms;
      if (t1BiggerSec) t1ms += secDiff*1000;
      else t0ms += secDiff*1000;
      uint32_t msDiff = (t1ms > t0ms) ? t1ms - t0ms : t0ms - t1ms;
      return msDiff;
    }

    //return true if t1 is later than t0
    bool isLater(const Time &t0, const Time &t1) {
      if (t1.sec > t0.sec) return true;
      if (t1.sec < t0.sec) return false;
      if (t1.ms  > t0.ms) return true;
      return false;
    }

    void adjust(Time&t, int32_t offset) {
      int32_t secs = offset /1000;
      int32_t ms = offset - secs*1000;
      t.sec += secs;
      int32_t nms = t.ms + ms;
      if (nms > 1000) {nms -= 1000; t.sec++;}
      if (nms < 0) {nms += 1000; t.sec--;}
      t.ms = nms;
    }

    Time getTime() {
      Time t;
      t.ms = millisecond();
      t.sec = unix;
      return t;
    }

    uint8_t getTimeSource() {
      return timeSrc;
    }

    void setTick() {
      if (tick == TickT::marked) tick = TickT::active;
    }

    void resetTick() {
      if (tick == TickT::active) tick = TickT::inactive;
    }

    bool isTick() {
      return (tick == TickT::active);
    }

    void printTime(const Time& t, Print &dest = Serial) {
      dest.printf_P(PSTR("%u,%03u\n"),t.sec,t.ms);
    }
};