#ifndef WLED_NODESTRUCT_H
#define WLED_NODESTRUCT_H

/*********************************************************************************************\
* NodeStruct from the ESP Easy project (https://github.com/letscontrolit/ESPEasy)
\*********************************************************************************************/

#include <map>
#include <IPAddress.h>

#define NODE_TYPE_ID_UNDEFINED        0
#define NODE_TYPE_ID_ESP8266         82 // should be 1
#define NODE_TYPE_ID_ESP32           32 // should be 2
#define NODE_TYPE_ID_ESP32S2         33 // etc
#define NODE_TYPE_ID_ESP32S3         34
#define NODE_TYPE_ID_ESP32C3         35

// updated node types from the ESP Easy project
// https://github.com/letscontrolit/ESPEasy/blob/mega/src/src/DataTypes/NodeTypeID.h
//#define NODE_TYPE_ID_ESP32        33
//#define NODE_TYPE_ID_ESP32S2      34
//#define NODE_TYPE_ID_ESP32C3      35
//#define NODE_TYPE_ID_ESP32S3      36
#define NODE_TYPE_ID_ESP32C2      37
#define NODE_TYPE_ID_ESP32H2      38
#define NODE_TYPE_ID_ESP32C6      39
#define NODE_TYPE_ID_ESP32C61     40
#define NODE_TYPE_ID_ESP32C5      41
#define NODE_TYPE_ID_ESP32P4      42
#define NODE_TYPE_ID_ESP32P4r3    45
#define NODE_TYPE_ID_ESP32H21     43
#define NODE_TYPE_ID_ESP32H4      44

/*********************************************************************************************\
* NodeStruct
\*********************************************************************************************/
struct NodeStruct
{
  String    nodeName;
  IPAddress ip;
  uint8_t   age;
  union {
    uint8_t nodeType;   // a waste of space as we only have 5 types
    struct {
      uint8_t type : 7; // still a waste of space (4 bits would be enough and future-proof)
      bool    on   : 1;
    };
  };
  uint32_t  build;

  NodeStruct() : age(0), nodeType(0), build(0)
  {
    for (unsigned i = 0; i < 4; ++i) { ip[i] = 0; }
  }
};
typedef std::map<uint8_t, NodeStruct> NodesMap;

#endif // WLED_NODESTRUCT_H
