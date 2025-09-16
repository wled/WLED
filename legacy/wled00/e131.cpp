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
  static bool ddpSeenPush = false;  // have we seen a push yet?
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

  unsigned ddpChannelsPerLed = ((p->dataType & 0b00111000)>>3 == 0b011) ? 4 : 3; // data type 0x1B (formerly 0x1A) is RGBW (type 3, 8 bit/channel)

  uint32_t start =  htonl(p->channelOffset) / ddpChannelsPerLed;
  start += DMXAddress / ddpChannelsPerLed;
  unsigned stop = start + htons(p->dataLen) / ddpChannelsPerLed;
  uint8_t* data = p->data;
  unsigned c = 0;
  if (p->flags & DDP_TIMECODE_FLAG) c = 4; //packet has timecode flag, we do not support it, but data starts 4 bytes later

  if (realtimeMode != REALTIME_MODE_DDP) ddpSeenPush = false; // just starting, no push yet
  realtimeLock(realtimeTimeoutMs, REALTIME_MODE_DDP);

  if (!realtimeOverride || (realtimeMode && useMainSegmentOnly)) {
    if (useMainSegmentOnly) strip.getMainSegment().beginDraw();
    for (unsigned i = start; i < stop; i++, c += ddpChannelsPerLed) {
      setRealtimePixel(i, data[c], data[c+1], data[c+2], ddpChannelsPerLed >3 ? data[c+3] : 0);
    }
  }

  bool push = p->flags & DDP_PUSH_FLAG;
  ddpSeenPush |= push;
  if (!ddpSeenPush || push) { // if we've never seen a push, or this is one, render display
    e131NewData = true;
    int sn = p->sequenceNum & 0xF;
    if (sn) e131LastSequenceNumber[0] = sn;
  }
}

//E1.31 and Art-Net protocol support
void handleE131Packet(e131_packet_t* p, IPAddress clientIP, byte protocol){

  int uni = 0, dmxChannels = 0;
  uint8_t* e131_data = nullptr;
  int seq = 0, mde = REALTIME_MODE_E131;

  if (protocol == P_ARTNET)
  {
    if (p->art_opcode == ARTNET_OPCODE_OPPOLL) {
      handleArtnetPollReply(clientIP);
      return;
    }
    uni = p->art_universe;
    dmxChannels = htons(p->art_length);
    e131_data = p->art_data;
    seq = p->art_sequence_number;
    mde = REALTIME_MODE_ARTNET;
  } else if (protocol == P_E131) {
    // Ignore PREVIEW data (E1.31: 6.2.6)
    if ((p->options & 0x80) != 0) return;
    dmxChannels = htons(p->property_value_count) - 1;
    // DMX level data is zero start code. Ignore everything else. (E1.11: 8.5)
    if (dmxChannels == 0 || p->property_values[0] != 0) return;
    uni = htons(p->universe);
    e131_data = p->property_values;
    seq = p->sequence_number;
    if (e131Priority != 0) {
      if (p->priority < e131Priority ) return;
      // track highest priority & skip all lower priorities
      if (p->priority >= highPriority.get()) highPriority.set(p->priority);
      if (p->priority < highPriority.get()) return;
    }
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
  if (uni < e131Universe || uni >= (e131Universe + E131_MAX_UNIVERSE_COUNT)) return;

  unsigned previousUniverses = uni - e131Universe;

  if (e131SkipOutOfSequence)
    if (seq < e131LastSequenceNumber[previousUniverses] && seq > 20 && e131LastSequenceNumber[previousUniverses] < 250){
      DEBUG_PRINTF_P(PSTR("skipping E1.31 frame (last seq=%d, current seq=%d, universe=%d)\n"), e131LastSequenceNumber[previousUniverses], seq, uni);
      return;
    }
  e131LastSequenceNumber[previousUniverses] = seq;

  // update status info
  realtimeIP = clientIP;

  handleDMXData(uni, dmxChannels, e131_data, mde, previousUniverses);
}

void handleDMXData(uint16_t uni, uint16_t dmxChannels, uint8_t* e131_data, uint8_t mde, uint8_t previousUniverses) {
  byte wChannel = 0;
  unsigned totalLen = strip.getLengthTotal();
  unsigned availDMXLen = 0;
  unsigned dataOffset = DMXAddress;

  // For legacy DMX start address 0 the available DMX length offset is 0
  const unsigned dmxLenOffset = (DMXAddress == 0) ? 0 : 1;

  // Check if DMX start address fits in available channels
  if (dmxChannels >= DMXAddress) {
    availDMXLen = (dmxChannels - DMXAddress) + dmxLenOffset;
  }

  // DMX data in Art-Net packet starts at index 0, for E1.31 at index 1
  if (mde == REALTIME_MODE_ARTNET && dataOffset > 0) {
    dataOffset--;
  }

  switch (DMXMode) {
    case DMX_MODE_DISABLED:
      return;  // nothing to do
      break;

    case DMX_MODE_SINGLE_RGB:   // 3 channel: [R,G,B]
      if (uni != e131Universe) return;
      if (availDMXLen < 3) return;

      realtimeLock(realtimeTimeoutMs, mde);

      if (realtimeOverride && !(realtimeMode && useMainSegmentOnly)) return;

      wChannel = (availDMXLen > 3) ? e131_data[dataOffset+3] : 0;
      if (useMainSegmentOnly) strip.getMainSegment().beginDraw();
      for (unsigned i = 0; i < totalLen; i++)
        setRealtimePixel(i, e131_data[dataOffset+0], e131_data[dataOffset+1], e131_data[dataOffset+2], wChannel);
      break;

    case DMX_MODE_SINGLE_DRGB:  // 4 channel: [Dimmer,R,G,B]
      if (uni != e131Universe) return;
      if (availDMXLen < 4) return;

      realtimeLock(realtimeTimeoutMs, mde);
      if (realtimeOverride && !(realtimeMode && useMainSegmentOnly)) return;
      wChannel = (availDMXLen > 4) ? e131_data[dataOffset+4] : 0;

      if (bri != e131_data[dataOffset+0]) {
        bri = e131_data[dataOffset+0];
        strip.setBrightness(bri, true);
      }

      if (useMainSegmentOnly) strip.getMainSegment().beginDraw();
      for (unsigned i = 0; i < totalLen; i++)
        setRealtimePixel(i, e131_data[dataOffset+1], e131_data[dataOffset+2], e131_data[dataOffset+3], wChannel);
      break;

    case DMX_MODE_PRESET:       // 2 channel: [Dimmer,Preset]
      {
        if (uni != e131Universe || availDMXLen < 2) return;

        // limit max. selectable preset to 250, even though DMX max. val is 255
        int dmxValPreset = (e131_data[dataOffset+1] > 250 ? 250 : e131_data[dataOffset+1]);
        
        // only apply preset if value changed 
        if (dmxValPreset != 0 && dmxValPreset != currentPreset &&  
            // only apply preset if not in playlist, or playlist changed
            (currentPlaylist < 0 || dmxValPreset != currentPlaylist)) { 
          presetCycCurr = dmxValPreset;
          applyPreset(dmxValPreset, CALL_MODE_NOTIFICATION);
        }

        // only change brightness if value changed
        if (bri != e131_data[dataOffset]) {                                        
          bri = e131_data[dataOffset];
          strip.setBrightness(scaledBri(bri), false);
          stateUpdated(CALL_MODE_WS_SEND);
        }
        return;
        break;
      }

    case DMX_MODE_EFFECT:           // 15 channels [bri,effectCurrent,effectSpeed,effectIntensity,effectPalette,effectOption,R,G,B,R2,G2,B2,R3,G3,B3]
    case DMX_MODE_EFFECT_W:         // 18 channels, same as above but with extra +3 white channels [..,W,W2,W3]
    case DMX_MODE_EFFECT_SEGMENT:   // 15 channels per segment;
    case DMX_MODE_EFFECT_SEGMENT_W: // 18 Channels per segment;
      {
        if (uni != e131Universe) return;
        bool isSegmentMode = DMXMode == DMX_MODE_EFFECT_SEGMENT || DMXMode == DMX_MODE_EFFECT_SEGMENT_W;
        unsigned dmxEffectChannels = (DMXMode == DMX_MODE_EFFECT || DMXMode == DMX_MODE_EFFECT_SEGMENT) ? 15 : 18;
        for (unsigned id = 0; id < strip.getSegmentsNum(); id++) {
          Segment& seg = strip.getSegment(id);
          if (isSegmentMode)
            dataOffset = DMXAddress + id * (dmxEffectChannels + DMXSegmentSpacing);
          else
            dataOffset = DMXAddress;
          // Modify address for Art-Net data
          if (mde == REALTIME_MODE_ARTNET && dataOffset > 0)
            dataOffset--;
          // Skip out of universe addresses
          if (dataOffset > dmxChannels - dmxEffectChannels + 1)
            return;

          if (e131_data[dataOffset+1] < strip.getModeCount())
            if (e131_data[dataOffset+1] != seg.mode)      seg.setMode(   e131_data[dataOffset+1]);
          if (e131_data[dataOffset+2]   != seg.speed)     seg.speed     = e131_data[dataOffset+2];      
          if (e131_data[dataOffset+3]   != seg.intensity) seg.intensity = e131_data[dataOffset+3];
          if (e131_data[dataOffset+4]   != seg.palette)   seg.setPalette(e131_data[dataOffset+4]);

          if ((e131_data[dataOffset+5] & 0b00000010) != seg.reverse_y) { seg.setOption(SEG_OPTION_REVERSED_Y, e131_data[dataOffset+5] & 0b00000010); }
          if ((e131_data[dataOffset+5] & 0b00000100) != seg.mirror_y) { seg.setOption(SEG_OPTION_MIRROR_Y, e131_data[dataOffset+5] & 0b00000100); }
          if ((e131_data[dataOffset+5] & 0b00001000) != seg.transpose) { seg.setOption(SEG_OPTION_TRANSPOSED, e131_data[dataOffset+5] & 0b00001000); }
          if ((e131_data[dataOffset+5] & 0b00110000) / 8 != seg.map1D2D) {
            seg.map1D2D = (e131_data[dataOffset+5] & 0b00110000) / 8;
          }
          // To maintain backwards compatibility with prior e1.31 values, reverse is fixed to mask 0x01000000
          if ((e131_data[dataOffset+5] & 0b01000000) != seg.reverse) { seg.setOption(SEG_OPTION_REVERSED, e131_data[dataOffset+5] & 0b01000000); }
          // To maintain backwards compatibility with prior e1.31 values, mirror is fixed to mask 0x10000000
          if ((e131_data[dataOffset+5] & 0b10000000) != seg.mirror) { seg.setOption(SEG_OPTION_MIRROR, e131_data[dataOffset+5] & 0b10000000); }

          uint32_t colors[3];
          byte whites[3] = {0,0,0};
          if (dmxEffectChannels == 18) {
            whites[0] = e131_data[dataOffset+15];
            whites[1] = e131_data[dataOffset+16];
            whites[2] = e131_data[dataOffset+17];
          }
          colors[0] = RGBW32(e131_data[dataOffset+ 6], e131_data[dataOffset+ 7], e131_data[dataOffset+ 8], whites[0]);
          colors[1] = RGBW32(e131_data[dataOffset+ 9], e131_data[dataOffset+10], e131_data[dataOffset+11], whites[1]);
          colors[2] = RGBW32(e131_data[dataOffset+12], e131_data[dataOffset+13], e131_data[dataOffset+14], whites[2]);
          if (colors[0] != seg.colors[0]) seg.setColor(0, colors[0]);
          if (colors[1] != seg.colors[1]) seg.setColor(1, colors[1]);
          if (colors[2] != seg.colors[2]) seg.setColor(2, colors[2]);

          // Set segment opacity or global brightness
          if (isSegmentMode) {
            if (e131_data[dataOffset] != seg.opacity) seg.setOpacity(e131_data[dataOffset]);
          } else if ( id == strip.getSegmentsNum()-1U ) {
            if (bri != e131_data[dataOffset]) {
              bri = e131_data[dataOffset];
              strip.setBrightness(bri, true);
            }
          }
        }
        return;
        break;
      }
      
    case DMX_MODE_MULTIPLE_DRGB:
    case DMX_MODE_MULTIPLE_RGB:
    case DMX_MODE_MULTIPLE_RGBW:
      {
        bool is4Chan = (DMXMode == DMX_MODE_MULTIPLE_RGBW);
        const unsigned dmxChannelsPerLed = is4Chan ? 4 : 3;
        const unsigned ledsPerUniverse = is4Chan ? MAX_4_CH_LEDS_PER_UNIVERSE : MAX_3_CH_LEDS_PER_UNIVERSE;
        uint8_t stripBrightness = bri;
        unsigned previousLeds, dmxOffset, ledsTotal;

        if (previousUniverses == 0) {
          if (availDMXLen < 1) return;
          dmxOffset = dataOffset;
          previousLeds = 0;
          // First DMX address is dimmer in DMX_MODE_MULTIPLE_DRGB mode.
          if (DMXMode == DMX_MODE_MULTIPLE_DRGB) {
            stripBrightness = e131_data[dmxOffset++];
            ledsTotal = (availDMXLen - 1) / dmxChannelsPerLed;
          } else {
            ledsTotal = availDMXLen / dmxChannelsPerLed;
          }
        } else {
          // All subsequent universes start at the first channel.
          dmxOffset = (mde == REALTIME_MODE_ARTNET) ? 0 : 1;
          const unsigned dimmerOffset = (DMXMode == DMX_MODE_MULTIPLE_DRGB) ? 1 : 0;
          unsigned ledsInFirstUniverse = (((MAX_CHANNELS_PER_UNIVERSE - DMXAddress) + dmxLenOffset) - dimmerOffset) / dmxChannelsPerLed;
          previousLeds = ledsInFirstUniverse + (previousUniverses - 1) * ledsPerUniverse;
          ledsTotal = previousLeds + (dmxChannels / dmxChannelsPerLed);
        }

        // All LEDs already have values
        if (previousLeds >= totalLen) {
          return;
        }

        realtimeLock(realtimeTimeoutMs, mde);
        if (realtimeOverride && !(realtimeMode && useMainSegmentOnly)) return;

        if (ledsTotal > totalLen) {
          ledsTotal = totalLen;
        }

        if (DMXMode == DMX_MODE_MULTIPLE_DRGB && previousUniverses == 0) {
          if (bri != stripBrightness) {
            bri = stripBrightness;
            strip.setBrightness(bri, true);
          }
        }

        if (useMainSegmentOnly) strip.getMainSegment().beginDraw();
        if (!is4Chan) {
          for (unsigned i = previousLeds; i < ledsTotal; i++) {
            setRealtimePixel(i, e131_data[dmxOffset], e131_data[dmxOffset+1], e131_data[dmxOffset+2], 0);
            dmxOffset+=3;
          }
        } else {
          for (unsigned i = previousLeds; i < ledsTotal; i++) {
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

void handleArtnetPollReply(IPAddress ipAddress) {
  ArtPollReply artnetPollReply;
  prepareArtnetPollReply(&artnetPollReply);

  unsigned startUniverse = e131Universe;
  unsigned endUniverse = e131Universe;

  switch (DMXMode) {
    case DMX_MODE_DISABLED:
      break;

    case DMX_MODE_SINGLE_RGB:
    case DMX_MODE_SINGLE_DRGB:
    case DMX_MODE_PRESET:
    case DMX_MODE_EFFECT:
    case DMX_MODE_EFFECT_W:
    case DMX_MODE_EFFECT_SEGMENT:
    case DMX_MODE_EFFECT_SEGMENT_W:
      break;  // 1 universe is enough

    case DMX_MODE_MULTIPLE_DRGB:
    case DMX_MODE_MULTIPLE_RGB:
    case DMX_MODE_MULTIPLE_RGBW:
      {
        bool is4Chan = (DMXMode == DMX_MODE_MULTIPLE_RGBW);
        const unsigned dmxChannelsPerLed = is4Chan ? 4 : 3;
        const unsigned dimmerOffset = (DMXMode == DMX_MODE_MULTIPLE_DRGB) ? 1 : 0;
        const unsigned dmxLenOffset = (DMXAddress == 0) ? 0 : 1; // For legacy DMX start address 0
        const unsigned ledsInFirstUniverse = (((MAX_CHANNELS_PER_UNIVERSE - DMXAddress) + dmxLenOffset) - dimmerOffset) / dmxChannelsPerLed;
        const unsigned totalLen = strip.getLengthTotal();

        if (totalLen > ledsInFirstUniverse) {
          const unsigned ledsPerUniverse = is4Chan ? MAX_4_CH_LEDS_PER_UNIVERSE : MAX_3_CH_LEDS_PER_UNIVERSE;
          const unsigned remainLED = totalLen - ledsInFirstUniverse;

          endUniverse += (remainLED / ledsPerUniverse);

          if ((remainLED % ledsPerUniverse) > 0) {
            endUniverse++;
          }

          if ((endUniverse - startUniverse) > E131_MAX_UNIVERSE_COUNT) {
            endUniverse = startUniverse + E131_MAX_UNIVERSE_COUNT - 1;
          }
        }
        break;
      }
    default:
      DEBUG_PRINTLN(F("unknown E1.31 DMX mode"));
      return;  // nothing to do
      break;
  }

  if (DMXMode != DMX_MODE_DISABLED) {
    for (unsigned i = startUniverse; i <= endUniverse; ++i) {
      sendArtnetPollReply(&artnetPollReply, ipAddress, i);
    }
  }

  #ifdef WLED_ENABLE_DMX
    if (e131ProxyUniverse > 0 && (DMXMode == DMX_MODE_DISABLED || (e131ProxyUniverse < startUniverse || e131ProxyUniverse > endUniverse))) {
      sendArtnetPollReply(&artnetPollReply, ipAddress, e131ProxyUniverse);
    }
  #endif
}

void prepareArtnetPollReply(ArtPollReply *reply) {
  // Art-Net
  reply->reply_id[0] = 0x41;
  reply->reply_id[1] = 0x72;
  reply->reply_id[2] = 0x74;
  reply->reply_id[3] = 0x2d;
  reply->reply_id[4] = 0x4e;
  reply->reply_id[5] = 0x65;
  reply->reply_id[6] = 0x74;
  reply->reply_id[7] = 0x00;

  reply->reply_opcode = ARTNET_OPCODE_OPPOLLREPLY;

  IPAddress localIP = Network.localIP();
  for (unsigned i = 0; i < 4; i++) {
    reply->reply_ip[i] = localIP[i];
  }

  reply->reply_port = ARTNET_DEFAULT_PORT;

  char * numberEnd = versionString;
  reply->reply_version_h = (uint8_t)strtol(numberEnd, &numberEnd, 10);
  numberEnd++;
  reply->reply_version_l = (uint8_t)strtol(numberEnd, &numberEnd, 10);

  // Switch values depend on universe, set before sending
  reply->reply_net_sw = 0x00;
  reply->reply_sub_sw = 0x00;

  reply->reply_oem_h = 0x00; // TODO add assigned oem code
  reply->reply_oem_l = 0x00;

  reply->reply_ubea_ver = 0x00;

  // Indicators in Normal Mode
  // All or part of Port-Address programmed by network or Web browser
  reply->reply_status_1 = 0xE0;

  reply->reply_esta_man = 0x0000;

  strlcpy((char *)(reply->reply_short_name), serverDescription, 18);
  strlcpy((char *)(reply->reply_long_name), serverDescription, 64);

  reply->reply_node_report[0] = '\0';

  reply->reply_num_ports_h = 0x00;
  reply->reply_num_ports_l = 0x01; // One output port

  reply->reply_port_types[0] = 0x80; // Output DMX data
  reply->reply_port_types[1] = 0x00;
  reply->reply_port_types[2] = 0x00;
  reply->reply_port_types[3] = 0x00;

  // No inputs
  reply->reply_good_input[0] = 0x00;
  reply->reply_good_input[1] = 0x00;
  reply->reply_good_input[2] = 0x00;
  reply->reply_good_input[3] = 0x00;

  // One output
  reply->reply_good_output_a[0] = 0x80; // Data is being transmitted
  reply->reply_good_output_a[1] = 0x00;
  reply->reply_good_output_a[2] = 0x00;
  reply->reply_good_output_a[3] = 0x00;

  // Values depend on universe, set before sending
  reply->reply_sw_in[0] = 0x00;
  reply->reply_sw_in[1] = 0x00;
  reply->reply_sw_in[2] = 0x00;
  reply->reply_sw_in[3] = 0x00;

  // Values depend on universe, set before sending
  reply->reply_sw_out[0] = 0x00;
  reply->reply_sw_out[1] = 0x00;
  reply->reply_sw_out[2] = 0x00;
  reply->reply_sw_out[3] = 0x00;

  reply->reply_sw_video = 0x00;
  reply->reply_sw_macro = 0x00;
  reply->reply_sw_remote = 0x00;

  reply->reply_spare[0] = 0x00;
  reply->reply_spare[1] = 0x00;
  reply->reply_spare[2] = 0x00;

  // A DMX to / from Art-Net device
  reply->reply_style = 0x00;

  Network.localMAC(reply->reply_mac);

  for (unsigned i = 0; i < 4; i++) {
    reply->reply_bind_ip[i] = localIP[i];
  }

  reply->reply_bind_index = 1;

  // Product supports web browser configuration
  // Node’s IP is DHCP or manually configured
  // Node is DHCP capable
  // Node supports 15 bit Port-Address (Art-Net 3 or 4)
  // Node is able to switch between ArtNet and sACN
  reply->reply_status_2 = (multiWiFi[0].staticIP[0] == 0) ? 0x1F : 0x1D;

  // RDM is disabled
  // Output style is continuous
  reply->reply_good_output_b[0] = 0xC0;
  reply->reply_good_output_b[1] = 0xC0;
  reply->reply_good_output_b[2] = 0xC0;
  reply->reply_good_output_b[3] = 0xC0;

  // Fail-over state: Hold last state
  // Node does not support fail-over
  reply->reply_status_3 = 0x00;

  for (unsigned i = 0; i < 21; i++) {
    reply->reply_filler[i] = 0x00;
  }
}

void sendArtnetPollReply(ArtPollReply *reply, IPAddress ipAddress, uint16_t portAddress) {
  reply->reply_net_sw = (uint8_t)((portAddress >> 8) & 0x007F);
  reply->reply_sub_sw = (uint8_t)((portAddress >> 4) & 0x000F);
  reply->reply_sw_out[0] = (uint8_t)(portAddress & 0x000F);

  snprintf_P((char *)reply->reply_node_report, sizeof(reply->reply_node_report)-1, PSTR("#0001 [%04u] OK - WLED v" TOSTRING(WLED_VERSION)), pollReplyCount);

  if (pollReplyCount < 9999) {
    pollReplyCount++;
  } else {
    pollReplyCount = 0;
  }

  notifierUdp.beginPacket(ipAddress, ARTNET_DEFAULT_PORT);
  notifierUdp.write(reply->raw, sizeof(ArtPollReply));
  notifierUdp.endPacket();

  reply->reply_bind_index++;
}