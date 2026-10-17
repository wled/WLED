#include "wled.h"
#include "sync.h"

/*
 * WLED's own proprietary state-sync protocol ("Notifier"): notify() builds and sends a
 * packet describing this unit's current state; parseNotifyPacket() decodes an incoming
 * packet and applies it. parseNotifyPacket() is transport-agnostic - it is called both
 * from the UDP receive path (udp.cpp) and the ESP-NOW receive path (espnow_sync.cpp).
 */

// notify()'s own send-retry bookkeeping. Private to this file: udp.cpp's dispatcher
// only calls notifyRetryIfNeeded(), it never touches these fields directly.
struct NotifierSendState {
  byte count = 0;
  byte lastCallMode = CALL_MODE_INIT;
  unsigned long lastSentTime = 0;
};
static NotifierSendState notifierSend;

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
  //9: supports sync groups, 37 byte packet 10: supports CCT, 39 byte packet 11: per segment options, variable packet length (40+WS2812FX::getMaxSegments()*3)
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
    const Segment &selseg = strip.getSegment(i);
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
    auto err = wled::espNow.send(ESPNOW_BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&buffer), packetSize+3);
    if (!err && s0 < s) {
      // send rest of the segments
      buffer.packet++;
      packetSize = 0;
      for (size_t i = s0; i < s; i++) {
        memcpy(buffer.data + packetSize, &udpOut[41+i*UDP_SEG_SIZE], UDP_SEG_SIZE);
        packetSize += UDP_SEG_SIZE;
        if (packetSize + UDP_SEG_SIZE < bufferSize) continue;
        DEBUG_PRINTF_P(PSTR("ESP-NOW sending packet: %d (%u)\n"), (int)buffer.packet, packetSize+3);
        err = wled::espNow.send(ESPNOW_BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&buffer), packetSize+3);
        buffer.packet++;
        packetSize = 0;
        if (err) break;
      }
      if (!err && packetSize > 0) {
        DEBUG_PRINTF_P(PSTR("ESP-NOW sending last packet: %d (%d)\n"), (int)buffer.packet, packetSize+3);
        err = wled::espNow.send(ESPNOW_BROADCAST_ADDRESS, reinterpret_cast<const uint8_t*>(&buffer), packetSize+3);
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
    IPAddress broadcastIp = ~uint32_t(WLEDNetwork.subnetMask()) | uint32_t(WLEDNetwork.gatewayIP());
    notifierUdp.beginPacket(broadcastIp, udpPort);
    notifierUdp.write(udpOut, WLEDPACKETSIZE); // TODO: add actual used buffer size
    notifierUdp.endPacket();
  }
  notifierSend.lastCallMode = callMode;
  notifierSend.lastSentTime = millis();
  notifierSend.count = followUp ? notifierSend.count + 1 : 0;
}

// called from udp.cpp's handleNotifications() once per loop; encapsulates the
// send-retry check so the dispatcher doesn't need to know notify()'s internal state.
void notifyRetryIfNeeded()
{
  if (udpConnected && (notifierSend.count < udpNumRetries) && ((millis() - notifierSend.lastSentTime) > 250)) {
    notify(notifierSend.lastCallMode, true);
  }
}

void parseNotifyPacket(const uint8_t *udpIn) {
  //ignore notification if received within a second after sending a notification ourselves
  if (millis() - notifierSend.lastSentTime < 1000) return;
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
    for (size_t i = 0; i < numSrcSegs && i < WS2812FX::getMaxSegments(); i++) {
      unsigned ofs = 41 + i*udpIn[40]; //start of segment offset byte
      if (ofs + 36 > UDP_IN_MAXSIZE) break; // avoid reading outside of array
      unsigned id = udpIn[0 +ofs];
      DEBUG_PRINTF_P(PSTR("UDP segment received: %u\n"), id);
      if      (id >  strip.getSegmentsNum()) break;
      else if (id == strip.getSegmentsNum()) {
        if (receiveSegmentBounds && id < WS2812FX::getMaxSegments()) strip.appendSegment();
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
        selseg.options = (selseg.options & 0b0000000000110001U) | ((uint16_t)udpIn[28+ofs]<<8) | (udpIn[9 +ofs] & 0b11001110U); // ignore selected, freeze, reset
        if (applyEffects) {
          DEBUG_PRINTF_P(PSTR("Apply sliders: %u\n"), id);
          selseg.custom1 = udpIn[29+ofs];
          selseg.custom2 = udpIn[30+ofs];
          selseg.custom3 = udpIn[31+ofs] & 0x1F;
          selseg.check1  = (udpIn[31+ofs]>>5) & 0x1;
          selseg.check2  = (udpIn[31+ofs]>>6) & 0x1;
          selseg.check3  = (udpIn[31+ofs]>>7) & 0x1;
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
