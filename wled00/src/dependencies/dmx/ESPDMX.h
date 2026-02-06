// - - - - -
// ESPDMX - A Arduino library for sending and receiving DMX using the builtin serial hardware port.
// ESPDMX.cpp: Library implementation file
//
// Copyright (C) 2015  Rick <ricardogg95@gmail.com>
// This work is licensed under a GNU style license.
//
// Last change: Marcel Seerig <https://github.com/mseerig>
//
// Documentation and samples are available at https://github.com/Rickgg/ESP-Dmx
// - - - - -

#include <inttypes.h>


#ifndef ESPDMX_h
#define ESPDMX_h


#define dmxMaxChannel  512
#define defaultMax 32

#define DMXSPEED       250000
#define DMXFORMAT      SERIAL_8N2
#define BREAKSPEED     83333
#define BREAKFORMAT    SERIAL_8N1

// ---- Methods ----

class DMXESPSerial {
public:
  void init(int sendPin);
  uint8_t read(int Channel);
  void write(int channel, uint8_t value);
  void update();
  void end();
private:
  int sendPin;
  bool dmxStarted = false;

  //DMX value array and size. Entry 0 will hold startbyte, so we need 512+1 elements
  uint8_t dmxDataStore[dmxMaxChannel+1] = {};
  int channelSize;

};

#endif
