#pragma once

#include "usermod_fseq.h"
#include "xlz_unzip.h"
#include "wled.h"
#include "sd_adapter_compat.h"

#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>

uint16_t FSEQ_refreshFileIndexCache();
int16_t FSEQ_findFileIndexByName(const String &name);
void FSEQ_markFppControlActivity();
void FSEQ_clearFppOverride();
bool FSEQ_isFppOverrideActive();
void FSEQ_invalidateFileIndexCache();

class UsermodFPP;

inline UsermodFPP *&FPP_usermodInstance() {
  static UsermodFPP *instance = nullptr;
  return instance;
}

inline void FPP_write16(uint8_t *buffer, uint16_t value) {
  buffer[0] = value & 0xFF;
  buffer[1] = (value >> 8) & 0xFF;
}

inline void FPP_write32(uint8_t *buffer, uint32_t value) {
  buffer[0] = value & 0xFF;
  buffer[1] = (value >> 8) & 0xFF;
  buffer[2] = (value >> 16) & 0xFF;
  buffer[3] = (value >> 24) & 0xFF;
}

bool FPP_sendEffectSyncMessage(uint8_t action, const String &fileName,
                               uint32_t currentFrame, float secondsElapsed,
                               bool sendMulticast, bool sendBroadcast);

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
    if (_buffer) free(_buffer);
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    if (!_buffer || _writeError) return 0;

    size_t total = 0;
    while (size > 0) {
      size_t space = _capacity - _offset;
      size_t toCopy = (size < space) ? size : space;
      memcpy(_buffer + _offset, buffer, toCopy);
      _offset += toCopy;
      buffer += toCopy;
      size -= toCopy;
      total += toCopy;

      if (_offset == _capacity && !flushBuffer()) {
        _writeError = true;
        break;
      }
    }

    return total;
  }

  size_t write(uint8_t b) override { return write(&b, 1); }

  void flush() override {
    if (!flushBuffer()) {
      _writeError = true;
    }
    _upstream.flush();
  }

  bool hasWriteError() const { return _writeError; }

  int available() override { return _upstream.available(); }
  int read() override { return _upstream.read(); }
  int peek() override { return _upstream.peek(); }

private:
  bool flushBuffer() {
    if (_offset == 0) return true;

    const size_t written = _upstream.write(_buffer, _offset);
    if (written != _offset) {
      DEBUG_PRINTF("[WBS] ERROR: Short SD write: %u/%u\n",
                   (unsigned)written, (unsigned)_offset);
      _offset = 0;
      return false;
    }

    _offset = 0;
    return true;
  }

  Stream &_upstream;
  uint8_t *_buffer = nullptr;
  size_t _capacity = 0;
  size_t _offset = 0;
  bool _writeError = false;
};

#define FILE_UPLOAD_BUFFER_SIZE 8192
#define CTRL_PKT_SYNC 1
#define CTRL_PKT_PING 4
#define CTRL_PKT_BLANK 3

class UsermodFPP : public Usermod {
private:
  friend bool FPP_sendEffectSyncMessage(uint8_t action, const String &fileName,
                                        uint32_t currentFrame, float secondsElapsed,
                                        bool sendMulticast, bool sendBroadcast);
  AsyncUDP udpListen;
  AsyncUDP udpMulticast;
  bool udpListenStarted = false;
  bool udpMulticastStarted = false;
  bool udpStarted = false;
  const IPAddress multicastAddr = IPAddress(239, 70, 80, 80);
  const uint16_t udpPort = 32320;
  unsigned long lastPingTime = 0;
  const unsigned long pingInterval = 30000;
  bool announceBurstActive = false;
  uint8_t announceBurstRemaining = 0;
  unsigned long lastAnnounceBurstTime = 0;
  const unsigned long announceBurstInterval = 1000;
  wl_status_t lastWiFiStatus = WL_IDLE_STATUS;

  File currentUploadFile;
  String currentUploadFileName = "";
  unsigned long uploadStartTime = 0;
  WriteBufferingStream *uploadStream = nullptr;
  const unsigned long uploadInactivityTimeout = 60000;

  bool xlzChecked = false;
  unsigned long xlzStartTime = 0;
  bool uploadSessionActive = false;
  bool xlzPendingScan = false;
  bool xlzProcessing = false;
  unsigned long lastUploadActivity = 0;
  unsigned long lastUploadFinished = 0;

  enum PendingCommandType : uint8_t {
    PENDING_NONE = 0,
    PENDING_START,
    PENDING_STOP,
    PENDING_SYNC,
    PENDING_BLANK
  };

  portMUX_TYPE fppMux = portMUX_INITIALIZER_UNLOCKED;
  volatile PendingCommandType pendingCommand = PENDING_NONE;
  volatile bool pendingPingReply = false;
  char pendingFileName[65] = {0};
  float pendingSecondsElapsed = 0.0f;
  IPAddress lastFppSenderIP = IPAddress(0, 0, 0, 0);
  IPAddress pendingPingReplyIP = IPAddress(0, 0, 0, 0);

  float lastFppSyncSeconds = 0.0f;
  uint32_t lastFppSyncMillis = 0;
  float lastStatusElapsedSeconds = 0.0f;
  uint32_t lastStatusElapsedMillis = 0;
  char lastSequenceName[65] = {0};

  struct FppStatusSnapshot {
    bool playbackActive = false;
    char sequenceName[65] = {0};
    float elapsedSeconds = 0.0f;
    uint32_t updatedMillis = 0;
  };

  FppStatusSnapshot statusSnapshot;

  String getDeviceName() { return String(serverDescription); }

  void copyNormalizedSequenceName(const char *fileName, char *dest, size_t destSize) {
    if (!dest || destSize == 0) return;
    memset(dest, 0, destSize);
    if (!fileName || fileName[0] == '\0') return;

    const char *normalized = (fileName[0] == '/') ? fileName + 1 : fileName;
    const size_t len = strnlen(normalized, destSize - 1);
    memcpy(dest, normalized, len);
  }

  void cacheSequenceNameLoopOnly(const char *fileName) {
    copyNormalizedSequenceName(fileName, lastSequenceName, sizeof(lastSequenceName));
  }

  void rememberSyncProgressLoopOnly(const char *fileName, float secondsElapsed) {
    if (fileName && fileName[0] != '\0') {
      cacheSequenceNameLoopOnly(fileName);
    }

    lastFppSyncSeconds = secondsElapsed;
    if (lastFppSyncSeconds < 0.0f) lastFppSyncSeconds = 0.0f;
    lastFppSyncMillis = millis();
  }

  void clearPlaybackStatusCacheLoopOnly(bool clearSequenceName = false) {
    lastFppSyncSeconds = 0.0f;
    lastFppSyncMillis = 0;
    lastStatusElapsedSeconds = 0.0f;
    lastStatusElapsedMillis = 0;
    if (clearSequenceName) {
      memset(lastSequenceName, 0, sizeof(lastSequenceName));
    }
  }

  float getStableStatusElapsedSecondsLoopOnly() {
    const uint32_t now = millis();
    float elapsed = FSEQPlayer::getElapsedSeconds();

    if (elapsed <= 0.0f && lastFppSyncMillis != 0) {
      const float syncElapsed =
          lastFppSyncSeconds + ((float)(now - lastFppSyncMillis) / 1000.0f);
      if (syncElapsed > elapsed) elapsed = syncElapsed;
    }

    if (elapsed <= 0.0f && lastStatusElapsedMillis != 0) {
      const float heldElapsed =
          lastStatusElapsedSeconds + ((float)(now - lastStatusElapsedMillis) / 1000.0f);
      if (heldElapsed > elapsed) elapsed = heldElapsed;
    }

    if (elapsed > 0.0f) {
      lastStatusElapsedSeconds = elapsed;
      lastStatusElapsedMillis = now;
    }

    return (elapsed > 0.0f) ? elapsed : 0.0f;
  }

  void publishStatusSnapshotLoopOnly(bool playbackActive,
                                     const char *fileName,
                                     float elapsedSeconds) {
    FppStatusSnapshot next;
    next.playbackActive = playbackActive;
    copyNormalizedSequenceName(fileName, next.sequenceName, sizeof(next.sequenceName));
    next.elapsedSeconds = elapsedSeconds > 0.0f ? elapsedSeconds : 0.0f;
    next.updatedMillis = millis();

    portENTER_CRITICAL(&fppMux);
    statusSnapshot = next;
    portEXIT_CRITICAL(&fppMux);
  }

  FppStatusSnapshot getStatusSnapshot() {
    FppStatusSnapshot copy;
    portENTER_CRITICAL(&fppMux);
    copy = statusSnapshot;
    portEXIT_CRITICAL(&fppMux);
    return copy;
  }

  IPAddress getLastFppSenderIPSnapshot() {
    IPAddress copy;
    portENTER_CRITICAL(&fppMux);
    copy = lastFppSenderIP;
    portEXIT_CRITICAL(&fppMux);
    return copy;
  }

  void updatePlaybackStatusSnapshotLoopOnly() {
    const bool playbackActive = FSEQPlayer::isPlaying() ||
                                FSEQ_isFppOverrideActive() ||
                                (realtimeMode == REALTIME_MODE_FSEQ &&
                                 lastSequenceName[0] != '\0');

    if (!playbackActive) {
      publishStatusSnapshotLoopOnly(false, nullptr, 0.0f);
      return;
    }

    const String activeFileName = FSEQPlayer::getFileName();
    if (activeFileName.length() > 0) {
      cacheSequenceNameLoopOnly(activeFileName.c_str());
    }

    const float elapsed = getStableStatusElapsedSecondsLoopOnly();
    publishStatusSnapshotLoopOnly(true, lastSequenceName, elapsed);
  }

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
    serializeJson(doc, json);
    return json;
  }

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

    const FppStatusSnapshot snapshot = getStatusSnapshot();

    if (snapshot.playbackActive) {
      String fileName = String(snapshot.sequenceName);
      const uint32_t elapsed = (uint32_t)snapshot.elapsedSeconds;
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
    if (dashPos > 0) ver = ver.substring(0, dashPos);
    int dotPos = ver.indexOf('.');
    if (dotPos > 0) {
      major = ver.substring(0, dotPos).toInt();
      minor = ver.substring(dotPos + 1).toInt();
    } else {
      major = ver.toInt();
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
    serializeJson(doc, json);
    return json;
  }

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
    serializeJson(doc, json);
    return json;
  }

  IPAddress getBroadcastAddress() {
    IPAddress ip = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    IPAddress broadcast;
    for (uint8_t i = 0; i < 4; i++) {
      broadcast[i] = (ip[i] & mask[i]) | (~mask[i] & 0xFF);
    }
    return broadcast;
  }

  AsyncUDP *getTxUdp() {
    if (udpListenStarted) return &udpListen;
    if (udpMulticastStarted) return &udpMulticast;
    return nullptr;
  }

  bool sendUdpPacket(const uint8_t *data, size_t len, const IPAddress &destination) {
    AsyncUDP *txUdp = getTxUdp();
    return txUdp ? txUdp->writeTo(data, len, destination, udpPort) : false;
  }

  void stopUdpListeners() {
    if (udpListenStarted) udpListen.close();
    if (udpMulticastStarted) udpMulticast.close();
    udpListenStarted = false;
    udpMulticastStarted = false;
    udpStarted = false;
  }

  void startUdpIfNeeded() {
    if (udpStarted || WiFi.status() != WL_CONNECTED) return;

    stopUdpListeners();

    udpListenStarted = udpListen.listen(udpPort);
    if (udpListenStarted) {
      udpListen.onPacket([this](AsyncUDPPacket packet) { processUdpPacket(packet); });
      DEBUG_PRINTF("[FPP] UDP listener started on port %u\n", udpPort);
    } else {
      DEBUG_PRINTF("[FPP] UDP listener on port %u failed\n", udpPort);
    }

    udpMulticastStarted = udpMulticast.listenMulticast(multicastAddr, udpPort);
    if (udpMulticastStarted) {
      udpMulticast.onPacket([this](AsyncUDPPacket packet) { processUdpPacket(packet); });
      DEBUG_PRINTF("[FPP] UDP multicast listener started on %s:%u\n",
                   multicastAddr.toString().c_str(), udpPort);
    } else {
      DEBUG_PRINTF("[FPP] UDP multicast listener failed on %s:%u\n",
                   multicastAddr.toString().c_str(), udpPort);
    }

    udpStarted = udpListenStarted || udpMulticastStarted;
    if (!udpStarted) {
      DEBUG_PRINTLN(F("[FPP] Failed to start UDP listeners"));
      return;
    }

    announceBurstActive = true;
    announceBurstRemaining = 5;
    lastAnnounceBurstTime = 0;
    lastPingTime = 0;
    DEBUG_PRINTLN(F("[FPP] Discovery listeners active"));
  }

  bool sendSyncMessage(uint8_t action, const String &fileName,
                       uint32_t currentFrame, float secondsElapsed,
                       bool sendMulticast, bool sendBroadcast) {
    if (!udpStarted || WiFi.status() != WL_CONNECTED) return false;
    if (!sendMulticast && !sendBroadcast) return false;

    char filename[65];
    memset(filename, 0, sizeof(filename));
    const String normalized = fileName.startsWith("/") ? fileName.substring(1) : fileName;
    const size_t fileLen = min(strlen(normalized.c_str()), sizeof(filename) - 1);
    if (fileLen > 0) {
      memcpy(filename, normalized.c_str(), fileLen);
      filename[fileLen] = '\0';
    }

    const uint16_t payloadLen = 10 + (uint16_t)fileLen + 1;
    const size_t packetLen = 7 + payloadLen;
    uint8_t packet[7 + 10 + 65];
    memset(packet, 0, sizeof(packet));

    packet[0] = 'F';
    packet[1] = 'P';
    packet[2] = 'P';
    packet[3] = 'D';
    packet[4] = CTRL_PKT_SYNC;
    FPP_write16(packet + 5, payloadLen);
    packet[7] = action;
    packet[8] = 0x00;
    FPP_write32(packet + 9, currentFrame);
    memcpy(packet + 13, &secondsElapsed, sizeof(secondsElapsed));
    memcpy(packet + 17, filename, fileLen);
    packet[17 + fileLen] = '\0';

    bool sent = false;
    if (sendBroadcast) {
      const IPAddress subnetBroadcast = getBroadcastAddress();
      const IPAddress globalBroadcast(255, 255, 255, 255);
      sent |= sendUdpPacket(packet, packetLen, subnetBroadcast);
      sent |= sendUdpPacket(packet, packetLen, globalBroadcast);
    }
    if (sendMulticast) {
      sent |= sendUdpPacket(packet, packetLen, multicastAddr);
    }

    DEBUG_PRINTF("[FPP] Sync %u file=%s sec=%.3f mc=%u bc=%u\n",
                 action, filename, secondsElapsed,
                 sendMulticast ? 1 : 0,
                 sendBroadcast ? 1 : 0);
    return sent;
  }

  void sendPingPacket(IPAddress destination = IPAddress(255, 255, 255, 255)) {
    uint8_t buf[301];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'F'; buf[1] = 'P'; buf[2] = 'P'; buf[3] = 'D';
    buf[4] = 0x04;
    uint16_t dataLen = 294;
    buf[5] = dataLen & 0xFF;
    buf[6] = (dataLen >> 8) & 0xFF;
    buf[7] = 0x03;
    buf[8] = 0x00;
    buf[9] = 0xC3;
    uint16_t versionMajor = 0, versionMinor = 0;
    String ver = versionString;
    int dashPos = ver.indexOf('-');
    if (dashPos > 0) ver = ver.substring(0, dashPos);
    int dotPos = ver.indexOf('.');
    if (dotPos > 0) {
      versionMajor = ver.substring(0, dotPos).toInt();
      versionMinor = ver.substring(dotPos + 1).toInt();
    }
    buf[10] = (versionMajor >> 8) & 0xFF;
    buf[11] = versionMajor & 0xFF;
    buf[12] = (versionMinor >> 8) & 0xFF;
    buf[13] = versionMinor & 0xFF;
    buf[14] = 0x08;
    IPAddress ip = WiFi.localIP();
    buf[15] = ip[0]; buf[16] = ip[1]; buf[17] = ip[2]; buf[18] = ip[3];
    String id = "WLED-" + WiFi.macAddress();
    id.replace(":", "");
    if (id.length() > 64) id = id.substring(0, 64);
    for (int i = 0; i < 64; i++) buf[19 + i] = (i < id.length()) ? id[i] : 0;
    String verStr = versionString;
    for (int i = 0; i < 40; i++) buf[84 + i] = (i < verStr.length()) ? verStr[i] : 0;
    String hwType = "WLED";
    for (int i = 0; i < 40; i++) buf[125 + i] = (i < hwType.length()) ? hwType[i] : 0;
    for (int i = 0; i < 120; i++) buf[166 + i] = 0;
    bool ok = sendUdpPacket(buf, sizeof(buf), destination);
    DEBUG_PRINTF("[FPP] Ping %s -> %s:%u len=%u\n",
                 ok ? "sent" : "FAILED",
                 destination.toString().c_str(),
                 udpPort,
                 sizeof(buf));
  }

  void sendDiscoveryBurst() {
    IPAddress subnetBroadcast = getBroadcastAddress();
    //IPAddress globalBroadcast(255, 255, 255, 255);

    sendPingPacket(subnetBroadcast);
    //sendPingPacket(globalBroadcast);
  }

  void queuePendingCommand(PendingCommandType cmd,
                           const char *fileName = nullptr,
                           float seconds = 0.0f,
                           IPAddress senderIP = IPAddress(0, 0, 0, 0)) {
    portENTER_CRITICAL(&fppMux);

    const bool newHasFile = (fileName && fileName[0] != '\0');

    // Keep a pending START alive until loop() has processed it.
    // If more START/SYNC packets arrive before then, only refresh the target
    // time/IP and optionally the filename.
    if (pendingCommand == PENDING_START &&
        (cmd == PENDING_START || cmd == PENDING_SYNC)) {
      pendingSecondsElapsed = seconds;
      lastFppSenderIP = senderIP;

      if (newHasFile) {
        memset(pendingFileName, 0, sizeof(pendingFileName));
        const size_t len = strnlen(fileName, sizeof(pendingFileName) - 1);
        memcpy(pendingFileName, fileName, len);
      }

      portEXIT_CRITICAL(&fppMux);
      return;
    }

    pendingCommand = cmd;
    pendingSecondsElapsed = seconds;
    lastFppSenderIP = senderIP;

    if (cmd == PENDING_STOP || cmd == PENDING_BLANK) {
      memset(pendingFileName, 0, sizeof(pendingFileName));
    } else if (newHasFile) {
      memset(pendingFileName, 0, sizeof(pendingFileName));
      const size_t len = strnlen(fileName, sizeof(pendingFileName) - 1);
      memcpy(pendingFileName, fileName, len);
    } else if (cmd == PENDING_START) {
      // Never reuse an old filename for an explicit START if none was supplied.
      memset(pendingFileName, 0, sizeof(pendingFileName));
    }
    // For a SYNC without filename we intentionally keep the most recent pending
    // filename so a late join can still start from the correct sequence.

    portEXIT_CRITICAL(&fppMux);
  }
  
  void queuePendingPingReply(IPAddress senderIP) {
    portENTER_CRITICAL(&fppMux);
    pendingPingReplyIP = senderIP;
    pendingPingReply = true;
    portEXIT_CRITICAL(&fppMux);
  }

  bool isUnsafeUploadPath(const String &path) {
    if (path.length() == 0) return true;
    if (path.indexOf("..") >= 0) return true;
    if (path.indexOf('\\') >= 0) return true;
    return false;
  }

  String normalizeUploadPath(String path) {
    path.trim();
    if (!path.startsWith("/")) path = "/" + path;
    return path;
  }

  void cleanupFppUploadState(bool removePartialFile) {
    const String partialFile = currentUploadFileName;

    if (uploadStream) {
      delete uploadStream;
      uploadStream = nullptr;
    }

    if (currentUploadFile) {
      currentUploadFile.close();
    }

    if (removePartialFile && partialFile.length() > 0) {
      SD_ADAPTER.remove(partialFile.c_str());
      DEBUG_PRINTF("[FPP] Removed partial upload: %s\n", partialFile.c_str());
    }

    currentUploadFile = File();
    currentUploadFileName = "";
    uploadStartTime = 0;
    lastUploadActivity = 0;
    uploadSessionActive = false;
    xlzPendingScan = false;
  }

  void cleanupStaleFppUploadIfNeeded() {
    if (!uploadStream && !currentUploadFile) return;
    if (lastUploadActivity == 0) return;
    if (millis() - lastUploadActivity < uploadInactivityTimeout) return;

    DEBUG_PRINTLN(F("[FPP] Upload timed out; cleaning partial state"));
    cleanupFppUploadState(true);
  }

  void finishSuccessfulFppUpload(const String &uploadedFile) {
    String lowerName = uploadedFile;
    lowerName.toLowerCase();

    lastUploadFinished = millis();
    lastUploadActivity = lastUploadFinished;
    currentUploadFileName = "";
    currentUploadFile = File();
    uploadStartTime = 0;

    if (lowerName.endsWith(".xlz")) {
      xlzPendingScan = true;
      uploadSessionActive = true;
    } else {
      FSEQ_invalidateFileIndexCache();
      uploadSessionActive = false;
    }
  }

  void processUdpPacket(AsyncUDPPacket packet) {
    if (packet.length() < 7) return;
    if (WiFi.status() == WL_CONNECTED && packet.remoteIP() == WiFi.localIP()) return;
    if (packet.data()[0] != 'F' || packet.data()[1] != 'P' ||
        packet.data()[2] != 'P' || packet.data()[3] != 'D') return;

    uint8_t packetType = packet.data()[4];
    switch (packetType) {
    case CTRL_PKT_SYNC: {
      const uint16_t extraDataLen =
          (uint16_t)packet.data()[5] | ((uint16_t)packet.data()[6] << 8);
      const size_t payloadLen = packet.length() - 7;
      if (payloadLen < 10) {
        DEBUG_PRINTLN(F("[FPP] Sync packet too short, ignoring"));
        break;
      }
      if (extraDataLen != 0 && payloadLen < extraDataLen) {
        DEBUG_PRINTLN(F("[FPP] Sync packet truncated, ignoring"));
        break;
      }

      const uint8_t syncAction = packet.data()[7];
      const uint8_t syncType = packet.data()[8];
      if (syncType != 0x00) {
        break;
      }

      float secondsElapsed = 0.0f;
      memcpy(&secondsElapsed, packet.data() + 13, sizeof(secondsElapsed));

      const size_t filenameOffset = 17;
      const size_t filenameBytes = packet.length() > filenameOffset
                                       ? (packet.length() - filenameOffset)
                                       : 0;
      const size_t copyLen = min((size_t)64, filenameBytes);
      char safeFilename[65];
      memset(safeFilename, 0, sizeof(safeFilename));
      if (copyLen > 0) {
        memcpy(safeFilename, packet.data() + filenameOffset, copyLen);
        safeFilename[copyLen] = '\0';
        const size_t realLen = strnlen(safeFilename, copyLen);
        safeFilename[realLen] = '\0';
      }

      switch (syncAction) {
        case 0:
          queuePendingCommand(PENDING_START, safeFilename, secondsElapsed, packet.remoteIP());
          break;
        case 1:
          queuePendingCommand(PENDING_STOP, nullptr, 0.0f, packet.remoteIP());
          break;
        case 2:
          queuePendingCommand(PENDING_SYNC, safeFilename, secondsElapsed, packet.remoteIP());
          break;
        default:
          break;
      }
      break;
    }
    case CTRL_PKT_PING:
      if (packet.isBroadcast() || packet.isMulticast()) {
        queuePendingPingReply(packet.remoteIP());
      }
      break;
    case CTRL_PKT_BLANK:
      queuePendingCommand(PENDING_BLANK, nullptr, 0.0f, packet.remoteIP());
      break;
    default:
      break;
    }
  }

  void startRealtimeFppPlayback(const String &fileName,
                               float secondsElapsed,
                               const IPAddress &senderIP) {
    String normalized = fileName;
    if (normalized.length() == 0) {
      if (FSEQPlayer::isPlaying()) {
        normalized = "/" + FSEQPlayer::getFileName();
      } else if (lastSequenceName[0] != '\0') {
        normalized = "/" + String(lastSequenceName);
      } else {
        DEBUG_PRINTLN(F("[FPP] Cannot start realtime playback: no filename available"));
        return;
      }
    } else if (!normalized.startsWith("/")) {
      normalized = "/" + normalized;
    }

    DEBUG_PRINTF("[FPP] Start realtime playback: %s @ %.3fs\n",
                 normalized.c_str(), secondsElapsed);

    rememberSyncProgressLoopOnly(normalized.c_str(), secondsElapsed);
    FSEQ_markFppControlActivity();
    realtimeIP = senderIP;
    useMainSegmentOnly = false;
    realtimeLock(3000, REALTIME_MODE_FSEQ);
    FSEQPlayer::loadRecording(normalized.c_str(), secondsElapsed, true);

    if (!FSEQPlayer::isPlaying()) {
      DEBUG_PRINTF("[FPP] Failed to activate realtime playback for %s\n",
                   normalized.c_str());
      return;
    }

    FSEQPlayer::renderRealtimeFrame();
  }

  void stopRealtimeFppPlayback() {
    FSEQ_clearFppOverride();
    FSEQPlayer::clearLastPlayback();
    clearPlaybackStatusCacheLoopOnly(false);
    exitRealtime();
  }

  void processPendingFppCommand() {
    PendingCommandType cmd;
    char fileName[65];
    float seconds;
    IPAddress senderIP;

    portENTER_CRITICAL(&fppMux);
    cmd = pendingCommand;
    if (cmd == PENDING_NONE) {
      portEXIT_CRITICAL(&fppMux);
      return;
    }
    pendingCommand = PENDING_NONE;
    memcpy(fileName, pendingFileName, sizeof(fileName));
    seconds = pendingSecondsElapsed;
    senderIP = lastFppSenderIP;
    portEXIT_CRITICAL(&fppMux);

    String fn = String(fileName);
    switch (cmd) {
      case PENDING_START:
        startRealtimeFppPlayback(fn, seconds, senderIP);
        break;
      case PENDING_STOP:
      case PENDING_BLANK:
        stopRealtimeFppPlayback();
        break;
      case PENDING_SYNC: {
        String normalized = fn;
        if (normalized.length() == 0) {
          if (FSEQPlayer::isPlaying()) {
            normalized = "/" + FSEQPlayer::getFileName();
          } else if (lastSequenceName[0] != '\0') {
            normalized = "/" + String(lastSequenceName);
          }
        } else if (!normalized.startsWith("/")) {
          normalized = "/" + normalized;
        }

        if (normalized.length() == 0) {
          DEBUG_PRINTLN(F("[FPP] Ignoring SYNC without filename and without active playback"));
          break;
        }

        const String wantedFileName = normalized.substring(1);
        const String activeFileName = FSEQPlayer::getFileName();
        if (!FSEQPlayer::isPlaying() ||
            !activeFileName.equalsIgnoreCase(wantedFileName)) {
          startRealtimeFppPlayback(normalized, seconds, senderIP);
          break;
        }

        rememberSyncProgressLoopOnly(normalized.c_str(), seconds);
        FSEQ_markFppControlActivity();
        realtimeIP = senderIP;
        useMainSegmentOnly = false;
        realtimeLock(3000, REALTIME_MODE_FSEQ);
        FSEQPlayer::syncPlayback(seconds);
        break;
      }
      default:
        break;
    }
  }

public:
  static const char _name[];

  void setup() {
    DEBUG_PRINTF("[%s] FPP Usermod loaded\n", _name);
    FPP_usermodInstance() = this;
    server.on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
      request->send(200, "application/json", buildSystemInfoJSON());
    });
    server.on("/api/system/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
      request->send(200, "application/json", buildSystemStatusJSON());
    });
    server.on("/api/fppd/multiSyncSystems", HTTP_GET, [this](AsyncWebServerRequest *request) {
      request->send(200, "application/json", buildFppdMultiSyncSystemsJSON());
    });

    server.on("/fpp", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        const unsigned long now = millis();
        lastUploadActivity = now;

        if (index == 0) {
          cleanupStaleFppUploadIfNeeded();

          if (uploadStream || currentUploadFile) {
            request->send(409, "text/plain", "Upload already in progress");
            return;
          }

          String fileParam = "";
          if (request->hasParam("filename")) fileParam = request->arg("filename");
          currentUploadFileName = normalizeUploadPath(fileParam.length() > 0 ? fileParam : "default.fseq");

          if (isUnsafeUploadPath(currentUploadFileName) || currentUploadFileName == "/") {
            currentUploadFileName = "";
            request->send(400, "text/plain", "Invalid filename");
            return;
          }

          if (SD_ADAPTER.exists(currentUploadFileName.c_str())) {
            SD_ADAPTER.remove(currentUploadFileName.c_str());
          }

          currentUploadFile = SD_ADAPTER.open(currentUploadFileName.c_str(), FILE_WRITE);
          if (!currentUploadFile) {
            cleanupFppUploadState(false);
            request->send(500, "text/plain", "File open failed");
            return;
          }

          uploadStream = new WriteBufferingStream(currentUploadFile, FILE_UPLOAD_BUFFER_SIZE);
          if (!uploadStream) {
            cleanupFppUploadState(true);
            request->send(500, "text/plain", "Upload buffer allocation failed");
            return;
          }

          uploadStartTime = now;
          uploadSessionActive = true;
        }

        if (!uploadStream) {
          request->send(500, "text/plain", "Upload stream missing");
          return;
        }

        const size_t written = uploadStream->write(data, len);
        if (written != len || uploadStream->hasWriteError()) {
          cleanupFppUploadState(true);
          request->send(500, "text/plain", "Upload write failed");
          return;
        }

        if (index + len == total) {
          uploadStream->flush();
          const bool writeOk = !uploadStream->hasWriteError();
          delete uploadStream;
          uploadStream = nullptr;

          String uploadedFile = currentUploadFileName;
          if (currentUploadFile) {
            currentUploadFile.close();
          }

          if (!writeOk) {
            if (uploadedFile.length() > 0) {
              SD_ADAPTER.remove(uploadedFile.c_str());
            }
            cleanupFppUploadState(false);
            request->send(500, "text/plain", "Upload flush failed");
            return;
          }

          finishSuccessfulFppUpload(uploadedFile);
          request->send(200, "text/plain", "Upload complete");
        }
      });

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

    lastWiFiStatus = WiFi.status();
    startUdpIfNeeded();
  }

  void loop() {
    wl_status_t wifiNow = WiFi.status();

    if (wifiNow != lastWiFiStatus) {
      DEBUG_PRINTF("[FPP] WiFi status changed: %d -> %d\n", lastWiFiStatus, wifiNow);

      if (wifiNow == WL_CONNECTED) {
        stopUdpListeners();
        announceBurstActive = true;
        announceBurstRemaining = 5;
        lastAnnounceBurstTime = 0;
        lastPingTime = 0;
        startUdpIfNeeded();
      } else {
        stopUdpListeners();
      }

      lastWiFiStatus = wifiNow;
    }

    startUdpIfNeeded();
    cleanupStaleFppUploadIfNeeded();
    processPendingFppCommand();

    bool doPingReply = false;
    IPAddress pingReplyIP;

    portENTER_CRITICAL(&fppMux);
    if (pendingPingReply) {
      doPingReply = true;
      pingReplyIP = pendingPingReplyIP;
      pendingPingReply = false;
    }
    portEXIT_CRITICAL(&fppMux);

    if (doPingReply && udpStarted && wifiNow == WL_CONNECTED) {
      sendPingPacket(pingReplyIP);
    }

    if (udpStarted && wifiNow == WL_CONNECTED) {
      if (announceBurstActive &&
          (lastAnnounceBurstTime == 0 ||
           millis() - lastAnnounceBurstTime >= announceBurstInterval)) {
        sendDiscoveryBurst();
        lastAnnounceBurstTime = millis();

        if (announceBurstRemaining > 0) {
          announceBurstRemaining--;
        }
        if (announceBurstRemaining == 0) {
          announceBurstActive = false;
        }
      }

      if (millis() - lastPingTime >= pingInterval) {
        sendDiscoveryBurst();
        lastPingTime = millis();
      }
    }

    if (FSEQ_isFppOverrideActive()) {
      realtimeIP = getLastFppSenderIPSnapshot();
      realtimeLock(3000, REALTIME_MODE_FSEQ);
      FSEQPlayer::renderRealtimeFrame();
    } else if (realtimeMode == REALTIME_MODE_FSEQ && FSEQPlayer::isPlaying()) {
      stopRealtimeFppPlayback();
    } else if (!FSEQPlayer::isPlaying() && !FSEQ_isFppOverrideActive()) {
      clearPlaybackStatusCacheLoopOnly(false);
    }

    updatePlaybackStatusSnapshotLoopOnly();

    if (xlzStartTime == 0) xlzStartTime = millis();
    if (!xlzChecked && (millis() - xlzStartTime >= 2000)) {
      File root = SD_ADAPTER.open("/");
      if (root && root.isDirectory()) {
        root.close();
        if (!FSEQPlayer::isAnyPlaybackActive()) {
          XLZUnzip::processAllPendingXLZ();
          xlzChecked = true;
        }
      } else if (root) {
        root.close();
        xlzChecked = true;
      }
    }

    if (uploadSessionActive && xlzPendingScan && !xlzProcessing) {
      if (millis() - lastUploadActivity >= 10000) {
        if (FSEQPlayer::isAnyPlaybackActive()) return;
        xlzProcessing = true;
        XLZUnzip::processAllPendingXLZ();
        xlzProcessing = false;
        xlzPendingScan = false;
        uploadSessionActive = false;
      }
    }
  }

  uint16_t getId() override { return USERMOD_ID_FPP; }
  void addToConfig(JsonObject &root) override {}
  bool readFromConfig(JsonObject &root) override { return true; }
};

inline bool FPP_sendEffectSyncMessage(uint8_t action, const String &fileName,
                                      uint32_t currentFrame, float secondsElapsed,
                                      bool sendMulticast, bool sendBroadcast) {
  UsermodFPP *instance = FPP_usermodInstance();
  return instance ? instance->sendSyncMessage(action, fileName, currentFrame,
                                              secondsElapsed, sendMulticast,
                                              sendBroadcast)
                  : false;
}

inline const char UsermodFPP::_name[] PROGMEM = "FPP Connect";
