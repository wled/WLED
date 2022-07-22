#pragma once

#include <SPI.h>
#include <wled.h>

#define RADIO_VERSION 1

#include "bluetooth.h"

#define RADIO_SENDPERIOD 1000                       // how often we broadcast, in millisec

class Radio;

typedef uint8_t CommandId;

#define MESSAGE_DATA_MAX_SIZE 25
typedef struct {
  CommandId command;
  byte data[MESSAGE_DATA_MAX_SIZE];
  uint16_t crc = 0;
} RadioMessage;


class MessageReceiver {
  public:

  virtual void onCommand(uint8_t fromId, CommandId command, void *data) {
    // Abstract: subclasses must define
  }
};

uint16_t calculate_crc( byte *data, byte len ) {

  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for ( unsigned int index = 0 ; index < len  ; ++index ) {
    crc = crc_table[( crc ^ data[index] ) & 0x0f] ^ (crc >> 4);
    crc = crc_table[( crc ^ ( data[index] >> 4 )) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc & 65535;
}


void printMessageData(RadioMessage &message, int size) {
  Serial.print(sizeof(message.data));
  Serial.print(F(":"));
  for (unsigned int i = 0; i < sizeof(message.data); i++) {
    if (message.data[i] < 16)
      Serial.print(F("0"));
    Serial.print(message.data[i], HEX);
    Serial.print(F(" "));
  }
  Serial.print(F("["));
  Serial.print(size);
  Serial.print(F("] "));
}

class Radio {
  public:
    bool alive = false;                            // true if radio booted up
    bool reported_no_radio = false;

    BLEMeshNode mesh_node = BLEMeshNode();

    unsigned long radioFailures = 0;
    unsigned long radioRestarts = 0;

  void setup(bool isMaster) {
    if (isMaster)
      mesh_node.reset(4500);
    else
      mesh_node.reset();

    Serial.println(this->alive ? F("Radio: ok") : F("Radio: fail"));
    // Start the radio, but mute & listen for a bit
  }

  void update() {
    mesh_node.update();
  }

  bool sendCommand(uint32_t command, void *data=0, uint8_t size=0, MeshId relayId=0)
  {
    return this->sendCommandFrom(0, command, data, size, relayId);
  }

  bool sendCommandFrom(MeshId id, uint32_t command, void *data=0, uint8_t size=0, MeshId relayId=0)
  {
    bool sent = false;
    if (!this->alive)
      return true;
  
    RadioMessage message;
    if (size > sizeof(message.data)) {
      Serial.println(F("Too big to send"));
      return false;
    }
  
    message.command = command + RADIO_VERSION;
    memset(message.data, 0, sizeof(message.data));
    memcpy(message.data, data, size);
    uint16_t crc = calculate_crc(message.data, sizeof(message.data));
    message.crc = crc;

    Serial.print(F("["));
    Serial.print(mesh_node.ids.id);
    Serial.print(F(": "));
    Serial.print(message.command, HEX);

    mesh_node.broadcast((byte *)&message, sizeof(message));

    Serial.print(sent ? F(" ok] ") : F(" failed] "));
    return true;
  }

  void receiveCommands(MessageReceiver *receiver)
  {
#ifdef USERADIO
    RadioMessage message;
  
    if (!this->alive && !this->reported_no_radio)
    {
      Serial.println(F("No radio"));
      this->reported_no_radio = true;
      return;
    }
    
    // check for incoming data
    while (_radio.hasData())
    {
      _radio.readData(&message);

      // Messages must be from a tube with the current version
      if ((message.command>>12) != RADIO_VERSION)
        return;

      // Ignore relayed messages if we already have a master
      if (message.uplinkId && message.uplinkId <= this->uplinkTubeId)
        return;

      // Filter out corrupt messages
      unsigned long crc = calculate_crc(message.data, sizeof(message.data));
      if (crc != message.crc) {
        // Corrupt packet... ignore it.
        Serial.print(F("Invalid CRC: "));
        Serial.print(message.crc);
        Serial.print(F(" should be "));
        Serial.println(crc);
        continue;
      }

      // If we detect an ID collision, fix it by choosing a new random one
      while (message.tubeId == this->tubeId) {
        Serial.print(F("ID collision!"));
        this->resetId();
      }

      // Ignore messages from a lower ID
      if (message.tubeId < this->tubeId) {
        // Don't need to be noisy about relayed messages
        if (message.uplinkId == 0) {
          Serial.print(F("Ignoring message from "));
          Serial.println(message.tubeId);
        }
        return;
      }

      if (message.tubeId != 255 && message.tubeId > this->uplinkTubeId) {
        // Found a new master!
        this->uplinkTubeId = message.tubeId;
        Serial.print(F("My new uplink is "));
        Serial.println(this->uplinkTubeId);
      }  

      // Process the command
      receiver->onCommand(message.tubeId, message.command & 0xFFF, message.data);

      // Occcasionally relay commands - more frequently if higher ID
      uint8_t r = random8();
      if ((r % 3 == 0) && r < this->tubeId) {
        Serial.print(F(" (relaying as "));
        Serial.print(this->tubeId);
        Serial.print(F(")"));
        message.reluplinkIdayId = message.tubeId;
        message.tubeId = this->tubeId;
        _radio.send(RADIO_TX_ID, &message, sizeof(message), NRFLite::NO_ACK);
      }
      
      Serial.println();
    }
#endif
  }

};
