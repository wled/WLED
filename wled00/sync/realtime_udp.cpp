#include "wled.h"
#include "sync.h"

/*
 * Direct/legacy realtime pixel-push protocols: Hyperion (raw RGB), TPM2.NET, and WLED's
 * own legacy UDP realtime formats (WARLS/DRGB/DRGBW/DNRGB/DNRGBW). Three unrelated wire
 * formats that share the realtime state machine in realtime.cpp.
 */

#define TMP2NET_OUT_PORT 65442

// TPM2.NET frame-reassembly state - private to this file, no other translation unit
// needs it (previously these were WLED_GLOBAL, a leftover from when this logic lived
// inline inside handleNotifications()).
static uint8_t  tpmPacketCount = 0;
static uint16_t tpmPayloadFrameSize = 0;

static void sendTPM2Ack() {
  notifierUdp.beginPacket(notifierUdp.remoteIP(), TMP2NET_OUT_PORT);
  uint8_t response_ack = 0xac;
  notifierUdp.write(&response_ack, 1);
  notifierUdp.endPacket();
}

// hyperion / raw RGB. Reads its own socket independently of the notifier sockets.
// Returns true if a packet was read (handled or discarded), false if nothing was pending.
bool handleHyperionPacket()
{
  size_t packetSize = rgbUdp.parsePacket();
  if (!packetSize) return false;

  if (!receiveDirect) return true;
  if (packetSize > UDP_IN_MAXSIZE || packetSize < 3) return true;  // packetSize must not exceed buffersize (UDP_IN_MAXSIZE)
  realtimeIP = rgbUdp.remoteIP();
  DEBUG_PRINTLN(rgbUdp.remoteIP());
  uint8_t lbuf[packetSize];
  rgbUdp.read(lbuf, packetSize);
  realtimeLock(realtimeTimeoutMs, REALTIME_MODE_HYPERION);
  if (realtimeOverride) return true;
  unsigned totalLen = strip.getLengthTotal();
  for (size_t i = 0, id = 0; i < packetSize -2 && id < totalLen; i += 3, id++) {
    setRealtimePixel(id, lbuf[i], lbuf[i+1], lbuf[i+2], 0);
  }
  if (useMainSegmentOnly) strip.trigger();
  else                    strip.show();
  return true;
}

// TPM2.NET and legacy WLED UDP realtime (1 warls 2 drgb 3 drgbw 4 dnrgb 5 dnrgbw).
// Returns true if the packet matched one of these formats (and was consumed), false if
// the caller should keep trying other packet interpretations (e.g. the WLED notifier
// protocol or the JSON/HTTP-over-UDP API).
bool handleDirectRealtimePacket(uint8_t *udpIn, size_t packetSize, bool isSupp)
{
  //TPM2.NET
  if (udpIn[0] == 0x9c) {
    //WARNING: this code assumes that the final TMP2.NET payload is evenly distributed if using multiple packets (ie. frame size is constant)
    //if the number of LEDs in your installation doesn't allow that, please include padding bytes at the end of the last packet
    byte tpmType = udpIn[1];
    if (tpmType == 0xaa) { //TPM2.NET polling, expect answer
      sendTPM2Ack(); return true;
    }
    if (tpmType != 0xda) return true; //ignore, not TPM2.NET data

    realtimeIP = (isSupp) ? notifier2Udp.remoteIP() : notifierUdp.remoteIP();
    realtimeLock(realtimeTimeoutMs, REALTIME_MODE_TPM2NET);
    if (realtimeOverride) return true;

    tpmPacketCount++; //increment the packet count
    if (tpmPacketCount == 1) tpmPayloadFrameSize = (udpIn[2] << 8) + udpIn[3]; //save frame size for the whole payload if this is the first packet
    byte packetNum = udpIn[4]; //starts with 1!
    byte numPackets = udpIn[5];

    unsigned id = (tpmPayloadFrameSize/3)*(packetNum-1); //start LED
    unsigned totalLen = strip.getLengthTotal();
    // Clamp to prevent buffer overread: loop accesses up to udpIn[tpmPayloadFrameSize + 5]
    size_t currentPayloadFrameSize = (packetSize >= 5) ? min(tpmPayloadFrameSize, uint16_t(packetSize - 5)) : 0;
    for (size_t i = 6; i < currentPayloadFrameSize + 4U && id < totalLen; i += 3, id++) {
      setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], 0);
    }
    if (tpmPacketCount == numPackets) { //reset packet count and show if all packets were received
      tpmPacketCount = 0;
      if (useMainSegmentOnly) strip.trigger();
      else                    strip.show();
    }
    return true;
  }

  //UDP realtime: 1 warls 2 drgb 3 drgbw 4 dnrgb 5 dnrgbw
  if (udpIn[0] > 0 && udpIn[0] < 6) {
    realtimeIP = (isSupp) ? notifier2Udp.remoteIP() : notifierUdp.remoteIP();
    DEBUG_PRINTLN(realtimeIP);
    if (packetSize < 2) return true;

    if (udpIn[1] == 0) {
      realtimeTimeout = 0; // cancel realtime mode immediately
      return true;
    } else {
      realtimeLock(udpIn[1]*1000 +1, REALTIME_MODE_UDP);
    }
    if (realtimeOverride) return true;

    unsigned totalLen = strip.getLengthTotal();
    if (udpIn[0] == 1 && packetSize > 5) { //warls
      for (size_t i = 2; i < packetSize -3; i += 4) {
        setRealtimePixel(udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3], 0);
      }
    } else if (udpIn[0] == 2 && packetSize > 4) { //drgb
      for (size_t i = 2, id = 0; i < packetSize -2 && id < totalLen; i += 3, id++)
        {
          setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], 0);
        }
    } else if (udpIn[0] == 3 && packetSize > 6) { //drgbw
        for (size_t i = 2, id = 0; i < packetSize -3 && id < totalLen; i += 4, id++) {
          setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3]);
        }
    } else if (udpIn[0] == 4 && packetSize > 7) { //dnrgb
      unsigned id = ((udpIn[3] << 0) & 0xFF) + ((udpIn[2] << 8) & 0xFF00);
      for (size_t i = 4; i < packetSize -2 && id < totalLen; i += 3, id++) {
        setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], 0);
      }
    } else if (udpIn[0] == 5 && packetSize > 8) { //dnrgbw
      unsigned id = ((udpIn[3] << 0) & 0xFF) + ((udpIn[2] << 8) & 0xFF00);
      for (size_t i = 4; i < packetSize -2 && id < totalLen; i += 4, id++) {
        setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3]);
      }
    }
    if (useMainSegmentOnly) strip.trigger();
    else                    strip.show();
    return true;
  }

  return false;
}
