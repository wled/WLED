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

/* ----- LIBRARIES ----- */
#if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)

#include <Arduino.h>

#include "ESPDMX.h"


void DMXESPSerial::init(int sendPin) {
  this->sendPin = sendPin;
  channelSize = defaultMax;

  Serial1.begin(DMXSPEED);
  pinMode(sendPin, OUTPUT);
  dmxStarted = true;
}


// Function to read DMX data
uint8_t DMXESPSerial::read(int Channel) {
  if (dmxStarted == false) init();

  if (Channel < 1) Channel = 1;
  if (Channel > dmxMaxChannel) Channel = dmxMaxChannel;
  return(dmxDataStore[Channel]);
}

// Function to send DMX data
void DMXESPSerial::write(int Channel, uint8_t value) {
  if (dmxStarted == false) init();

  if (Channel < 1) Channel = 1;
  if (Channel > channelSize) Channel = channelSize;
  if (value < 0) value = 0;
  if (value > 255) value = 255;

  dmxDataStore[Channel] = value;
}

void DMXESPSerial::end() {
  channelSize = 0;
  Serial1.end();
  dmxStarted = false;
}

void DMXESPSerial::update() {
  if (dmxStarted == false) init();

  //Send break
  digitalWrite(sendPin, HIGH);
  Serial1.begin(BREAKSPEED, BREAKFORMAT);
  Serial1.write(0);
  Serial1.flush();
  delay(1);
  Serial1.end();

  //send data
  Serial1.begin(DMXSPEED, DMXFORMAT);
  digitalWrite(sendPin, LOW);
  Serial1.write(dmxDataStore, channelSize);
  Serial1.flush();
  delay(1);
  Serial1.end();
}

// Function to update the DMX bus

#endif
