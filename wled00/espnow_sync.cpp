#include "wled.h"
#include "sync.h"

/*
 * ESP-NOW transport for the WLED Notifier sync protocol. Not a protocol of its own -
 * reassembles fragmented packets and hands the result to parseNotifyPacket()
 * (sync_notifier.cpp), the same decoder used by the UDP transport.
 */

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

  bool knownRemote = false;
  for (const auto& mac : linked_remotes) {
    if (strlen(mac.data()) == 12 && strcmp(last_signal_src, mac.data()) == 0) {
      knownRemote = true;
      break;
    }
  }
  if (!knownRemote) {
    DEBUG_PRINT(F("ESP Now Message Received from Unlinked Sender: "));
    DEBUG_PRINTLN(last_signal_src);
    return;
  }

  // handle WiZ Mote data
  if (data[0] == 0x91 || data[0] == 0x81 || data[0] == 0x80) {
    handleWiZdata(data, len);
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
