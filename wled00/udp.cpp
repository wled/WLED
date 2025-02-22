#include "wled.h"

/*
 * UDP sync notifier / Realtime / Hyperion / TPM2.NET
 */

#define UDP_SEG_SIZE 36
#define SEG_OFFSET (41)
#define WLEDPACKETSIZE (41+(MAX_NUM_SEGMENTS*UDP_SEG_SIZE)+0)
#define UDP_IN_MAXSIZE 1472
#define PRESUMED_NETWORK_DELAY 3 //how many ms could it take on avg to reach the receiver? This will be added to transmitted times

typedef struct PartialEspNowPacket {
  uint8_t magic;
  uint8_t packet;
  uint8_t noOfPackets;
  uint8_t data[247];
} partial_packet_t;

void notify(byte callMode, bool followUp)
{
#ifndef WLED_DISABLE_ESPNOW
  if (!udpConnected && !useESPNowSync) return;
#else
  if (!udpConnected) return;
#endif
  if (!syncGroups || !sendNotificationsRT) return;
  switch (callMode)
  {
    case CALL_MODE_INIT:          return;
    case CALL_MODE_DIRECT_CHANGE: if (!notifyDirect) return; break;
    case CALL_MODE_BUTTON:        if (!notifyButton) return; break;
    case CALL_MODE_BUTTON_PRESET: if (!notifyButton) return; break;
    case CALL_MODE_NIGHTLIGHT:    if (!notifyDirect) return; break;
    case CALL_MODE_HUE:           if (!notifyHue)    return; break;
    case CALL_MODE_PRESET_CYCLE:  if (!notifyDirect) return; break;
    case CALL_MODE_ALEXA:         if (!notifyAlexa)  return; break;
    default: return;
  }
  byte udpOut[WLEDPACKETSIZE];  //TODO: optimize size to use only active segments
  Segment& mainseg = strip.getMainSegment();
  udpOut[0] = 0; //0: wled notifier protocol 1: WARLS protocol
  udpOut[1] = callMode;
  udpOut[2] = bri;
  uint32_t col = mainseg.colors[0];
  udpOut[3] = R(col);
  udpOut[4] = G(col);
  udpOut[5] = B(col);
  udpOut[6] = nightlightActive;
  udpOut[7] = nightlightDelayMins;
  udpOut[8] = mainseg.mode;
  udpOut[9] = mainseg.speed;
  udpOut[10] = W(col);
  //compatibilityVersionByte:
  //0: old 1: supports white 2: supports secondary color
  //3: supports FX intensity, 24 byte packet 4: supports transitionDelay 5: sup palette
  //6: supports timebase syncing, 29 byte packet 7: supports tertiary color 8: supports sys time sync, 36 byte packet
  //9: supports sync groups, 37 byte packet 10: supports CCT, 39 byte packet 11: per segment options, variable packet length (40+MAX_NUM_SEGMENTS*3)
  //12: enhanced effect sliders, 2D & mapping options
  udpOut[11] = 12;
  col = mainseg.colors[1];
  udpOut[12] = R(col);
  udpOut[13] = G(col);
  udpOut[14] = B(col);
  udpOut[15] = W(col);
  udpOut[16] = mainseg.intensity;
  udpOut[17] = (transitionDelay >> 0) & 0xFF;
  udpOut[18] = (transitionDelay >> 8) & 0xFF;
  udpOut[19] = mainseg.palette;
  col = mainseg.colors[2];
  udpOut[20] = R(col);
  udpOut[21] = G(col);
  udpOut[22] = B(col);
  udpOut[23] = W(col);

  udpOut[24] = followUp;
  uint32_t t = millis() + strip.timebase;
  udpOut[25] = (t >> 24) & 0xFF;
  udpOut[26] = (t >> 16) & 0xFF;
  udpOut[27] = (t >>  8) & 0xFF;
  udpOut[28] = (t >>  0) & 0xFF;

  //sync system time
  udpOut[29] = toki.getTimeSource();
  Toki::Time tm = toki.getTime();
  uint32_t unix = tm.sec;
  udpOut[30] = (unix >> 24) & 0xFF;
  udpOut[31] = (unix >> 16) & 0xFF;
  udpOut[32] = (unix >>  8) & 0xFF;
  udpOut[33] = (unix >>  0) & 0xFF;
  uint16_t ms = tm.ms;
  udpOut[34] = (ms >> 8) & 0xFF;
  udpOut[35] = (ms >> 0) & 0xFF;

  //sync groups
  udpOut[36] = syncGroups;

  //Might be changed to Kelvin in the future, receiver code should handle that case
  //0: byte 38 contains 0-255 value, 255: no valid CCT, 1-254: Kelvin value MSB
  udpOut[37] = strip.hasCCTBus() ? 0 : 255; //check this is 0 for the next value to be significant
  udpOut[38] = mainseg.cct;

  udpOut[39] = strip.getActiveSegmentsNum();
  udpOut[40] = UDP_SEG_SIZE; //size of each loop iteration (one segment)
  size_t s = 0, nsegs = strip.getSegmentsNum();
  for (size_t i = 0; i < nsegs; i++) {
    Segment &selseg = strip.getSegment(i);
    if (!selseg.isActive()) continue;
    unsigned ofs = 41 + s*UDP_SEG_SIZE; //start of segment offset byte
    udpOut[0 +ofs] = s;
    udpOut[1 +ofs] = selseg.start >> 8;
    udpOut[2 +ofs] = selseg.start & 0xFF;
    udpOut[3 +ofs] = selseg.stop >> 8;
    udpOut[4 +ofs] = selseg.stop & 0xFF;
    udpOut[5 +ofs] = selseg.grouping;
    udpOut[6 +ofs] = selseg.spacing;
    udpOut[7 +ofs] = selseg.offset >> 8;
    udpOut[8 +ofs] = selseg.offset & 0xFF;
    udpOut[9 +ofs] = selseg.options & 0x8F; //only take into account selected, mirrored, on, reversed, reverse_y (for 2D); ignore freeze, reset, transitional
    udpOut[10+ofs] = selseg.opacity;
    udpOut[11+ofs] = selseg.mode;
    udpOut[12+ofs] = selseg.speed;
    udpOut[13+ofs] = selseg.intensity;
    udpOut[14+ofs] = selseg.palette;
    udpOut[15+ofs] = R(selseg.colors[0]);
    udpOut[16+ofs] = G(selseg.colors[0]);
    udpOut[17+ofs] = B(selseg.colors[0]);
    udpOut[18+ofs] = W(selseg.colors[0]);
    udpOut[19+ofs] = R(selseg.colors[1]);
    udpOut[20+ofs] = G(selseg.colors[1]);
    udpOut[21+ofs] = B(selseg.colors[1]);
    udpOut[22+ofs] = W(selseg.colors[1]);
    udpOut[23+ofs] = R(selseg.colors[2]);
    udpOut[24+ofs] = G(selseg.colors[2]);
    udpOut[25+ofs] = B(selseg.colors[2]);
    udpOut[26+ofs] = W(selseg.colors[2]);
    udpOut[27+ofs] = selseg.cct;
    udpOut[28+ofs] = (selseg.options>>8) & 0xFF; //mirror_y, transpose, 2D mapping & sound
    udpOut[29+ofs] = selseg.custom1;
    udpOut[30+ofs] = selseg.custom2;
    udpOut[31+ofs] = selseg.custom3 | (selseg.check1<<5) | (selseg.check2<<6) | (selseg.check3<<7);
    udpOut[32+ofs] = selseg.startY >> 8;    // ATM always 0 as Segment::startY is 8-bit
    udpOut[33+ofs] = selseg.startY & 0xFF;
    udpOut[34+ofs] = selseg.stopY >> 8;     // ATM always 0 as Segment::stopY is 8-bit
    udpOut[35+ofs] = selseg.stopY & 0xFF;
    ++s;
  }

  //uint16_t offs = SEG_OFFSET;
  //next value to be added has index: udpOut[offs + 0]

#ifndef WLED_DISABLE_ESPNOW
  if (enableESPNow && useESPNowSync && statusESPNow == ESP_NOW_STATE_ON) {
    partial_packet_t buffer = {'W', 0, 1, {0}};
    // send global data
    DEBUG_PRINTLN(F("ESP-NOW sending first packet."));
    const size_t bufferSize = sizeof(buffer.data)/sizeof(uint8_t);
    size_t packetSize = 41;
    size_t s0 = 0;
    memcpy(buffer.data, udpOut, packetSize);
    // stuff as many segments in first packet as possible (normally up to 5)
    for (size_t i = 0; packetSize < bufferSize && i < s; i++) {
      memcpy(buffer.data + packetSize, &udpOut[41+i*UDP_SEG_SIZE], UDP_SEG_SIZE);
      packetSize += UDP_SEG_SIZE;
      s0++;
    }
    if (s > s0) buffer.noOfPackets += 1 + ((s - s0) * UDP_SEG_SIZE) / bufferSize; // set number of packets
    auto err = quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&buffer), packetSize+3);
    if (!err && s0 < s) {
      // send rest of the segments
      buffer.packet++;
      packetSize = 0;
      // WARNING: this will only work for up to 3 messages (~17 segments) as QuickESPNOW only has a ring buffer capable of holding 3 queued messages
      // to work around that limitation it is mandatory to utilize onDataSent() callback which should reduce number queued messages
      // and wait until at least one space is available in the buffer
      for (size_t i = s0; i < s; i++) {
        memcpy(buffer.data + packetSize, &udpOut[41+i*UDP_SEG_SIZE], UDP_SEG_SIZE);
        packetSize += UDP_SEG_SIZE;
        if (packetSize + UDP_SEG_SIZE < bufferSize) continue;
        DEBUG_PRINTF_P(PSTR("ESP-NOW sending packet: %d (%d)\n"), (int)buffer.packet, packetSize+3);
        err = quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&buffer), packetSize+3);
        buffer.packet++;
        packetSize = 0;
        if (err) break;
      }
      if (!err && packetSize > 0) {
        DEBUG_PRINTF_P(PSTR("ESP-NOW sending last packet: %d (%d)\n"), (int)buffer.packet, packetSize+3);
        err = quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&buffer), packetSize+3);
      }
    }
    if (err) {
      DEBUG_PRINTLN(F("ESP-NOW sending packet failed."));
    }
  }
  if (udpConnected) 
#endif
  {
    DEBUG_PRINTLN(F("UDP sending packet."));
    IPAddress broadcastIp = ~uint32_t(Network.subnetMask()) | uint32_t(Network.gatewayIP());
    notifierUdp.beginPacket(broadcastIp, udpPort);
    notifierUdp.write(udpOut, WLEDPACKETSIZE); // TODO: add actual used buffer size
    notifierUdp.endPacket();
  }
  notificationSentCallMode = callMode;
  notificationSentTime = millis();
  notificationCount = followUp ? notificationCount + 1 : 0;
}

static void parseNotifyPacket(const uint8_t *udpIn) {
  //ignore notification if received within a second after sending a notification ourselves
  if (millis() - notificationSentTime < 1000) return;
  if (udpIn[1] > 199) return; //do not receive custom versions

  //compatibilityVersionByte:
  byte version = udpIn[11];
  DEBUG_PRINTF_P(PSTR("UDP packet version: %d\n"), (int)version);

  // if we are not part of any sync group ignore message
  if (version < 9) {
    // legacy senders are treated as if sending in sync group 1 only
    if (!(receiveGroups & 0x01)) return;
  } else if (!(receiveGroups & udpIn[36])) return;

  bool someSel = (receiveNotificationBrightness || receiveNotificationColor || receiveNotificationEffects || receiveNotificationPalette);

  // set transition time before making any segment changes
  if (version > 3) {
    jsonTransitionOnce = true;
    strip.setTransition(((udpIn[17] << 0) & 0xFF) + ((udpIn[18] << 8) & 0xFF00));
  }

  //apply colors from notification to main segment, only if not syncing full segments
  if ((receiveNotificationColor || !someSel) && (version < 11 || !receiveSegmentOptions)) {
    // primary color, only apply white if intented (version > 0)
    strip.getMainSegment().setColor(0, RGBW32(udpIn[3], udpIn[4], udpIn[5], (version > 0) ? udpIn[10] : 0));
    if (version > 1) {
      strip.getMainSegment().setColor(1, RGBW32(udpIn[12], udpIn[13], udpIn[14], udpIn[15])); // secondary color
    }
    if (version > 6) {
      strip.getMainSegment().setColor(2, RGBW32(udpIn[20], udpIn[21], udpIn[22], udpIn[23])); // tertiary color
      if (version > 9 && udpIn[37] < 255) { // valid CCT/Kelvin value
        unsigned cct = udpIn[38];
        if (udpIn[37] > 0) { //Kelvin
          cct |= (udpIn[37] << 8);
        }
        strip.setCCT(cct);
      }
    }
  }

  bool timebaseUpdated = false;
  //apply effects from notification
  bool applyEffects = (receiveNotificationEffects || !someSel);
  if (applyEffects && currentPlaylist >= 0) unloadPlaylist();
  if (version > 10 && (receiveSegmentOptions || receiveSegmentBounds)) {
    unsigned numSrcSegs = udpIn[39];
    DEBUG_PRINTF_P(PSTR("UDP segments: %d\n"), numSrcSegs);
    // are we syncing bounds and slave has more active segments than master?
    if (receiveSegmentBounds && numSrcSegs < strip.getActiveSegmentsNum()) {
      DEBUG_PRINTLN(F("Removing excessive segments."));
      strip.suspend(); //should not be needed as UDP handling is not done in ISR callbacks but still added "just in case"
      for (size_t i=strip.getSegmentsNum(); i>numSrcSegs && i>0; i--) {
        Segment &seg = strip.getSegment(i-1);
        if (seg.isActive()) seg.deactivate(); // delete segment
      }
      strip.resume();
    }
    size_t inactiveSegs = 0;
    for (size_t i = 0; i < numSrcSegs && i < strip.getMaxSegments(); i++) {
      unsigned ofs = 41 + i*udpIn[40]; //start of segment offset byte
      unsigned id = udpIn[0 +ofs];
      DEBUG_PRINTF_P(PSTR("UDP segment received: %u\n"), id);
      if      (id >  strip.getSegmentsNum()) break;
      else if (id == strip.getSegmentsNum()) {
        if (receiveSegmentBounds && id < strip.getMaxSegments()) strip.appendSegment();
        else break;
      }
      DEBUG_PRINTF_P(PSTR("UDP segment check: %u\n"), id);
      Segment& selseg = strip.getSegment(id);
      // if we are not syncing bounds skip unselected segments
      if (selseg.isActive() && !(selseg.isSelected() || receiveSegmentBounds)) continue;
      // ignore segment if it is inactive and we are not syncing bounds
      if (!receiveSegmentBounds) {
        if (!selseg.isActive()) {
          inactiveSegs++;
          DEBUG_PRINTLN(F("Inactive segment."));
          continue;
        } else {
          id += inactiveSegs; // adjust id
        }
      }
      DEBUG_PRINTF_P(PSTR("UDP segment processing: %u\n"), id);

      uint16_t start  = (udpIn[1+ofs] << 8 | udpIn[2+ofs]);
      uint16_t stop   = (udpIn[3+ofs] << 8 | udpIn[4+ofs]);
      uint16_t startY = version > 11 ? (udpIn[32+ofs] << 8 | udpIn[33+ofs]) : 0;
      uint16_t stopY  = version > 11 ? (udpIn[34+ofs] << 8 | udpIn[35+ofs]) : 1;
      uint16_t offset = (udpIn[7+ofs] << 8 | udpIn[8+ofs]);
      if (!receiveSegmentOptions) {
        DEBUG_PRINTF_P(PSTR("Set segment w/o options: %d [%d,%d;%d,%d]\n"), id, (int)start, (int)stop, (int)startY, (int)stopY);
        strip.suspend(); //should not be needed as UDP handling is not done in ISR callbacks but still added "just in case"
        selseg.setGeometry(start, stop, selseg.grouping, selseg.spacing, offset, startY, stopY, selseg.map1D2D);
        strip.resume();
        continue; // we do receive bounds, but not options
      }
      selseg.options = (selseg.options & 0x0071U) | (udpIn[9 +ofs] & 0x0E); // ignore selected, freeze, reset & transitional
      selseg.setOpacity(udpIn[10+ofs]);
      if (applyEffects) {
        DEBUG_PRINTF_P(PSTR("Apply effect: %u\n"), id);
        selseg.setMode(udpIn[11+ofs]);
        selseg.speed     = udpIn[12+ofs];
        selseg.intensity = udpIn[13+ofs];
      }
      if (receiveNotificationPalette || !someSel) {
        DEBUG_PRINTF_P(PSTR("Apply palette: %u\n"), id);
        selseg.palette   = udpIn[14+ofs];
      }
      if (receiveNotificationColor || !someSel) {
        DEBUG_PRINTF_P(PSTR("Apply color: %u\n"), id);
        selseg.setColor(0, RGBW32(udpIn[15+ofs],udpIn[16+ofs],udpIn[17+ofs],udpIn[18+ofs]));
        selseg.setColor(1, RGBW32(udpIn[19+ofs],udpIn[20+ofs],udpIn[21+ofs],udpIn[22+ofs]));
        selseg.setColor(2, RGBW32(udpIn[23+ofs],udpIn[24+ofs],udpIn[25+ofs],udpIn[26+ofs]));
        selseg.setCCT(udpIn[27+ofs]);
      }
      if (version > 11) {
        // when applying synced options ignore selected as it may be used as indicator of which segments to sync
        // freeze, reset should never be synced
        // LSB to MSB: select, reverse, on, mirror, freeze, reset, reverse_y, mirror_y, transpose, map1d2d (3), ssim (2), set (2)
        DEBUG_PRINTF_P(PSTR("Apply options: %u\n"), id);
        selseg.options = (selseg.options & 0b0000000000110001U) | (udpIn[28+ofs]<<8) | (udpIn[9 +ofs] & 0b11001110U); // ignore selected, freeze, reset
        if (applyEffects) {
          DEBUG_PRINTF_P(PSTR("Apply sliders: %u\n"), id);
          selseg.custom1 = udpIn[29+ofs];
          selseg.custom2 = udpIn[30+ofs];
          selseg.custom3 = udpIn[31+ofs] & 0x1F;
          selseg.check1  = (udpIn[31+ofs]>>5) & 0x1;
          selseg.check1  = (udpIn[31+ofs]>>6) & 0x1;
          selseg.check1  = (udpIn[31+ofs]>>7) & 0x1;
        }
      }
      if (receiveSegmentBounds) {
        DEBUG_PRINTF_P(PSTR("Set segment w/ options: %d [%d,%d;%d,%d]\n"), id, (int)start, (int)stop, (int)startY, (int)stopY);
        strip.suspend(); //should not be needed as UDP handling is not done in ISR callbacks but still added "just in case"
        selseg.setGeometry(start, stop, udpIn[5+ofs], udpIn[6+ofs], offset, startY, stopY, selseg.map1D2D);
        strip.resume();
      } else {
        DEBUG_PRINTF_P(PSTR("Set segment grouping: %d [%d,%d]\n"), id, (int)udpIn[5+ofs], (int)udpIn[6+ofs]);
        strip.suspend(); //should not be needed as UDP handling is not done in ISR callbacks but still added "just in case"
        selseg.setGeometry(selseg.start, selseg.stop, udpIn[5+ofs], udpIn[6+ofs], selseg.offset, selseg.startY, selseg.stopY, selseg.map1D2D);
        strip.resume();
      }
    }
    stateChanged = true;
  }

  // simple effect sync, applies to all selected segments
  if ((applyEffects || receiveNotificationPalette) && (version < 11 || !receiveSegmentOptions)) {
    for (size_t i = 0; i < strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive() || !seg.isSelected()) continue;
      if (applyEffects) {
        seg.setMode(udpIn[8]);
        seg.speed = udpIn[9];
        if (version > 2) seg.intensity = udpIn[16];
      }
      if (version > 4 && receiveNotificationPalette) seg.setPalette(udpIn[19]);
    }
    stateChanged = true;
  }

  if (applyEffects && version > 5) {
    uint32_t t = (udpIn[25] << 24) | (udpIn[26] << 16) | (udpIn[27] << 8) | (udpIn[28]);
    t += PRESUMED_NETWORK_DELAY; //adjust trivially for network delay
    t -= millis();
    strip.timebase = t;
    timebaseUpdated = true;
  }

  //adjust system time, but only if sender is more accurate than self
  if (version > 7) {
    Toki::Time tm;
    tm.sec = (udpIn[30] << 24) | (udpIn[31] << 16) | (udpIn[32] << 8) | (udpIn[33]);
    tm.ms = (udpIn[34] << 8) | (udpIn[35]);
    if (udpIn[29] > toki.getTimeSource()) { //if sender's time source is more accurate
      toki.adjust(tm, PRESUMED_NETWORK_DELAY); //adjust trivially for network delay
      uint8_t ts = TOKI_TS_UDP;
      if (udpIn[29] > 99) ts = TOKI_TS_UDP_NTP;
      else if (udpIn[29] >= TOKI_TS_SEC) ts = TOKI_TS_UDP_SEC;
      toki.setTime(tm, ts);
    } else if (timebaseUpdated && toki.getTimeSource() > 99) { //if we both have good times, get a more accurate timebase
      Toki::Time myTime = toki.getTime();
      uint32_t diff = toki.msDifference(tm, myTime);
      strip.timebase -= PRESUMED_NETWORK_DELAY; //no need to presume, use difference between NTP times at send and receive points
      if (toki.isLater(tm, myTime)) {
        strip.timebase += diff;
      } else {
        strip.timebase -= diff;
      }
    }
  }

  nightlightActive = udpIn[6];
  if (nightlightActive) nightlightDelayMins = udpIn[7];

  if (receiveNotificationBrightness || !someSel) bri = udpIn[2];
  stateUpdated(CALL_MODE_NOTIFICATION);
}

void realtimeLock(uint32_t timeoutMs, byte md)
{
  if (!realtimeMode && !realtimeOverride) {
    unsigned stop, start;
    if (useMainSegmentOnly) {
      Segment& mainseg = strip.getMainSegment();
      start = mainseg.start;
      stop  = mainseg.stop;
      mainseg.freeze = true;
      // if WLED was off and using main segment only, freeze non-main segments so they stay off
      if (bri == 0) {
        for (size_t s = 0; s < strip.getSegmentsNum(); s++) {
          strip.getSegment(s).freeze = true;
        }
      }
    } else {
      start = 0;
      stop  = strip.getLengthTotal();
    }
    // clear strip/segment
    for (size_t i = start; i < stop; i++) strip.setPixelColor(i,BLACK);
  }
  // if strip is off (bri==0) and not already in RTM
  if (briT == 0 && !realtimeMode && !realtimeOverride) {
    strip.setBrightness(scaledBri(briLast), true);
  }

  if (realtimeTimeout != UINT32_MAX) {
    realtimeTimeout = (timeoutMs == 255001 || timeoutMs == 65000) ? UINT32_MAX : millis() + timeoutMs;
  }
  realtimeMode = md;

  if (realtimeOverride) return;
  if (arlsForceMaxBri) strip.setBrightness(scaledBri(255), true);
  if (briT > 0 && md == REALTIME_MODE_GENERIC) strip.show();
}

void exitRealtime() {
  if (!realtimeMode) return;
  if (realtimeOverride == REALTIME_OVERRIDE_ONCE) realtimeOverride = REALTIME_OVERRIDE_NONE;
  strip.setBrightness(scaledBri(bri), true);
  realtimeTimeout = 0; // cancel realtime mode immediately
  realtimeMode = REALTIME_MODE_INACTIVE; // inform UI immediately
  realtimeIP[0] = 0;
  if (useMainSegmentOnly) { // unfreeze live segment again
    strip.getMainSegment().freeze = false;
  } else {
    strip.show(); // possible fix for #3589
  }
  updateInterfaces(CALL_MODE_WS_SEND);
}


#define TMP2NET_OUT_PORT 65442

void sendTPM2Ack() {
  notifierUdp.beginPacket(notifierUdp.remoteIP(), TMP2NET_OUT_PORT);
  uint8_t response_ack = 0xac;
  notifierUdp.write(&response_ack, 1);
  notifierUdp.endPacket();
}


void handleNotifications()
{
  IPAddress localIP;

  //send second notification if enabled
  if(udpConnected && (notificationCount < udpNumRetries) && ((millis()-notificationSentTime) > 250)){
    notify(notificationSentCallMode,true);
  }

  if (e131NewData && millis() - strip.getLastShow() > 15)
  {
    e131NewData = false;
    strip.show();
  }

  //unlock strip when realtime UDP times out
  if (realtimeMode && millis() > realtimeTimeout) exitRealtime();

  //receive UDP notifications
  if (!udpConnected) return;

  bool isSupp = false;
  size_t packetSize = notifierUdp.parsePacket();
  if (!packetSize && udp2Connected) {
    packetSize = notifier2Udp.parsePacket();
    isSupp = true;
  }

  //hyperion / raw RGB
  if (!packetSize && udpRgbConnected) {
    packetSize = rgbUdp.parsePacket();
    if (packetSize) {
      if (!receiveDirect) return;
      if (packetSize > UDP_IN_MAXSIZE || packetSize < 3) return;
      realtimeIP = rgbUdp.remoteIP();
      DEBUG_PRINTLN(rgbUdp.remoteIP());
      uint8_t lbuf[packetSize];
      rgbUdp.read(lbuf, packetSize);
      realtimeLock(realtimeTimeoutMs, REALTIME_MODE_HYPERION);
      if (realtimeOverride && !(realtimeMode && useMainSegmentOnly)) return;
      unsigned totalLen = strip.getLengthTotal();
      if (useMainSegmentOnly) strip.getMainSegment().beginDraw(); // set up parameters for get/setPixelColor()
      for (size_t i = 0, id = 0; i < packetSize -2 && id < totalLen; i += 3, id++) {
        setRealtimePixel(id, lbuf[i], lbuf[i+1], lbuf[i+2], 0);
      }
      if (!(realtimeMode && useMainSegmentOnly)) strip.show();
      return;
    }
  }

  localIP = Network.localIP();
  //notifier and UDP realtime
  if (!packetSize || packetSize > UDP_IN_MAXSIZE) return;
  if (!isSupp && notifierUdp.remoteIP() == localIP) return; //don't process broadcasts we send ourselves

  uint8_t udpIn[packetSize +1];
  unsigned len;
  if (isSupp) len = notifier2Udp.read(udpIn, packetSize);
  else        len =  notifierUdp.read(udpIn, packetSize);

  // WLED nodes info notifications
  if (isSupp && udpIn[0] == 255 && udpIn[1] == 1 && len >= 40) {
    if (!nodeListEnabled || notifier2Udp.remoteIP() == localIP) return;

    unsigned unit = udpIn[39];
    NodesMap::iterator it = Nodes.find(unit);
    if (it == Nodes.end() && Nodes.size() < WLED_MAX_NODES) { // Create a new element when not present
      Nodes[unit].age = 0;
      it = Nodes.find(unit);
    }

    if (it != Nodes.end()) {
      for (size_t x = 0; x < 4; x++) {
        it->second.ip[x] = udpIn[x + 2];
      }
      it->second.age = 0; // reset 'age counter'
      char tmpNodeName[33] = { 0 };
      memcpy(&tmpNodeName[0], reinterpret_cast<byte *>(&udpIn[6]), 32);
      tmpNodeName[32]     = 0;
      it->second.nodeName = tmpNodeName;
      it->second.nodeName.trim();
      it->second.nodeType = udpIn[38];
      uint32_t build = 0;
      if (len >= 44)
        for (size_t i=0; i<sizeof(uint32_t); i++)
          build |= udpIn[40+i]<<(8*i);
      it->second.build = build;
    }
    return;
  }

  //wled notifier, ignore if realtime packets active
  if (udpIn[0] == 0 && !realtimeMode && receiveGroups)
  {
    DEBUG_PRINTF_P(PSTR("UDP notification from: %d.%d.%d.%d\n"), notifierUdp.remoteIP()[0], notifierUdp.remoteIP()[1], notifierUdp.remoteIP()[2], notifierUdp.remoteIP()[3]);
    parseNotifyPacket(udpIn);
    return;
  }

  if (!receiveDirect) return;

  //TPM2.NET
  if (udpIn[0] == 0x9c)
  {
    //WARNING: this code assumes that the final TMP2.NET payload is evenly distributed if using multiple packets (ie. frame size is constant)
    //if the number of LEDs in your installation doesn't allow that, please include padding bytes at the end of the last packet
    byte tpmType = udpIn[1];
    if (tpmType == 0xaa) { //TPM2.NET polling, expect answer
      sendTPM2Ack(); return;
    }
    if (tpmType != 0xda) return; //return if notTPM2.NET data

    realtimeIP = (isSupp) ? notifier2Udp.remoteIP() : notifierUdp.remoteIP();
    realtimeLock(realtimeTimeoutMs, REALTIME_MODE_TPM2NET);
    if (realtimeOverride && !(realtimeMode && useMainSegmentOnly)) return;

    tpmPacketCount++; //increment the packet count
    if (tpmPacketCount == 1) tpmPayloadFrameSize = (udpIn[2] << 8) + udpIn[3]; //save frame size for the whole payload if this is the first packet
    byte packetNum = udpIn[4]; //starts with 1!
    byte numPackets = udpIn[5];

    unsigned id = (tpmPayloadFrameSize/3)*(packetNum-1); //start LED
    unsigned totalLen = strip.getLengthTotal();
    if (useMainSegmentOnly) strip.getMainSegment().beginDraw(); // set up parameters for get/setPixelColor()
    for (size_t i = 6; i < tpmPayloadFrameSize + 4U && id < totalLen; i += 3, id++) {
      setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], 0);
    }
    if (tpmPacketCount == numPackets) { //reset packet count and show if all packets were received
      tpmPacketCount = 0;
      strip.show();
    }
    return;
  }

  //UDP realtime: 1 warls 2 drgb 3 drgbw
  if (udpIn[0] > 0 && udpIn[0] < 5)
  {
    realtimeIP = (isSupp) ? notifier2Udp.remoteIP() : notifierUdp.remoteIP();
    DEBUG_PRINTLN(realtimeIP);
    if (packetSize < 2) return;

    if (udpIn[1] == 0)
    {
      realtimeTimeout = 0;
      return;
    } else {
      realtimeLock(udpIn[1]*1000 +1, REALTIME_MODE_UDP);
    }
    if (realtimeOverride && !(realtimeMode && useMainSegmentOnly)) return;

    unsigned totalLen = strip.getLengthTotal();
    if (useMainSegmentOnly) strip.getMainSegment().beginDraw(); // set up parameters for get/setPixelColor()
    if (udpIn[0] == 1 && packetSize > 5) //warls
    {
      for (size_t i = 2; i < packetSize -3; i += 4)
      {
        setRealtimePixel(udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3], 0);
      }
    } else if (udpIn[0] == 2 && packetSize > 4) //drgb
    {
      for (size_t i = 2, id = 0; i < packetSize -2 && id < totalLen; i += 3, id++)
      {
        setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], 0);
      }
    } else if (udpIn[0] == 3 && packetSize > 6) //drgbw
    {
      for (size_t i = 2, id = 0; i < packetSize -3 && id < totalLen; i += 4, id++)
      {
        setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3]);
      }
    } else if (udpIn[0] == 4 && packetSize > 7) //dnrgb
    {
      unsigned id = ((udpIn[3] << 0) & 0xFF) + ((udpIn[2] << 8) & 0xFF00);
      for (size_t i = 4; i < packetSize -2 && id < totalLen; i += 3, id++)
      {
        setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], 0);
      }
    } else if (udpIn[0] == 5 && packetSize > 8) //dnrgbw
    {
      unsigned id = ((udpIn[3] << 0) & 0xFF) + ((udpIn[2] << 8) & 0xFF00);
      for (size_t i = 4; i < packetSize -2 && id < totalLen; i += 4, id++)
      {
        setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3]);
      }
    }
    strip.show();
    return;
  }

  // API over UDP
  udpIn[packetSize] = '\0';

  if (requestJSONBufferLock(18)) {
    if (udpIn[0] >= 'A' && udpIn[0] <= 'Z') { //HTTP API
      String apireq = "win"; apireq += '&'; // reduce flash string usage
      apireq += (char*)udpIn;
      handleSet(nullptr, apireq);
    } else if (udpIn[0] == '{') { //JSON API
      DeserializationError error = deserializeJson(*pDoc, udpIn);
      JsonObject root = pDoc->as<JsonObject>();
      if (!error && !root.isNull()) deserializeState(root);
    }
    releaseJSONBufferLock();
  }
}


void setRealtimePixel(uint16_t i, byte r, byte g, byte b, byte w)
{
  unsigned pix = i + arlsOffset;
  if (pix < strip.getLengthTotal()) {
    if (!arlsDisableGammaCorrection && gammaCorrectCol) {
      r = gamma8(r);
      g = gamma8(g);
      b = gamma8(b);
      w = gamma8(w);
    }
    uint32_t col = RGBW32(r,g,b,w);
    if (useMainSegmentOnly) {
      strip.getMainSegment().setPixelColor(pix, col); // this expects that strip.getMainSegment().beginDraw() has been called in handleNotification()
    } else {
      strip.setPixelColor(pix, col);
    }
  }
}

/*********************************************************************************************\
   Refresh aging for remote units, drop if too old...
\*********************************************************************************************/
void refreshNodeList()
{
  for (NodesMap::iterator it = Nodes.begin(); it != Nodes.end();) {
    bool mustRemove = true;

    if (it->second.ip[0] != 0) {
      if (it->second.age < 10) {
        it->second.age++;
        mustRemove = false;
        ++it;
      }
    }

    if (mustRemove) {
      it = Nodes.erase(it);
    }
  }
}

/*********************************************************************************************\
   Broadcast system info to other nodes. (to update node lists)
\*********************************************************************************************/
void sendSysInfoUDP()
{
  if (!udp2Connected) return;

  IPAddress ip = Network.localIP();
  if (!ip || ip == IPAddress(255,255,255,255)) ip = IPAddress(4,3,2,1);

  // TODO: make a nice struct of it and clean up
  //  0: 1 byte 'binary token 255'
  //  1: 1 byte id '1'
  //  2: 4 byte ip
  //  6: 32 char name
  // 38: 1 byte node type id
  // 39: 1 byte node id
  // 40: 4 byte version ID
  // 44 bytes total

  // send my info to the world...
  uint8_t data[44] = {0};
  data[0] = 255;
  data[1] = 1;

  for (size_t x = 0; x < 4; x++) {
    data[x + 2] = ip[x];
  }
  memcpy((byte *)data + 6, serverDescription, 32);
  #ifdef ESP8266
  data[38] = NODE_TYPE_ID_ESP8266;
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
  data[38] = NODE_TYPE_ID_ESP32C3;
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
  data[38] = NODE_TYPE_ID_ESP32S3;
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
  data[38] = NODE_TYPE_ID_ESP32S2;
  #elif defined(ARDUINO_ARCH_ESP32)
  data[38] = NODE_TYPE_ID_ESP32;
  #else
  data[38] = NODE_TYPE_ID_UNDEFINED;
  #endif
  if (bri) data[38] |= 0x80U;  // add on/off state
  data[39] = ip[3]; // unit ID == last IP number

  uint32_t build = VERSION;
  for (size_t i=0; i<sizeof(uint32_t); i++)
    data[40+i] = (build>>(8*i)) & 0xFF;

  IPAddress broadcastIP(255, 255, 255, 255);
  notifier2Udp.beginPacket(broadcastIP, udpPort2);
  notifier2Udp.write(data, sizeof(data));
  notifier2Udp.endPacket();
}


/*********************************************************************************************\
 * Art-Net, DDP, E131 output - work in progress
\*********************************************************************************************/

#define DDP_HEADER_LEN 10
#define DDP_SYNCPACKET_LEN 10

#define DDP_FLAGS1_VER 0xc0  // version mask
#define DDP_FLAGS1_VER1 0x40 // version=1
#define DDP_FLAGS1_PUSH 0x01
#define DDP_FLAGS1_QUERY 0x02
#define DDP_FLAGS1_REPLY 0x04
#define DDP_FLAGS1_STORAGE 0x08
#define DDP_FLAGS1_TIME 0x10

#define DDP_ID_DISPLAY 1
#define DDP_ID_CONFIG 250
#define DDP_ID_STATUS 251

// 1440 channels per packet
#define DDP_CHANNELS_PER_PACKET 1440 // 480 leds

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

uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, const uint8_t* buffer, uint8_t bri, bool isRGBW)  {
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

        uint8_t flags = DDP_FLAGS1_VER1;
        if (currentPacket == (packetCount - 1U)) {
          // last packet, set the push flag
          // TODO: determine if we want to send an empty push packet to each destination after sending the pixel data
          flags = DDP_FLAGS1_VER1 | DDP_FLAGS1_PUSH;
          if (channelCount % DDP_CHANNELS_PER_PACKET) {
            packetSize = channelCount % DDP_CHANNELS_PER_PACKET;
          }
        }

        // write the header
        /*0*/ddpUdp.write(flags);
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

#ifndef WLED_DISABLE_ESPNOW
// ESP-NOW message sent callback function
void espNowSentCB(uint8_t* address, uint8_t status) {
    DEBUG_PRINTF_P(PSTR("Message sent to " MACSTR ", status: %d\n"), MAC2STR(address), status);
}

// ESP-NOW message receive callback function
void espNowReceiveCB(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
  sprintf_P(last_signal_src, PSTR("%02x%02x%02x%02x%02x%02x"), address[0], address[1], address[2], address[3], address[4], address[5]);

  #ifdef WLED_DEBUG
    DEBUG_PRINT(F("ESP-NOW: ")); DEBUG_PRINT(last_signal_src); DEBUG_PRINT(F(" -> ")); DEBUG_PRINTLN(len);
    for (int i=0; i<len; i++) DEBUG_PRINTF_P(PSTR("%02x "), data[i]);
    DEBUG_PRINTLN();
  #endif

  // usermods hook can override processing
  if (UsermodManager::onEspNowMessage(address, data, len)) return;

  // handle WiZ Mote data
  if (data[0] == 0x91 || data[0] == 0x81 || data[0] == 0x80) {
    handleWiZdata(data, len);
    return;
  }

  if (strlen(linked_remote) == 12 && strcmp(last_signal_src, linked_remote) != 0) {
    DEBUG_PRINTLN(F("ESP-NOW unpaired remote sender."));
    return;
  }

  partial_packet_t *buffer = reinterpret_cast<partial_packet_t *>(data);
  if (len < 3 || !broadcast || buffer->magic != 'W' || !useESPNowSync || WLED_CONNECTED) {
    DEBUG_PRINTLN(F("ESP-NOW unexpected packet, not syncing or connected to WiFi."));
    return;
  }

  static uint8_t *udpIn = nullptr;
  static uint8_t packetsReceived = 0;
  static uint8_t segsReceived = 0;
  static unsigned long lastProcessed = 0;

  if (buffer->packet == 0) {
    packetsReceived = 0; // it will increment later (this is to make sure we start counting packets correctly)
    if (udpIn == nullptr) {
      udpIn = (uint8_t *)malloc(WLEDPACKETSIZE); // we cannot use stack as we are in callback
      if (!udpIn) return; // memory alocation failed
      DEBUG_PRINTLN(F("ESP-NOW inited UDP buffer."));
    }
    memcpy(udpIn, buffer->data, len-3); // global data (41 bytes + up to 5 segments)
    segsReceived = (len - 3 - 41) / UDP_SEG_SIZE;
  } else if (buffer->packet == packetsReceived && udpIn && ((len - 3) / UDP_SEG_SIZE) * UDP_SEG_SIZE == (len-3)) {
    // we received a packet full of segments
    if (segsReceived >= MAX_NUM_SEGMENTS) {
      // we are already past max segments, just ignore
      DEBUG_PRINTLN(F("ESP-NOW received segments past maximum."));
      len = 3;
    } else if ((segsReceived + ((len - 3) / UDP_SEG_SIZE)) >= MAX_NUM_SEGMENTS) {
      len = ((MAX_NUM_SEGMENTS - segsReceived) * UDP_SEG_SIZE) + 3; // we have reached max number of segments
    }
    if (len > 3) {
      memcpy(udpIn + 41 + (segsReceived * UDP_SEG_SIZE), buffer->data, len-3);
      segsReceived += (len - 3) / UDP_SEG_SIZE;
    }
  } else {
    // any out of order packet or incorrectly sized packet or if we have no UDP buffer will abort
    DEBUG_PRINTF_P(PSTR("ESP-NOW incorrect packet: %d (%d) [%d]\n"), (int)buffer->packet, (int)len-3, (int)UDP_SEG_SIZE);
    if (udpIn) free(udpIn);
    udpIn = nullptr;
    packetsReceived = 0;
    segsReceived = 0;
    return;
  }
  if (!udpIn) return;

  packetsReceived++;
  DEBUG_PRINTF_P(PSTR("ESP-NOW packet received: %d (%d/%d) s:[%d/%d]\n"), (int)buffer->packet, (int)packetsReceived, (int)buffer->noOfPackets, (int)segsReceived, MAX_NUM_SEGMENTS);
  if (packetsReceived >= buffer->noOfPackets) {
    // last packet received
    if (millis() - lastProcessed > 250) {
      DEBUG_PRINTLN(F("ESP-NOW processing complete message."));
      parseNotifyPacket(udpIn);
      lastProcessed = millis();
    } else {
      DEBUG_PRINTLN(F("ESP-NOW ignoring complete message."));
    }
    free(udpIn);
    udpIn = nullptr;
    packetsReceived = 0;
    segsReceived = 0;
  }
}
#endif
