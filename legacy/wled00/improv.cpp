#include "wled.h"

#ifdef WLED_DEBUG_IMPROV
  #define DIMPROV_PRINT(x) Serial.print(x)
  #define DIMPROV_PRINTLN(x) Serial.println(x)
  #define DIMPROV_PRINTF(x...) Serial.printf(x)
#else
  #define DIMPROV_PRINT(x)
  #define DIMPROV_PRINTLN(x)
  #define DIMPROV_PRINTF(x...)
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
#undef WLED_DISABLE_IMPROV_WIFISCAN
#define WLED_DISABLE_IMPROV_WIFISCAN
#endif

#define IMPROV_VERSION 1

void parseWiFiCommand(char *rpcData);

enum ImprovPacketType {
  Current_State = 0x01,
  Error_State = 0x02,
  RPC_Command = 0x03,
  RPC_Response = 0x04
};

enum ImprovPacketByte {
  Version = 6,
  PacketType = 7,
  Length = 8,
  RPC_CommandType = 9
};

#ifndef WLED_DISABLE_IMPROV_WIFISCAN
static bool improvWifiScanRunning = false;
#endif

//blocking function to parse an Improv Serial packet
void handleImprovPacket() {
  uint8_t header[6] = {'I','M','P','R','O','V'};

  bool timeout = false;
  unsigned waitTime = 25;
  unsigned packetByte = 0;
  unsigned packetLen = 9;
  unsigned checksum = 0;

  unsigned rpcCommandType = 0;
  char rpcData[128];
  rpcData[0] = 0;

  while (!timeout) {
    if (Serial.available() < 1) {
      delay(1);
      waitTime--;
      if (!waitTime) timeout = true;
      continue;
    }
    byte next = Serial.read();

    DIMPROV_PRINT("Received improv byte: "); DIMPROV_PRINTF("%x\r\n",next);

    switch (packetByte) {
      case ImprovPacketByte::Version: {
        if (next != IMPROV_VERSION) {
          DIMPROV_PRINTLN(F("Invalid version"));
          return;
        }
        break;
      }
      case ImprovPacketByte::PacketType: {
        if (next != ImprovPacketType::RPC_Command) {
          DIMPROV_PRINTF("Non RPC-command improv packet type %i\n",next);
          return;
        }
        if (!improvActive) improvActive = 1;
        break;
      }
      case ImprovPacketByte::Length: packetLen = 9 + next; break;
      case ImprovPacketByte::RPC_CommandType: rpcCommandType = next; break;
      default: {
        if (packetByte >= packetLen) { //end of packet, check checksum match

          if (checksum != next) {
            DIMPROV_PRINTF("Got RPC checksum %i, expected %i",next,checksum);
            sendImprovStateResponse(0x01, true);
            return;
          }

          switch (rpcCommandType) {
            case ImprovRPCType::Command_Wifi: parseWiFiCommand(rpcData); break;
            case ImprovRPCType::Request_State: {
              unsigned improvState = 0x02; //authorized
              if (WLED_WIFI_CONFIGURED) improvState = 0x03; //provisioning
              if (Network.isConnected()) improvState = 0x04; //provisioned
              sendImprovStateResponse(improvState, false);
              if (improvState == 0x04) sendImprovIPRPCResult(ImprovRPCType::Request_State);
              break;
            }
            case ImprovRPCType::Request_Info: sendImprovInfoResponse(); break;
            #ifndef WLED_DISABLE_IMPROV_WIFISCAN
            case ImprovRPCType::Request_Scan: startImprovWifiScan(); break;
            #endif
            default: {
              DIMPROV_PRINTF("Unknown RPC command %i\n",next);
              sendImprovStateResponse(0x02, true);
            }
          }
          return;
        }
        if (packetByte < 6) { //check header
          if (next != header[packetByte]) {
            DIMPROV_PRINTLN(F("Invalid improv header"));
            return;
          }
        } else if (packetByte > 9) { //RPC data
          rpcData[packetByte - 10] = next;
          if (packetByte > 137) return; //prevent buffer overflow
        }
      }
    }

    checksum += next;
    checksum &= 0xFF;
    packetByte++;
  }
}

void sendImprovStateResponse(uint8_t state, bool error) {
  if (!error && improvError > 0 && improvError < 3) sendImprovStateResponse(0x00, true);
  if (error) improvError = state;
  char out[11] = {'I','M','P','R','O','V'};
  out[6] = IMPROV_VERSION;
  out[7] = error? ImprovPacketType::Error_State : ImprovPacketType::Current_State;
  out[8] = 1;
  out[9] = state;

  unsigned checksum = 0;
  for (unsigned i = 0; i < 10; i++) checksum += out[i];
  out[10] = checksum;
  Serial.write((uint8_t*)out, 11);
  Serial.write('\n');
}

// used by sendImprovIPRPCResult(), sendImprovInfoResponse(), and handleImprovWifiScan()
void sendImprovRPCResult(ImprovRPCType type, uint8_t n_strings, const char **strings) {
  if (improvError > 0 && improvError < 3) sendImprovStateResponse(0x00, true);
  unsigned packetLen = 12;
  char out[256] = {'I','M','P','R','O','V'};
  out[6] = IMPROV_VERSION;
  out[7] = ImprovPacketType::RPC_Response;
  //out[8] = 2; //Length (set below)
  out[9] = type;
  //out[10] = 0; //Data len (set below)
  unsigned pos = 11;

  for (unsigned s = 0; s < n_strings; s++) {
    size_t len = strlen(strings[s]);
    if (pos + len > 254) continue; // simple buffer overflow guard
    out[pos++] = len;
    strcpy(out + pos, strings[s]);
    pos += len;
  }

  packetLen = pos  +1;
  out[8]    = pos  -9; // Length of packet (excluding first 9 header bytes and final checksum byte)
  out[10]   = pos -11; // Data len

  unsigned checksum = 0;
  for (unsigned i = 0; i < packetLen -1; i++) checksum += out[i];
  out[packetLen -1] = checksum;
  Serial.write((uint8_t*)out, packetLen);
  Serial.write('\n');
  DIMPROV_PRINT("RPC result checksum");
  DIMPROV_PRINTLN(checksum);
}

void sendImprovIPRPCResult(ImprovRPCType type) {
  if (Network.isConnected())
  {
    char urlStr[64];
    IPAddress localIP = Network.localIP();
    unsigned len = sprintf(urlStr, "http://%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    if (len > 24) return; //sprintf fail?
    const char *str[1] = {urlStr};
    sendImprovRPCResult(type, 1, str);
  } else {
    sendImprovRPCResult(type, 0);
  }

  improvActive = 1; //no longer provisioning
}

void sendImprovInfoResponse() {
  char bString[32];
  #ifdef ESP8266
  strcpy(bString, "esp8266");
  #else // ESP32
  strncpy(bString, ESP.getChipModel(), 31);
  #if CONFIG_IDF_TARGET_ESP32
  bString[5] = '\0'; // disregard chip revision for classic ESP32
  #else
  bString[31] = '\0'; // just in case
  #endif
  strlwr(bString);
  #endif
  //Use serverDescription if it has been changed from the default "WLED", else mDNS name
  bool useMdnsName = (strcmp(serverDescription, "WLED") == 0 && strlen(cmDNS) > 0);
  char vString[32];
  sprintf_P(vString, PSTR("%s/%i"), versionString, VERSION);
  const char *str[4] = {"WLED", vString, bString, useMdnsName ? cmDNS : serverDescription};

  sendImprovRPCResult(ImprovRPCType::Request_Info, 4, str);
}

#ifndef WLED_DISABLE_IMPROV_WIFISCAN
void startImprovWifiScan() {
  if (improvWifiScanRunning) return;
  WiFi.scanNetworks(true);
  improvWifiScanRunning = true;
}

void handleImprovWifiScan() {
  if (!improvWifiScanRunning) return;
  int16_t status = WiFi.scanComplete();
  if (status == WIFI_SCAN_RUNNING) return;
  // here scan completed or failed (-2)
  improvWifiScanRunning = false;

  for (int i = 0; i < status; i++) {
    char rssiStr[8];
    sprintf(rssiStr, "%d", WiFi.RSSI(i));
    #ifdef ESP8266
    bool isOpen = WiFi.encryptionType(i) == ENC_TYPE_NONE;
    #else
    bool isOpen = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    #endif

    char ssidStr[33];
    strcpy(ssidStr, WiFi.SSID(i).c_str());
    const char *str[3] = {ssidStr, rssiStr, isOpen ? "NO":"YES"};
    sendImprovRPCResult(ImprovRPCType::Request_Scan, 3, str);
  }
  sendImprovRPCResult(ImprovRPCType::Request_Scan, 0);

  WiFi.scanDelete();
}
#else
void startImprovWifiScan() {}
void handleImprovWifiScan() {}
#endif

void parseWiFiCommand(char* rpcData) {
  unsigned len = rpcData[0];
  if (!len || len > 126) return;

  unsigned ssidLen = rpcData[1];
  if (ssidLen > len -1 || ssidLen > 32) return;
  memset(multiWiFi[0].clientSSID, 0, 32);
  memcpy(multiWiFi[0].clientSSID, rpcData+2, ssidLen);

  memset(multiWiFi[0].clientPass, 0, 64);
  if (len > ssidLen +1) {
    unsigned passLen = rpcData[2+ssidLen];
    memset(multiWiFi[0].clientPass, 0, 64);
    memcpy(multiWiFi[0].clientPass, rpcData+3+ssidLen, passLen);
  }

  sendImprovStateResponse(0x03); //provisioning
  improvActive = 2;

  forceReconnect = true;
  serializeConfig();
}