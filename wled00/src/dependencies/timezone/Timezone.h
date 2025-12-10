/*----------------------------------------------------------------------*
 * Arduino Timezone Library v1.0                                        *
 * Jack Christensen Mar 2012                                            *
 *                                                                      *
 * This work is licensed under the Creative Commons Attribution-        *
 * ShareAlike 3.0 Unported License. To view a copy of this license,     *
 * visit http://creativecommons.org/licenses/by-sa/3.0/ or send a       *
 * letter to Creative Commons, 171 Second Street, Suite 300,            *
 * San Francisco, California, 94105, USA.                               *
 *----------------------------------------------------------------------*/ 
 
#ifndef Timezone_h
#define Timezone_h
#if ARDUINO >= 100
#include <Arduino.h> 
#else
#include <WProgram.h> 
#endif

#include "../time/TimeLib.h"      //http://www.arduino.cc/playground/Code/Time

//convenient constants for dstRules
enum week_t {Last, First, Second, Third, Fourth}; 
enum dow_t {Sun=1, Mon, Tue, Wed, Thu, Fri, Sat};
enum month_t {Jan=1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};

//structure to describe rules for when daylight/summer time begins,
//or when standard time begins.
struct TimeChangeRule
{
    uint8_t week;      //First, Second, Third, Fourth, or Last week of the month
    uint8_t dow;       //day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint8_t month;     //1=Jan, 2=Feb, ... 12=Dec
    uint8_t hour;      //0-23
    int16_t offset;        //offset from UTC in minutes
};
        
class Timezone
{
    public:
        Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart);
        Timezone(int address);
        time_t toLocal(time_t utc);
        time_t toLocal(time_t utc, TimeChangeRule **tcr);
        time_t toUTC(time_t local);
        boolean utcIsDST(time_t utc);
        boolean locIsDST(time_t local);
        void readRules(int address);
        void writeRules(int address);

    private:
        void calcTimeChanges(int yr);
        time_t toTime_t(TimeChangeRule r, int yr);
        TimeChangeRule _dst;    //rule for start of dst or summer time for any year
        TimeChangeRule _std;    //rule for start of standard time for any year
        time_t _dstUTC;         //dst start for given/current year, given in UTC
        time_t _stdUTC;         //std time start for given/current year, given in UTC
        time_t _dstLoc;         //dst start for given/current year, given in local time
        time_t _stdLoc;         //std time start for given/current year, given in local time
};
#endif
