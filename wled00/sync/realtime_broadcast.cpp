#include "wled.h"

/*********************************************************************************************\
 * Art-Net, DDP, E131 output - work in progress
 * (this is the outbound/sender side; see e131.cpp for the inbound receiver side of these
 *  same wire protocols)
\*********************************************************************************************/

//
// Send real time UDP updates to the specified client
//
// type   - protocol type (0=DDP, 1=E1.31, 2=ArtNet)
// client - the IP address to send to
// length - the number of pixels
// buffer - a buffer of at least length*4 bytes long
// isRGBW - true if the buffer contains 4 components per pixel

static       size_t sequenceNumber = 0; // this needs to be shared across all outputs
static const size_t ART_NET_HEADER_SIZE = 12;
static const byte   ART_NET_HEADER[] PROGMEM = {0x41,0x72,0x74,0x2d,0x4e,0x65,0x74,0x00,0x00,0x50,0x00,0x0e};

uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, const uint8_t *buffer, uint8_t bri, bool isRGBW)  {
  if (!(apActive || interfacesInited) || !client[0] || !length) return 1;  // network not initialised or dummy/unset IP address  031522 ajn added check for ap

  WiFiUDP ddpUdp;

  switch (type) {
    case 0: // DDP
    {
      // calculate the number of UDP packets we need to send
      size_t channelCount = length * (isRGBW? 4:3); // 1 channel for every R,G,B value
      size_t packetCount = ((channelCount-1) / DDP_CHANNELS_PER_PACKET) +1;

      // there are 3 channels per RGB pixel
      uint32_t channel = 0; // TODO: allow specifying the start channel
      // the current position in the buffer
      size_t bufferOffset = 0;

      for (size_t currentPacket = 0; currentPacket < packetCount; currentPacket++) {
        if (sequenceNumber > 15) sequenceNumber = 0;

        if (!ddpUdp.beginPacket(client, DDP_DEFAULT_PORT)) {  // port defined in ESPAsyncE131.h
          //DEBUG_PRINTLN(F("WiFiUDP.beginPacket returned an error"));
          return 1; // problem
        }

        // the amount of data is AFTER the header in the current packet
        size_t packetSize = DDP_CHANNELS_PER_PACKET;

        uint8_t flags = DDP_FLAGS_VER1;
        if (currentPacket == (packetCount - 1U)) {
          // last packet, set the push flag
          // TODO: determine if we want to send an empty push packet to each destination after sending the pixel data
          flags = DDP_FLAGS_VER1 | DDP_FLAGS_PUSH;
          if (channelCount % DDP_CHANNELS_PER_PACKET) {
            packetSize = channelCount % DDP_CHANNELS_PER_PACKET;
          }
        }

        // write the header
        /*0*/ddpUdp.write(flags);
        // TODO: sequence number should be 1-15 as 0 means "unused", it has no bad consequences other than out of sequence packet may be accepted
        /*1*/ddpUdp.write(sequenceNumber++ & 0x0F); // sequence may be unnecessary unless we are sending twice (as requested in Sync settings)
        /*2*/ddpUdp.write(isRGBW ?  DDP_TYPE_RGBW32 : DDP_TYPE_RGB24);
        /*3*/ddpUdp.write(DDP_ID_DISPLAY);
        // data offset in bytes, 32-bit number, MSB first
        /*4*/ddpUdp.write(0xFF & (channel >> 24));
        /*5*/ddpUdp.write(0xFF & (channel >> 16));
        /*6*/ddpUdp.write(0xFF & (channel >>  8));
        /*7*/ddpUdp.write(0xFF & (channel      ));
        // data length in bytes, 16-bit number, MSB first
        /*8*/ddpUdp.write(0xFF & (packetSize >> 8));
        /*9*/ddpUdp.write(0xFF & (packetSize     ));

        // write the colors, the write write(const uint8_t *buffer, size_t size)
        // function is just a loop internally too
        for (size_t i = 0; i < packetSize; i += (isRGBW?4:3)) {
          ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // R
          ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // G
          ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // B
          if (isRGBW) ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // W
        }

        if (!ddpUdp.endPacket()) {
          //DEBUG_PRINTLN(F("WiFiUDP.endPacket returned an error"));
          return 1; // problem
        }

        channel += packetSize;
      }
    } break;

    case 1: //E1.31
    {
    } break;

    case 2: //ArtNet
    {
      // calculate the number of UDP packets we need to send
      const size_t channelCount = length * (isRGBW?4:3); // 1 channel for every R,G,B,(W?) value
      const size_t ARTNET_CHANNELS_PER_PACKET = isRGBW?512:510; // 512/4=128 RGBW LEDs, 510/3=170 RGB LEDs
      const size_t packetCount = ((channelCount-1)/ARTNET_CHANNELS_PER_PACKET)+1;

      uint32_t channel = 0;
      size_t bufferOffset = 0;

      sequenceNumber++;

      for (size_t currentPacket = 0; currentPacket < packetCount; currentPacket++) {

        if (sequenceNumber > 255) sequenceNumber = 0;

        if (!ddpUdp.beginPacket(client, ARTNET_DEFAULT_PORT)) {
          DEBUG_PRINTLN(F("Art-Net WiFiUDP.beginPacket returned an error"));
          return 1; // borked
        }

        size_t packetSize = ARTNET_CHANNELS_PER_PACKET;

        if (currentPacket == (packetCount - 1U)) {
          // last packet
          if (channelCount % ARTNET_CHANNELS_PER_PACKET) {
            packetSize = channelCount % ARTNET_CHANNELS_PER_PACKET;
          }
        }

        byte header_buffer[ART_NET_HEADER_SIZE];
        memcpy_P(header_buffer, ART_NET_HEADER, ART_NET_HEADER_SIZE);
        ddpUdp.write(header_buffer, ART_NET_HEADER_SIZE); // This doesn't change. Hard coded ID, OpCode, and protocol version.
        ddpUdp.write(sequenceNumber & 0xFF); // sequence number. 1..255
        ddpUdp.write(0x00); // physical - more an FYI, not really used for anything. 0..3
        ddpUdp.write((currentPacket) & 0xFF); // Universe LSB. 1 full packet == 1 full universe, so just use current packet number.
        ddpUdp.write(0x00); // Universe MSB, unused.
        ddpUdp.write(0xFF & (packetSize >> 8)); // 16-bit length of channel data, MSB
        ddpUdp.write(0xFF & (packetSize     )); // 16-bit length of channel data, LSB

        for (size_t i = 0; i < packetSize; i += (isRGBW?4:3)) {
          ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // R
          ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // G
          ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // B
          if (isRGBW) ddpUdp.write(scale8(buffer[bufferOffset++], bri)); // W
        }

        if (!ddpUdp.endPacket()) {
          DEBUG_PRINTLN(F("Art-Net WiFiUDP.endPacket returned an error"));
          return 1; // borked
        }
        channel += packetSize;
      }
    } break;
  }
  return 0;
}
