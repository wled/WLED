/*
* ESPAsyncE131.cpp
*
* Project: ESPAsyncE131 - Asynchronous E.131 (sACN) library for Arduino ESP8266 and ESP32
* Copyright (c) 2019 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#include "ESPAsyncE131.h"
#include "../network/Network.h"
#include <string.h>

// E1.17 ACN Packet Identifier
const byte ESPAsyncE131::ACN_ID[12] = { 0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00 };

// Art-Net Packet Identifier
const byte ESPAsyncE131::ART_ID[8]  = { 0x41, 0x72, 0x74, 0x2d, 0x4e, 0x65, 0x74, 0x00 };

// Constructor
ESPAsyncE131::ESPAsyncE131(e131_packet_callback_function callback) {
  _callback = callback;
}

/////////////////////////////////////////////////////////
//
// Public begin() members
//
/////////////////////////////////////////////////////////

bool ESPAsyncE131::begin(bool multicast, uint16_t port, uint16_t universe, uint8_t n) {
  bool success = false;

  if (multicast) {
		success = initMulticast(port, universe, n);
	} else {
    success = initUnicast(port);
	}

  return success;
}

/////////////////////////////////////////////////////////
//
// Private init() members
//
/////////////////////////////////////////////////////////

bool ESPAsyncE131::initUnicast(uint16_t port) {
  bool success = false;

  if (udp.listen(port)) {
    udp.onPacket(std::bind(&ESPAsyncE131::parsePacket, this, std::placeholders::_1));
    success = true;
  }
  return success;
}

bool ESPAsyncE131::initMulticast(uint16_t port, uint16_t universe, uint8_t n) {
  bool success = false;

  IPAddress address = IPAddress(239, 255, ((universe >> 8) & 0xff),
    ((universe >> 0) & 0xff));

  if (udp.listenMulticast(address, port)) {
    ip4_addr_t ifaddr;
    ip4_addr_t multicast_addr;

    ifaddr.addr = static_cast<uint32_t>(Network.localIP());
    for (uint8_t i = 1; i < n; i++) {
        multicast_addr.addr = static_cast<uint32_t>(IPAddress(239, 255,
          (((universe + i) >> 8) & 0xff), (((universe + i) >> 0)
          & 0xff)));
      igmp_joingroup(&ifaddr, &multicast_addr);
    }

    udp.onPacket(std::bind(&ESPAsyncE131::parsePacket, this, std::placeholders::_1));

    success = true;
  }
  return success;
}

/////////////////////////////////////////////////////////
//
// Packet parsing - Private
//
/////////////////////////////////////////////////////////

void ESPAsyncE131::parsePacket(AsyncUDPPacket _packet) {
  bool error = false;
  uint8_t protocol = P_E131;

  e131_packet_t *sbuff = reinterpret_cast<e131_packet_t *>(_packet.data());
	
	//E1.31 packet identifier ("ACS-E1.17")
  if (memcmp(sbuff->acn_id, ESPAsyncE131::ACN_ID, sizeof(sbuff->acn_id)))
    protocol = P_ARTNET;
	
	if (protocol == P_ARTNET) {
		if (memcmp(sbuff->art_id, ESPAsyncE131::ART_ID, sizeof(sbuff->art_id)))
			error = true; //not "Art-Net"
		if (sbuff->art_opcode != ARTNET_OPCODE_OPDMX && sbuff->art_opcode != ARTNET_OPCODE_OPPOLL)
			error = true; //not a DMX or poll packet
	} else { //E1.31 error handling
		if (htonl(sbuff->root_vector) != ESPAsyncE131::VECTOR_ROOT)
			error = true;
		if (htonl(sbuff->frame_vector) != ESPAsyncE131::VECTOR_FRAME)
			error = true;
		if (sbuff->dmp_vector != ESPAsyncE131::VECTOR_DMP)
			error = true;
		if (sbuff->property_values[0] != 0)
			error = true;
	} 
  
  if (error && _packet.localPort() == DDP_DEFAULT_PORT) { //DDP packet
    error = false;
    protocol = P_DDP;
  }

  if (!error) {
    _callback(sbuff, _packet.remoteIP(), protocol);
  }
}