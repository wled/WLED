/*
* ESPAsyncE131.h
*
* Project: ESPAsyncE131 - Asynchronous E.131 (sACN) library for Arduino ESP8266 and ESP32
* Copyright (c) 2019 Shelby Merrick
* http://www.forkineye.com
*
*  Project: ESPAsyncDDP - Asynchronous DDP library for Arduino ESP8266 and ESP32
* Copyright (c) 2019 Daniel Kulp
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
*/

/*
 * Inspired by https://github.com/hideakitai/ArtNet for ArtNet support
 */

#ifndef ESPASYNCE131_H_
#define ESPASYNCE131_H_

#ifdef ESP32
#include <WiFi.h>
#include <AsyncUDP.h>
#elif defined (ESP8266)
#include <ESPAsyncUDP.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#else
#error Platform not supported
#endif

#include <lwip/ip_addr.h>
#include <lwip/igmp.h>
#include <Arduino.h>

#if LWIP_VERSION_MAJOR == 1
typedef struct ip_addr ip4_addr_t;
#endif

// Defaults
#define E131_DEFAULT_PORT   5568
#define ARTNET_DEFAULT_PORT 6454
#define DDP_DEFAULT_PORT    4048

#define DDP_PUSH_FLAG 0x01
#define DDP_TIMECODE_FLAG 0x10

#define DDP_TYPE_RGB24  0x0B // 00 001 011 (RGB , 8 bits per channel, 3 channels)
#define DDP_TYPE_RGBW32 0x1B // 00 011 011 (RGBW, 8 bits per channel, 4 channels)

#define ARTNET_OPCODE_OPDMX 0x5000
#define ARTNET_OPCODE_OPPOLL 0x2000
#define ARTNET_OPCODE_OPPOLLREPLY 0x2100

#define P_E131   0
#define P_ARTNET 1
#define P_DDP    2

// E1.31 Packet Offsets
#define E131_ROOT_PREAMBLE_SIZE 0
#define E131_ROOT_POSTAMBLE_SIZE 2
#define E131_ROOT_ID 4
#define E131_ROOT_FLENGTH 16
#define E131_ROOT_VECTOR 18
#define E131_ROOT_CID 22

#define E131_FRAME_FLENGTH 38
#define E131_FRAME_VECTOR 40
#define E131_FRAME_SOURCE 44
#define E131_FRAME_PRIORITY 108
#define E131_FRAME_RESERVED 109
#define E131_FRAME_SEQ 111
#define E131_FRAME_OPT 112
#define E131_FRAME_UNIVERSE 113

#define E131_DMP_FLENGTH 115
#define E131_DMP_VECTOR 117
#define E131_DMP_TYPE 118
#define E131_DMP_ADDR_FIRST 119
#define E131_DMP_ADDR_INC 121
#define E131_DMP_COUNT 123
#define E131_DMP_DATA 125

// E1.31 Packet Structure
typedef union {
    struct { //E1.31 packet
      // Root Layer
      uint16_t preamble_size;
      uint16_t postamble_size;
      uint8_t  acn_id[12];
      uint16_t root_flength;
      uint32_t root_vector;
      uint8_t  cid[16];

      // Frame Layer
      uint16_t frame_flength;
      uint32_t frame_vector;
      uint8_t  source_name[64];
      uint8_t  priority;
      uint16_t reserved;
      uint8_t  sequence_number;
      uint8_t  options;
      uint16_t universe;

      // DMP Layer
      uint16_t dmp_flength;
      uint8_t  dmp_vector;
      uint8_t  type;
      uint16_t first_address;
      uint16_t address_increment;
      uint16_t property_value_count;
      uint8_t  property_values[513];
    } __attribute__((packed));
	
	struct { //Art-Net packet
    uint8_t  art_id[8];
    uint16_t art_opcode;
    uint16_t art_protocol_ver;
    uint8_t  art_sequence_number;
    uint8_t  art_physical;
    uint16_t art_universe;
    uint16_t art_length;

    uint8_t  art_data[512];
  } __attribute__((packed));

  struct { //DDP Header
    uint8_t flags;
    uint8_t sequenceNum;
    uint8_t dataType;
    uint8_t destination;
    uint32_t channelOffset;
    uint16_t dataLen;
    uint8_t data[1];
  } __attribute__((packed));

  /*struct { //DDP Time code Header (unsupported)
    uint8_t flags;
    uint8_t sequenceNum;
    uint8_t dataType;
    uint8_t destination;
    uint32_t channelOffset;
    uint16_t dataLen;
    uint32_t timeCode;
    uint8_t data[1];
  } __attribute__((packed));*/

  uint8_t raw[1458];
} e131_packet_t;

typedef union {
  struct {
    uint8_t reply_id[8];
    uint16_t reply_opcode;
    uint8_t reply_ip[4];
    uint16_t reply_port;
    uint8_t reply_version_h;
    uint8_t reply_version_l;
    uint8_t reply_net_sw;
    uint8_t reply_sub_sw;
    uint8_t reply_oem_h;
    uint8_t reply_oem_l;
    uint8_t reply_ubea_ver;
    uint8_t reply_status_1;
    uint16_t reply_esta_man;
    uint8_t reply_short_name[18];
    uint8_t reply_long_name[64];
    uint8_t reply_node_report[64];
    uint8_t reply_num_ports_h;
    uint8_t reply_num_ports_l;
    uint8_t reply_port_types[4];
    uint8_t reply_good_input[4];
    uint8_t reply_good_output_a[4];
    uint8_t reply_sw_in[4];
    uint8_t reply_sw_out[4];
    uint8_t reply_sw_video;
    uint8_t reply_sw_macro;
    uint8_t reply_sw_remote;
    uint8_t reply_spare[3];
    uint8_t reply_style;
    uint8_t reply_mac[6];
    uint8_t reply_bind_ip[4];
    uint8_t reply_bind_index;
    uint8_t reply_status_2;
    uint8_t reply_good_output_b[4];
    uint8_t reply_status_3;
    uint8_t reply_filler[21];
  } __attribute__((packed));
  
  uint8_t raw[239];
} ArtPollReply;

// new packet callback
typedef void (*e131_packet_callback_function) (e131_packet_t* p, IPAddress clientIP, byte protocol);

class ESPAsyncE131 {
 private:
    // Constants for packet validation
    static const uint8_t ACN_ID[];
	  static const uint8_t ART_ID[];
    static const uint32_t VECTOR_ROOT = 4;
    static const uint32_t VECTOR_FRAME = 2;
    static const uint8_t VECTOR_DMP = 2;

    AsyncUDP        udp;        // AsyncUDP

    // Internal Initializers
    bool initUnicast(uint16_t port);
    bool initMulticast(uint16_t port, uint16_t universe, uint8_t n = 1);

    // Packet parser callback
    void parsePacket(AsyncUDPPacket _packet);
    
    e131_packet_callback_function _callback = nullptr;

 public:
    ESPAsyncE131(e131_packet_callback_function callback);

    // Generic UDP listener, no physical or IP configuration
    bool begin(bool multicast, uint16_t port = E131_DEFAULT_PORT, uint16_t universe = 1, uint8_t n = 1);
};

// Class to track e131 package priority
class E131Priority {
  private:
    uint8_t priority;
    time_t setupTime;
    uint8_t seconds;
  
  public:
    E131Priority(uint8_t timeout=3) { 
      seconds = timeout;
      set(0);
    };

    // Set priority (+ remember time)
    void set(uint8_t prio) {
      setupTime = time(0);
      priority = prio;
    }

    // Get priority (+ reset & return 0 if older timeout)
    uint8_t get() {
      if (time(0) > setupTime + seconds) priority = 0;
      return priority;
    }
};

#endif  // ESPASYNCE131_H_