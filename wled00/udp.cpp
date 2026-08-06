#include "wled.h"
#include "sync.h"

/*
 * UDP sync dispatcher.
 *
 * The individual protocols this dispatches to now live in their own files:
 *  - sync_notifier.cpp      WLED's own state-sync protocol (notify/parseNotifyPacket)
 *  - sync_nodes.cpp         node discovery
 *  - realtime.cpp           shared realtime state machine
 *  - realtime_udp.cpp       Hyperion / TPM2.NET / legacy UDP realtime formats
 *  - realtime_broadcast.cpp outbound Art-Net/DDP/E1.31 sender (see e131.cpp for the
 *                           inbound receiver side of those same wire protocols)
 *  - espnow_sync.cpp        ESP-NOW transport for the notifier protocol
 */

void handleNotifications()
{
  IPAddress localIP;

  //send second notification if enabled
  notifyRetryIfNeeded();

  if (e131NewData && millis() - strip.getLastShow() > 15)
  {
    e131NewData = false;
    if (useMainSegmentOnly) strip.trigger();
    else                    strip.show();
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
    if (handleHyperionPacket()) return;
  }

  localIP = WLEDNetwork.localIP();
  //notifier and UDP realtime
  if (!packetSize || packetSize > UDP_IN_MAXSIZE) return;
  if (!isSupp && notifierUdp.remoteIP() == localIP) return; //don't process broadcasts we send ourselves

  uint8_t udpIn[packetSize +1];
  unsigned len;
  if (isSupp) len = notifier2Udp.read(udpIn, packetSize);
  else        len =  notifierUdp.read(udpIn, packetSize);

  // WLED nodes info notifications
  if (parseNodeInfoPacket(udpIn, len, isSupp, localIP)) return;

  //wled notifier, ignore if realtime packets active
  if (udpIn[0] == 0 && !realtimeMode && receiveGroups)
  {
    DEBUG_PRINTF_P(PSTR("UDP notification from: %d.%d.%d.%d\n"), notifierUdp.remoteIP()[0], notifierUdp.remoteIP()[1], notifierUdp.remoteIP()[2], notifierUdp.remoteIP()[3]);
    parseNotifyPacket(udpIn);
    return;
  }

  if (receiveDirect) {
    if (handleDirectRealtimePacket(udpIn, packetSize, isSupp)) return;
  }

  // API over UDP
  udpIn[packetSize] = '\0';

  if (requestJSONBufferLock(JSON_LOCK_NOTIFY)) {
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

  UsermodManager::onUdpPacket(udpIn, packetSize);
}
