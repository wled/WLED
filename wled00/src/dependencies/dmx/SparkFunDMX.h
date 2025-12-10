/******************************************************************************
SparkFunDMX.h
Arduino Library for the SparkFun ESP32 LED to DMX Shield
Andy England @ SparkFun Electronics
7/22/2019

Development environment specifics:
Arduino IDE 1.6.4

This code is released under the [MIT License](http://opensource.org/licenses/MIT).
Please review the LICENSE.md file included with this example. If you have any questions 
or concerns with licensing, please contact techsupport@sparkfun.com.
Distributed as-is; no warranty is given.
******************************************************************************/

#include <inttypes.h>


#ifndef SparkFunDMX_h
#define SparkFunDMX_h

#define DMX_SEND_ONLY // this disables DMX sending features, and saves us two GPIO pins

// ---- Methods ----

class SparkFunDMX {
public:
  void initWrite(int maxChan);
#if !defined(DMX_SEND_ONLY)
  void initRead(int maxChan);
  uint8_t read(int Channel);
#endif
  void write(int channel, uint8_t value);
  void update();
private:
  const uint8_t _startCodeValue = 0xFF;
  const bool _READ = true;
  const bool _WRITE = false;
  bool _READWRITE;
};

#endif