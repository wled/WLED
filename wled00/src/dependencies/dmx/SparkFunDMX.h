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

// ---- Methods ----

class SparkFunDMX {
public:
  void initRead(int maxChan);
  void initWrite(int maxChan);
  uint8_t read(int Channel);
  void write(int channel, uint8_t value);
  void update();
private:
  uint8_t _startCodeValue = 0xFF;
  bool _READ = true;
  bool _WRITE = false;
  bool _READWRITE;
};

#endif