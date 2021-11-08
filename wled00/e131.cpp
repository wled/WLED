#include "wled.h"

#define MAX_3_CH_LEDS_PER_UNIVERSE 170
#define MAX_4_CH_LEDS_PER_UNIVERSE 128
#define MAX_CHANNELS_PER_UNIVERSE 512

/*
 * E1.31 handler
 */

//DDP protocol support, called by handleE131Packet
//handles RGB data only
void handleDDPPacket(e131_packet_t* p) {
  int lastPushSeq = e131LastSequenceNumber[0];
  
  //reject late packets belonging to previous frame (assuming 4 packets max. before push)
  if (e131SkipOutOfSequence && lastPushSeq) {
    int sn = p->sequenceNum & 0xF;
    if (sn) {
      if (lastPushSeq > 5) {
        if (sn > (lastPushSeq -5) && sn < lastPushSeq) return;
      } else {
        if (sn > (10 + lastPushSeq) || sn < lastPushSeq) return;
      }
    }
  }

  uint32_t start = htonl(p->channelOffset) /3;
  start += DMXAddress /3;
  uint16_t stop = start + htons(p->dataLen) /3;
  uint8_t* data = p->data;
  uint16_t c = 0;
  if (p->flags & DDP_TIMECODE_FLAG) c = 4; //packet has timecode flag, we do not support it, but data starts 4 bytes later

  realtimeLock(realtimeTimeoutMs, REALTIME_MODE_DDP);
  
  if (!realtimeOverride) {
    for (uint16_t i = start; i < stop; i++) {
      setRealtimePixel(i, data[c], data[c+1], data[c+2], 0);
      c+=3;
    }
  }

  bool push = p->flags & DDP_PUSH_FLAG;
  if (push) {
    e131NewData = true;
    byte sn = p->sequenceNum & 0xF;
    if (sn) e131LastSequenceNumber[0] = sn;
  }
}

//E1.31 and Art-Net protocol support
void handleE131Packet(e131_packet_t* p, IPAddress clientIP, byte protocol){

  uint16_t uni = 0, dmxChannels = 0;
  uint8_t* e131_data = nullptr;
  uint8_t seq = 0, mde = REALTIME_MODE_E131;

  if (protocol == P_ARTNET)
  {
    uni = p->art_universe;
    dmxChannels = htons(p->art_length);
    e131_data = p->art_data;
    seq = p->art_sequence_number;
    mde = REALTIME_MODE_ARTNET;
  } else if (protocol == P_E131) {
    uni = htons(p->universe);
    dmxChannels = htons(p->property_value_count) -1;
    e131_data = p->property_values;
    seq = p->sequence_number;
  } else { //DDP
    realtimeIP = clientIP;
    handleDDPPacket(p);
    return;
  }

  #ifdef WLED_ENABLE_DMX
  // does not act on out-of-order packets yet
  if (e131ProxyUniverse > 0 && uni == e131ProxyUniverse) {
    for (uint16_t i = 1; i <= dmxChannels; i++)
      dmx.write(i, e131_data[i]);
    dmx.update();
  }
  #endif

  // only listen for universes we're handling & allocated memory
  if (uni >= (e131Universe + E131_MAX_UNIVERSE_COUNT)) return;

  uint8_t previousUniverses = uni - e131Universe;

  if (e131SkipOutOfSequence)
    if (seq < e131LastSequenceNumber[uni-e131Universe] && seq > 20 && e131LastSequenceNumber[uni-e131Universe] < 250){
      DEBUG_PRINT("skipping E1.31 frame (last seq=");
      DEBUG_PRINT(e131LastSequenceNumber[uni-e131Universe]);
      DEBUG_PRINT(", current seq=");
      DEBUG_PRINT(seq);
      DEBUG_PRINT(", universe=");
      DEBUG_PRINT(uni);
      DEBUG_PRINTLN(")");
      return;
    }
  e131LastSequenceNumber[uni-e131Universe] = seq;

  // update status info
  realtimeIP = clientIP;
  byte wChannel = 0;
  uint16_t totalLen = strip.getLengthTotal();

  switch (DMXMode) {
    case DMX_MODE_DISABLED:
      return;  // nothing to do
      break;

    case DMX_MODE_SINGLE_RGB:
      if (uni != e131Universe) return;
      if (dmxChannels-DMXAddress+1 < 3) return;
      realtimeLock(realtimeTimeoutMs, mde);
      if (realtimeOverride) return;
      wChannel = (dmxChannels-DMXAddress+1 > 3) ? e131_data[DMXAddress+3] : 0;
      for (uint16_t i = 0; i < totalLen; i++)
        setRealtimePixel(i, e131_data[DMXAddress+0], e131_data[DMXAddress+1], e131_data[DMXAddress+2], wChannel);
      break;

    case DMX_MODE_SINGLE_DRGB:
      if (uni != e131Universe) return;
      if (dmxChannels-DMXAddress+1 < 4) return;
      realtimeLock(realtimeTimeoutMs, mde);
      if (realtimeOverride) return;
      wChannel = (dmxChannels-DMXAddress+1 > 4) ? e131_data[DMXAddress+4] : 0;
      if (DMXOldDimmer != e131_data[DMXAddress+0]) {
        DMXOldDimmer = e131_data[DMXAddress+0];
        bri = e131_data[DMXAddress+0];
        strip.setBrightness(bri);
      }
      for (uint16_t i = 0; i < totalLen; i++)
        setRealtimePixel(i, e131_data[DMXAddress+1], e131_data[DMXAddress+2], e131_data[DMXAddress+3], wChannel);
      break;

    case DMX_MODE_EFFECT:
      if (uni != e131Universe) return;
      if (dmxChannels-DMXAddress+1 < 11) return;
      if (DMXOldDimmer != e131_data[DMXAddress+0]) {
        DMXOldDimmer = e131_data[DMXAddress+0];
        bri = e131_data[DMXAddress+0];
      }
      if (e131_data[DMXAddress+1] < MODE_COUNT)
        effectCurrent = e131_data[DMXAddress+ 1];
      effectSpeed     = e131_data[DMXAddress+ 2];  // flickers
      effectIntensity = e131_data[DMXAddress+ 3];
      effectPalette   = e131_data[DMXAddress+ 4];
      col[0]          = e131_data[DMXAddress+ 5];
      col[1]          = e131_data[DMXAddress+ 6];
      col[2]          = e131_data[DMXAddress+ 7];
      colSec[0]       = e131_data[DMXAddress+ 8];
      colSec[1]       = e131_data[DMXAddress+ 9];
      colSec[2]       = e131_data[DMXAddress+10];
      if (dmxChannels-DMXAddress+1 > 11)
      {
        col[3]        = e131_data[DMXAddress+11]; //white
        colSec[3]     = e131_data[DMXAddress+12];
      }
      transitionDelayTemp = 0;               // act fast
      colorUpdated(CALL_MODE_NOTIFICATION);  // don't send UDP
      return;                                // don't activate realtime live mode
      break;

    case DMX_MODE_MULTIPLE_DRGB:
    case DMX_MODE_MULTIPLE_RGB:
    case DMX_MODE_MULTIPLE_RGBW:
      {
        realtimeLock(realtimeTimeoutMs, mde);
        bool is4Chan = (DMXMode == DMX_MODE_MULTIPLE_RGBW);
        const uint16_t dmxChannelsPerLed = is4Chan ? 4 : 3;
        const uint16_t ledsPerUniverse = is4Chan ? MAX_4_CH_LEDS_PER_UNIVERSE : MAX_3_CH_LEDS_PER_UNIVERSE;
        if (realtimeOverride) return;
        uint16_t previousLeds, dmxOffset;
        if (previousUniverses == 0) {
          if (dmxChannels-DMXAddress < 1) return;
          dmxOffset = DMXAddress;
          previousLeds = 0;
          // First DMX address is dimmer in DMX_MODE_MULTIPLE_DRGB mode.
          if (DMXMode == DMX_MODE_MULTIPLE_DRGB) {
            strip.setBrightness(e131_data[dmxOffset++]);
          }
        } else {
          // All subsequent universes start at the first channel.
          dmxOffset = (protocol == P_ARTNET) ? 0 : 1;
          uint16_t ledsInFirstUniverse = (MAX_CHANNELS_PER_UNIVERSE - DMXAddress) / dmxChannelsPerLed;
          previousLeds = ledsInFirstUniverse + (previousUniverses - 1) * ledsPerUniverse;
        }
        uint16_t ledsTotal = previousLeds + (dmxChannels - dmxOffset +1) / dmxChannelsPerLed;
        if (!is4Chan) {
          for (uint16_t i = previousLeds; i < ledsTotal; i++) {
            setRealtimePixel(i, e131_data[dmxOffset], e131_data[dmxOffset+1], e131_data[dmxOffset+2], 0);
            dmxOffset+=3;
          }
        } else {
          for (uint16_t i = previousLeds; i < ledsTotal; i++) {
            setRealtimePixel(i, e131_data[dmxOffset], e131_data[dmxOffset+1], e131_data[dmxOffset+2], e131_data[dmxOffset+3]);
            dmxOffset+=4;
          }
        }
        break;
      }
    default:
      DEBUG_PRINTLN(F("unknown E1.31 DMX mode"));
      return;  // nothing to do
      break;
  }

  e131NewData = true;
}
