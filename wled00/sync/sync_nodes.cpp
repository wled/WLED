#include "wled.h"

/*
 * WLED node discovery: lightweight "which other WLED units are on the network" protocol,
 * broadcast on the secondary notifier socket. Unrelated to the main state-sync protocol
 * in sync_notifier.cpp.
 */

// returns true if the packet was a node-info packet (whether or not it was actually processed)
bool parseNodeInfoPacket(const uint8_t *udpIn, unsigned len, bool isSupp, const IPAddress &localIP)
{
  if (!(isSupp && udpIn[0] == 255 && udpIn[1] == 1 && len >= 40)) return false;

  if (!nodeListEnabled || notifier2Udp.remoteIP() == localIP) return true;

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
    memcpy(&tmpNodeName[0], reinterpret_cast<const byte *>(&udpIn[6]), 32);
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
  return true;
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

  IPAddress ip = WLEDNetwork.localIP();
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
  data[38] = uint8_t(WLED_BOARD); // see wled_boards.h
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
