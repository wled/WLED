#ifndef CHIP_SELECT_H
#define CHIP_SELECT_H

#include "Hardware.h"

/*
 * `digit`s are as defined in Hardware.h, 0 == seconds ones, 5 == hours tens.
 */

class ChipSelect {
private:
  uint8_t digits_map;
  const uint8_t all_on = 0x3F;
  const uint8_t all_off = 0x00;
public:
  ChipSelect() : digits_map(all_off) {}
  
  void update() {
    // Documented in README.md.  Q7 and Q6 are unused. Q5 is Seconds Ones, Q0 is Hours Tens.
    // Q7 is the first bit written, Q0 is the last.  So we push two dummy bits, then start with
    // Seconds Ones and end with Hours Tens.
    // CS is Active Low, but digits_map is 1 for enable, 0 for disable.  So we bit-wise NOT first.

    uint8_t to_shift = (~digits_map) << 2;

    digitalWrite(CSSR_LATCH_PIN, LOW);
    shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, LSBFIRST, to_shift);
    digitalWrite(CSSR_LATCH_PIN, HIGH);
  }

    void begin() 
  {
    pinMode(CSSR_LATCH_PIN, OUTPUT);
    pinMode(CSSR_DATA_PIN, OUTPUT);
    pinMode(CSSR_CLOCK_PIN, OUTPUT);

    digitalWrite(CSSR_DATA_PIN, LOW);
    digitalWrite(CSSR_CLOCK_PIN, LOW);
    digitalWrite(CSSR_LATCH_PIN, LOW);
    update();
  }

  // These speak the indexes defined in Hardware.h.
  // So 0 is disabled, 1 is enabled (even though CS is active low, this gets mapped.)
  // So bit 0 (LSB), is index 0, is SECONDS_ONES
  // Translation to what the 74HC595 uses is done in update()
  void setDigitMap(uint8_t map, bool update_=true)   { digits_map = map; if (update_) update(); }
  uint8_t getDigitMap()                        { return digits_map; }

  // Helper functions
  // Sets just the one digit by digit number
  void setDigit(uint8_t digit, bool update_=true) { setDigitMap(0x01 << digit, update_); }
  void setAll(bool update_=true)                  { setDigitMap(all_on,  update_); }
  void clear(bool update_=true)                   { setDigitMap(all_off, update_); }
  void setSecondsOnes()                           { setDigit(SECONDS_ONES); }
  void setSecondsTens()                           { setDigit(SECONDS_TENS); }
  void setMinutesOnes()                           { setDigit(MINUTES_ONES); }
  void setMinutesTens()                           { setDigit(MINUTES_TENS); }
  void setHoursOnes()                             { setDigit(HOURS_ONES); }
  void setHoursTens()                             { setDigit(HOURS_TENS); }
  bool isSecondsOnes()                            { return ((digits_map & SECONDS_ONES_MAP) > 0); }
  bool isSecondsTens()                            { return ((digits_map & SECONDS_TENS_MAP) > 0); }
  bool isMinutesOnes()                            { return ((digits_map & MINUTES_ONES_MAP) > 0); }
  bool isMinutesTens()                            { return ((digits_map & MINUTES_TENS_MAP) > 0); }
  bool isHoursOnes()                              { return ((digits_map & HOURS_ONES_MAP) > 0); }
  bool isHoursTens()                              { return ((digits_map & HOURS_TENS_MAP) > 0); }
};


#endif // CHIP_SELECT_H
