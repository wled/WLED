#pragma once

#include "usermod_fseq.h" // Contains FSEQ playback logic and getter methods for pins
#include "xlz_unzip.h"
#include "wled.h"

#include "sd_adapter_compat.h"

#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>

// ----- Minimal WriteBufferingStream Implementation -----
// This class buffers data before writing it to an underlying Stream.
class WriteBufferingStream : public Stream {
public:
  WriteBufferingStream(Stream &upstream, size_t capacity)
      : _upstream(upstream) {
    _capacity = capacity;
    _buffer = (uint8_t *)malloc(capacity);
    _offset = 0;
    if (!_buffer) {
      DEBUG_PRINTLN(F("[WBS] ERROR: Buffer allocation failed"));
    }
  }
  ~WriteBufferingStream() {
    flush();
    if (_buffer)
      free(_buffer);
  }
  // Write a block of data to the buffer
  size_t write(const uint8_t *buffer, size_t size) override {
    if (!_buffer) return 0;
    size_t total = 0;
    while (size > 0) {
      size_t space = _capacity - _offset;
      size_t toCopy = (size < space) ? size : space;
      memcpy(_buffer + _offset, buffer, toCopy);
      _offset += toCopy;
      buffer += toCopy;
      size -= toCopy;
      total += toCopy;
      if (_offset == _capacity)
        flush();
    }
    return total;
  }
  // Write a single byte
  size_t write(uint8_t b) override { return write(&b, 1); }
  // Flush the buffer to the upstream stream
  void flush() override {
    if (_offset > 0) {
      _upstream.write(_buffer, _offset);
      _offset = 0;
    }
    _upstream.flush();
  }
  int available() override { return _upstream.available(); }
  int read() override { return _upstream.read(); }
  int peek() override { return _upstream.peek(); }

private:
  Stream &_upstream;
  uint8_t *_buffer = nullptr;
  size_t _capacity = 0;
  size_t _offset = 0;
};
// ----- End WriteBufferingStream -----

#define FILE_UPLOAD_BUFFER_SIZE 8192

// Definitions for UDP (FPP) synchronization
#define CTRL_PKT_SYNC 1
#define CTRL_PKT_PING 4
#define CTRL_PKT_BLANK 3

// Structure for the synchronization packet
// Using pragma pack to avoid any padding issues
#pragma pack(push, 1)
struct FPPMultiSyncPacket {
  uint8_t header[4];     // e.g. "FPPD"
  uint8_t packet_type;   // e.g. CTRL_PKT_SYNC
  uint16_t data_len;     // data length
  uint8_t sync_action;   // action: start, stop, sync, open, etc.
  uint8_t sync_type;     // sync type, e.g. 0 for FSEQ
  uint32_t frame_number; // current frame number
  float seconds_elapsed; // elapsed seconds
  char filename[64];     // name of the file to play
  uint8_t raw[128];      // raw packet data
};
#pragma pack(pop)

// UsermodFPP class: Implements FPP (FSEQ/UDP) functionality
class UsermodFPP : public Usermod {
private:
  AsyncUDP udpRx;            // receive socket
  AsyncUDP udpTx;            // send socket
  bool udpStarted = false;

  const IPAddress multicastAddr = IPAddress(239, 70, 80, 80);
  const uint16_t udpPort = 32320;

  unsigned long lastPingTime = 0;
  const unsigned long pingInterval = 10000;

  // startup / reconnect announce burst
  bool announceBurstActive = false;
  uint8_t announceBurstRemaining = 0;
  unsigned long lastAnnounceBurstTime = 0;
  const unsigned long announceBurstInterval = 1000;
  
  IPAddress getBroadcastAddress() {
    IPAddress ip = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    IPAddress broadcast;

    for (uint8_t i = 0; i < 4; i++) {
      broadcast[i] = (ip[i] & mask[i]) | (~mask[i]);
    }

    return broadcast;
  }

  // Variables for FSEQ file upload
  File currentUploadFile;
  String currentUploadFileName = "";
  unsigned long uploadStartTime = 0;
  WriteBufferingStream *uploadStream = nullptr;

  // Deferred XLZ handling
  bool xlzChecked = false;                 // startup scan done
  unsigned long xlzStartTime = 0;          // startup timer
  bool uploadSessionActive = false;        // any recent upload activity
  bool xlzPendingScan = false;             // .xlz uploaded and waiting for idle timeout
  bool xlzProcessing = false;              // guard against re-entry
  unsigned long lastUploadActivity = 0;    // updated on every chunk
  unsigned long lastUploadFinished = 0;    // updated when a file finishes

  // Returns device name from server description
  String getDeviceName() { return String(serverDescription); }

  // Build JSON with system information
  String buildSystemInfoJSON() {
    DynamicJsonDocument doc(1024);

    String devName = getDeviceName();

    String id = "WLED-" + WiFi.macAddress();
    id.replace(":", "");

    doc["HostName"] = id;
    doc["HostDescription"] = devName;
    doc["Platform"] = "ESP32";
    doc["Variant"] = "WLED";
    doc["Mode"] = "remote";
    doc["Version"] = versionString;

    uint16_t major = 0, minor = 0;
    String ver = versionString;
    int dashPos = ver.indexOf('-');
    if (dashPos > 0) ver = ver.substring(0, dashPos);
    int dotPos = ver.indexOf('.');
    if (dotPos > 0) {
      major = ver.substring(0, dotPos).toInt();
      minor = ver.substring(dotPos + 1).toInt();
    } else {
      major = ver.toInt();
    }

    doc["majorVersion"] = major;
    doc["minorVersion"] = minor;
    doc["typeId"] = 195;
    doc["UUID"] = WiFi.macAddress();
    doc["zip"] = true;

    JsonObject utilization = doc.createNestedObject("Utilization");
    utilization["MemoryFree"] = ESP.getFreeHeap();
    utilization["Uptime"] = millis();

    doc["rssi"] = WiFi.RSSI();

    JsonArray ips = doc.createNestedArray("IPS");
    ips.add(WiFi.localIP().toString());

    String json;
	if (doc.overflowed()) {
      DEBUG_PRINTLN(F("[FPP] JSON overflow in buildSystemInfoJSON"));
    }
    serializeJson(doc, json);
    return json;
  }

  // Build JSON with system status
  String buildSystemStatusJSON() {
    DynamicJsonDocument doc(2048);

    JsonObject mqtt = doc.createNestedObject("MQTT");
    mqtt["configured"] = false;
    mqtt["connected"] = false;

    JsonObject currentPlaylist = doc.createNestedObject("current_playlist");
    currentPlaylist["count"] = "0";
    currentPlaylist["description"] = "";
    currentPlaylist["index"] = "0";
    currentPlaylist["playlist"] = "";
    currentPlaylist["type"] = "";

    doc["volume"] = 70;
    doc["media_filename"] = "";
    doc["fppd"] = "running";
    doc["current_song"] = "";

    if (FSEQPlayer::isPlaying()) {
      String fileName = FSEQPlayer::getFileName();
      float elapsedF = FSEQPlayer::getElapsedSeconds();
      uint32_t elapsed = (uint32_t)elapsedF;

      doc["current_sequence"] = fileName;
      doc["playlist"] = "";
      doc["seconds_elapsed"] = String(elapsed);
      doc["seconds_played"] = String(elapsed);
      doc["seconds_remaining"] = "0";
      doc["sequence_filename"] = fileName;

      uint32_t mins = elapsed / 60;
      uint32_t secs = elapsed % 60;
      char timeStr[16];
      snprintf(timeStr, sizeof(timeStr), "%02u:%02u", mins, secs);

      doc["time_elapsed"] = timeStr;
      doc["time_remaining"] = "00:00";

      doc["status"] = 1;
      doc["status_name"] = "playing";
      doc["mode"] = 8;
      doc["mode_name"] = "remote";
    } else {
      doc["current_sequence"] = "";
      doc["playlist"] = "";
      doc["seconds_elapsed"] = "0";
      doc["seconds_played"] = "0";
      doc["seconds_remaining"] = "0";
      doc["sequence_filename"] = "";
      doc["time_elapsed"] = "00:00";
      doc["time_remaining"] = "00:00";
      doc["status"] = 0;
      doc["status_name"] = "idle";
      doc["mode"] = 8;
      doc["mode_name"] = "remote";
    }

    JsonObject adv = doc.createNestedObject("advancedView");

    String devName = getDeviceName();

    String id = "WLED-" + WiFi.macAddress();
    id.replace(":", "");

    adv["HostName"] = id;
    adv["HostDescription"] = devName;
    adv["Platform"] = "WLED";
    adv["Variant"] = "ESP32";
    adv["Mode"] = "remote";
    adv["Version"] = versionString;

    uint16_t major = 0;
    uint16_t minor = 0;

    String ver = versionString;
    int dashPos = ver.indexOf('-');
    if (dashPos > 0) {
      ver = ver.substring(0, dashPos);
    }

    int dotPos = ver.indexOf('.');
    if (dotPos > 0) {
      major = ver.substring(0, dotPos).toInt();
      minor = ver.substring(dotPos + 1).toInt();
    } else {
      major = ver.toInt();
      minor = 0;
    }

    adv["majorVersion"] = major;
    adv["minorVersion"] = minor;
    adv["typeId"] = 195;
    adv["UUID"] = WiFi.macAddress();

    JsonObject util = adv.createNestedObject("Utilization");
    util["MemoryFree"] = ESP.getFreeHeap();
    util["Uptime"] = millis();

    adv["rssi"] = WiFi.RSSI();

    JsonArray ips = adv.createNestedArray("IPS");
    ips.add(WiFi.localIP().toString());

    String json;
	if (doc.overflowed()) {
      DEBUG_PRINTLN(F("[FPP] JSON overflow in buildSystemInfoJSON"));
    }
    serializeJson(doc, json);
    return json;
  }

  // Build JSON for FPP multi-sync systems
  String buildFppdMultiSyncSystemsJSON() {
    DynamicJsonDocument doc(1024);

    JsonArray systems = doc.createNestedArray("systems");
    JsonObject sys = systems.createNestedObject();

    String devName = getDeviceName();

    String id = "WLED-" + WiFi.macAddress();
    id.replace(":", "");

    sys["hostname"] = devName;
    sys["id"] = id;
    sys["ip"] = WiFi.localIP().toString();
    sys["version"] = versionString;
    sys["hardwareType"] = "WLED";
    sys["type"] = 195;
    sys["num_chan"] = strip.getLength() * 3;
    sys["NumPixelPort"] = 1;
    sys["NumSerialPort"] = 0;
    sys["mode"] = "remote";

    String json;
	if (doc.overflowed()) {
      DEBUG_PRINTLN(F("[FPP] JSON overflow in buildSystemInfoJSON"));
    }
    serializeJson(doc, json);
    return json;
  }

  void startUdpIfNeeded() {
    if (udpStarted || WiFi.status() != WL_CONNECTED) return;

    if (udpRx.listenMulticast(multicastAddr, udpPort)) {
      udpStarted = true;
      udpRx.onPacket([this](AsyncUDPPacket packet) { processUdpPacket(packet); });

      DEBUG_PRINTLN(F("[FPP] UDP listener started on multicast"));

      // send several fast announce packets right after startup/reconnect
      announceBurstActive = true;
      announceBurstRemaining = 5;
      lastAnnounceBurstTime = 0;

      // also trigger an immediate regular ping
      lastPingTime = 0;
    } else {
      DEBUG_PRINTLN(F("[FPP] UDP listener start failed"));
    }
  }

  void sendPingPacket(IPAddress destination = IPAddress(255, 255, 255, 255)) {
    uint8_t buf[301];
    memset(buf, 0, sizeof(buf));

    buf[0] = 'F';
    buf[1] = 'P';
    buf[2] = 'P';
    buf[3] = 'D';

    buf[4] = CTRL_PKT_PING;

    uint16_t dataLen = 294;
    buf[5] = dataLen & 0xFF;
    buf[6] = (dataLen >> 8) & 0xFF;

    buf[7] = 0x03;
    buf[8] = 0x00;

    buf[9] = 0xC3;

    uint16_t versionMajor = 0;
    uint16_t versionMinor = 0;

    String ver = versionString;
    int dashPos = ver.indexOf('-');
    if (dashPos > 0) {
      ver = ver.substring(0, dashPos);
    }

    int dotPos = ver.indexOf('.');
    if (dotPos > 0) {
      versionMajor = ver.substring(0, dotPos).toInt();
      versionMinor = ver.substring(dotPos + 1).toInt();
    } else {
      versionMajor = ver.toInt();
    }

    buf[10] = (versionMajor >> 8) & 0xFF;
    buf[11] = versionMajor & 0xFF;
    buf[12] = (versionMinor >> 8) & 0xFF;
    buf[13] = versionMinor & 0xFF;

    buf[14] = 0x08;

    IPAddress ip = WiFi.localIP();
    buf[15] = ip[0];
    buf[16] = ip[1];
    buf[17] = ip[2];
    buf[18] = ip[3];

    String id = "WLED-" + WiFi.macAddress();
    id.replace(":", "");
    if (id.length() > 64) id = id.substring(0, 64);

    for (int i = 0; i < 64; i++) {
      buf[19 + i] = (i < id.length()) ? id[i] : 0;
    }

    String verStr = versionString;
    for (int i = 0; i < 40; i++) {
      buf[84 + i] = (i < verStr.length()) ? verStr[i] : 0;
    }

    String hwType = "WLED";
    for (int i = 0; i < 40; i++) {
      buf[125 + i] = (i < hwType.length()) ? hwType[i] : 0;
    }

    for (int i = 0; i < 120; i++) {
      buf[166 + i] = 0;
    }

    bool ok = udpTx.writeTo(buf, sizeof(buf), destination, udpPort);

    DEBUG_PRINTF("[FPP] Ping send %s -> %s:%u (%u bytes)\n",
                 ok ? "OK" : "FAILED",
                 destination.toString().c_str(),
                 udpPort,
                 sizeof(buf));
  }

  // UDP - process received packet
  void processUdpPacket(AsyncUDPPacket packet) {
    if (packet.length() < 5)
      return;
    if (packet.data()[0] != 'F' || packet.data()[1] != 'P' ||
        packet.data()[2] != 'P' || packet.data()[3] != 'D')
      return;

    uint8_t packetType = packet.data()[4];
    switch (packetType) {
    case CTRL_PKT_SYNC: {
      const size_t baseSize = 17;

      if (packet.length() <= baseSize) {
        DEBUG_PRINTLN(F("[FPP] Sync packet too short, ignoring"));
        break;
      }

      uint8_t syncAction = packet.data()[7];
      uint32_t frameNumber = 0;
      float secondsElapsed = 0.0f;
      memcpy(&frameNumber, packet.data() + 9, sizeof(frameNumber));
      memcpy(&secondsElapsed, packet.data() + 13, sizeof(secondsElapsed));

      DEBUG_PRINTLN(F("[FPP] Received UDP sync packet"));
      DEBUG_PRINTF("[FPP] Sync Packet - Action: %d\n", syncAction);
      DEBUG_PRINTF("[FPP] Frame Number: %lu\n", frameNumber);
      DEBUG_PRINTF("[FPP] Seconds Elapsed: %.2f\n", secondsElapsed);

      size_t filenameOffset = 17;
      size_t maxFilenameLen =
          min((size_t)64, packet.length() - filenameOffset);

      char safeFilename[65];
      memcpy(safeFilename, packet.data() + filenameOffset, maxFilenameLen);
      safeFilename[maxFilenameLen] = '\0';

      DEBUG_PRINT(F("[FPP] Filename: "));
      DEBUG_PRINTLN(safeFilename);

      ProcessSyncPacket(syncAction, String(safeFilename), secondsElapsed);
      break;
    }
    case CTRL_PKT_PING:
      DEBUG_PRINTLN(F("[FPP] Received UDP ping packet"));
      sendPingPacket(packet.remoteIP());
      break;
    case CTRL_PKT_BLANK:
      DEBUG_PRINTLN(F("[FPP] Received UDP blank packet"));
      FSEQPlayer::clearLastPlayback();
      realtimeLock(10, REALTIME_MODE_INACTIVE);
      break;
    default:
      DEBUG_PRINTLN(F("[FPP] Unknown UDP packet type"));
      break;
    }
  }

  // Process sync command with detailed debug output
  void ProcessSyncPacket(uint8_t action, String fileName,
                         float secondsElapsed) {
    if (!fileName.startsWith("/")) {
      fileName = "/" + fileName;
    }

    DEBUG_PRINTLN(F("[FPP] ProcessSyncPacket: Sync command received"));
    DEBUG_PRINTF("[FPP] Action: %d\n", action);
    DEBUG_PRINT(F("[FPP] FileName: "));
    DEBUG_PRINTLN(fileName);
    DEBUG_PRINTF("[FPP] Seconds Elapsed: %.2f\n", secondsElapsed);

    switch (action) {
    case 0: // SYNC_PKT_START
      FSEQPlayer::loadRecording(fileName.c_str(), 0, strip.getLength(),
                                secondsElapsed);
      break;
    case 1: // SYNC_PKT_STOP
      FSEQPlayer::clearLastPlayback();
      realtimeLock(10, REALTIME_MODE_INACTIVE);
      break;
    case 2: // SYNC_PKT_SYNC
      DEBUG_PRINTLN(F("[FPP] ProcessSyncPacket: Sync command received"));
      DEBUG_PRINTF("[FPP] Sync Packet - FileName: %s, Seconds Elapsed: %.2f\n",
                   fileName.c_str(), secondsElapsed);
      if (!FSEQPlayer::isPlaying()) {
        DEBUG_PRINTLN(F("[FPP] Sync: Playback not active, starting playback."));
        FSEQPlayer::loadRecording(fileName.c_str(), 0, strip.getLength(),
                                  secondsElapsed);
      } else {
        FSEQPlayer::syncPlayback(secondsElapsed);
      }
      break;
    case 3: // SYNC_PKT_OPEN
      DEBUG_PRINTLN(F(
          "[FPP] Open command received – metadata request (not implemented)"));
      break;
    default:
      DEBUG_PRINTLN(F("[FPP] ProcessSyncPacket: Unknown sync action"));
      break;
    }
  }

public:
  static constexpr const char _name[] PROGMEM = "FPP Connect";

  // Setup function called once at startup
  void setup() {
    DEBUG_PRINTF("[%s] FPP Usermod loaded\n", _name);

    server.on("/api/system/info", HTTP_GET,
              [this](AsyncWebServerRequest *request) {
                String json = buildSystemInfoJSON();
                request->send(200, "application/json", json);
              });

    server.on("/api/system/status", HTTP_GET,
              [this](AsyncWebServerRequest *request) {
                String json = buildSystemStatusJSON();
                request->send(200, "application/json", json);
              });

    server.on("/api/fppd/multiSyncSystems", HTTP_GET,
              [this](AsyncWebServerRequest *request) {
                String json = buildFppdMultiSyncSystemsJSON();
                request->send(200, "application/json", json);
              });

    // Endpoint for file upload (raw, application/octet-stream)
    server.on(
        "/fpp", HTTP_POST,
        [](AsyncWebServerRequest *request) {
        },
        NULL,
        [this](AsyncWebServerRequest *request,
               uint8_t *data, size_t len,
               size_t index, size_t total) {

          // mark upload session activity on every chunk
          uploadSessionActive = true;
          lastUploadActivity = millis();

          DEBUG_PRINTF("[FPP] Chunk index=%u len=%u total=%u\n", index, len, total);

          if (index == 0) {
            if (uploadStream || currentUploadFile) {
              request->send(409, "text/plain", "Upload already in progress");
              return;
            }

            DEBUG_PRINTLN("[FPP] Starting file upload");

            String fileParam = "";
            if (request->hasParam("filename")) {
              fileParam = request->arg("filename");
            }

            currentUploadFileName =
                (fileParam != "")
                    ? (fileParam.startsWith("/") ? fileParam : "/" + fileParam)
                    : "/default.fseq";

            DEBUG_PRINTF("[FPP] Using filename: %s\n",
                         currentUploadFileName.c_str());

            if (SD_ADAPTER.exists(currentUploadFileName.c_str())) {
              SD_ADAPTER.remove(currentUploadFileName.c_str());
            }

            currentUploadFile =
                SD_ADAPTER.open(currentUploadFileName.c_str(), FILE_WRITE);

            if (!currentUploadFile) {
              DEBUG_PRINTLN(F("[FPP] ERROR: Failed to open file"));
              request->send(500, "text/plain", "File open failed");
              return;
            }

            uploadStream = new WriteBufferingStream(
                currentUploadFile, FILE_UPLOAD_BUFFER_SIZE);

            uploadStartTime = millis();
          }

          if (uploadStream) {
            uploadStream->write(data, len);
          }

          if (index + len == total) {
            DEBUG_PRINTLN("[FPP] Upload finished");

            if (uploadStream) {
              uploadStream->flush();
              delete uploadStream;
              uploadStream = nullptr;
            }

            String uploadedFile = currentUploadFileName;

            if (currentUploadFile) {
              currentUploadFile.close();
            }

            unsigned long duration = millis() - uploadStartTime;
            DEBUG_PRINTF("[FPP] Upload complete in %lu ms\n", duration);

            String lowerName = uploadedFile;
            lowerName.toLowerCase();

            if (lowerName.endsWith(".xlz")) {
              xlzPendingScan = true;
              DEBUG_PRINTF("[XLZ] Deferred unpack scheduled for: %s\n",
                           uploadedFile.c_str());
            }

            lastUploadFinished = millis();
            lastUploadActivity = lastUploadFinished;

            currentUploadFileName = "";
            request->send(200, "text/plain", "Upload complete");
          }
        });

    // Endpoint to list FSEQ files on SD card
    server.on("/fseqfilelist", HTTP_GET, [](AsyncWebServerRequest *request) {
      DynamicJsonDocument doc(1024);
      JsonArray files = doc.createNestedArray("files");

      File root = SD_ADAPTER.open("/");
      if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
          String name = file.name();
          if (name.endsWith(".fseq") || name.endsWith(".FSEQ")) {
            JsonObject fileObj = files.createNestedObject();
            fileObj["name"] = name;
            fileObj["size"] = file.size();
          }
          file.close();
          file = root.openNextFile();
        }
      } else {
        doc["error"] = "Cannot open SD root directory";
      }

      String json;
      serializeJson(doc, json);
      request->send(200, "application/json", json);
    });

    // Endpoint to start FSEQ playback
    server.on("/fpp/connect", HTTP_GET, [this](AsyncWebServerRequest *request) {
      if (!request->hasArg("file")) {
        request->send(400, "text/plain", "Missing 'file' parameter");
        return;
      }
      String filepath = request->arg("file");
      if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
      }
      FSEQPlayer::loadRecording(filepath.c_str(), 0, strip.getLength());
      request->send(200, "text/plain", "FPP connect started: " + filepath);
    });

    // Endpoint to stop FSEQ playback
    server.on("/fpp/stop", HTTP_GET, [this](AsyncWebServerRequest *request) {
      FSEQPlayer::clearLastPlayback();
      realtimeLock(10, REALTIME_MODE_INACTIVE);
      request->send(200, "text/plain", "FPP connect stopped");
    });

    startUdpIfNeeded();

  }

  // Main loop function
  void loop() {
    startUdpIfNeeded();

    if (udpStarted && WiFi.status() == WL_CONNECTED) {
      IPAddress broadcastIp = getBroadcastAddress();

      // fast announce burst after startup / reconnect
      if (announceBurstActive &&
          (lastAnnounceBurstTime == 0 ||
           millis() - lastAnnounceBurstTime >= announceBurstInterval)) {

        sendPingPacket(broadcastIp);
        lastAnnounceBurstTime = millis();

        if (announceBurstRemaining > 0) {
          announceBurstRemaining--;
        }

        if (announceBurstRemaining == 0) {
          announceBurstActive = false;
        }
      }

      // regular keepalive ping
      if (millis() - lastPingTime >= pingInterval) {
        sendPingPacket(broadcastIp);
        lastPingTime = millis();
      }
    }

    // startup scan after reboot
    if (xlzStartTime == 0) {
      xlzStartTime = millis();
      DEBUG_PRINTF("[XLZ] start timer at %lu\n", xlzStartTime);
    }

    if (!xlzChecked && (millis() - xlzStartTime >= 2000)) {
      xlzChecked = true;

      DEBUG_PRINTF("[XLZ] 2s reached, starting startup scan at %lu\n", millis());

      File root = SD_ADAPTER.open("/");
      if (!root || !root.isDirectory()) {
        DEBUG_PRINTLN("[XLZ] SD root not accessible, skipping startup scan");
        if (root) root.close();
      } else {
        root.close();
        DEBUG_PRINTLN("[XLZ] SD ready -> startup scanning");
        XLZUnzip::processAllPendingXLZ();
        DEBUG_PRINTLN("[XLZ] startup scan finished");
      }
    }

    // deferred XLZ processing after upload inactivity
    if (uploadSessionActive && xlzPendingScan && !xlzProcessing) {
      if (millis() - lastUploadActivity >= 10000) {
        DEBUG_PRINTF("[XLZ] upload idle for 10s -> processing pending XLZ at %lu\n",
                     millis());

        xlzProcessing = true;
        XLZUnzip::processAllPendingXLZ();
        xlzProcessing = false;

        xlzPendingScan = false;
        uploadSessionActive = false;

        DEBUG_PRINTLN("[XLZ] deferred upload scan finished");
      }
    }
  }

  uint16_t getId() override { return USERMOD_ID_FPP; }
  void addToConfig(JsonObject &root) override {}
  bool readFromConfig(JsonObject &root) override { return true; }
};