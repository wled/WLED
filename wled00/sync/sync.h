/*
 * Shared constants for the WLED Notifier sync protocol and its ESP-NOW transport.
 * Used by sync_notifier.cpp, espnow_sync.cpp, realtime_udp.cpp and udp.cpp.
 */
#ifndef WLED_SYNC_H
#define WLED_SYNC_H

#define UDP_SEG_SIZE 36
#define SEG_OFFSET (41)
static constexpr size_t WLEDPACKETSIZE = 41+(WS2812FX::getMaxSegments()*UDP_SEG_SIZE);  // make sure this is known at compile-time
#define UDP_IN_MAXSIZE 1472
#define PRESUMED_NETWORK_DELAY 3 //how many ms could it take on avg to reach the receiver? This will be added to transmitted times

typedef struct PartialEspNowPacket {
  uint8_t magic;
  uint8_t packet;
  uint8_t noOfPackets;
  uint8_t data[247];
} partial_packet_t;

#endif
