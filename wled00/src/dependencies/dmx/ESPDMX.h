// - - - - -
// ESPDMX - A Arduino biblioteca for sending and receiving DMX usando the builtin serial hardware puerto.
// ESPDMX.cpp: Biblioteca implementación archivo
//
// Copyright (C) 2015  Rick <ricardogg95@gmail.com>
// This work is licensed under a GNU style license.
//
// Last change: Marcel Seerig <https://github.com/mseerig>
//
// Documentación and samples are available at https://github.com/Rickgg/ESP-Dmx
// - - - - -

#include <inttypes.h>


#ifndef ESPDMX_h
#define ESPDMX_h

// ---- Methods ----

class DMXESPSerial {
public:
  void init();
  void init(int MaxChan);
  uint8_t read(int Channel);
  void write(int channel, uint8_t value);
  void update();
  void end();
};

#endif
