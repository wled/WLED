#pragma once

// --- Macro Definitions ---
#ifndef USED_STORAGE_FILESYSTEMS
  #ifdef WLED_USE_SD_SPI
    #define USED_STORAGE_FILESYSTEMS "SD SPI, LittleFS"
  #else
    #define USED_STORAGE_FILESYSTEMS "SD MMC, LittleFS"
  #endif
#endif

#ifndef SD_ADAPTER
  #if defined(WLED_USE_SD) || defined(WLED_USE_SD_SPI)
    #ifdef WLED_USE_SD_SPI
      #ifndef WLED_USE_SD
        #define WLED_USE_SD
      #endif
      #ifndef WLED_PIN_SCK
        #define WLED_PIN_SCK SCK
      #endif
      #ifndef WLED_PIN_MISO
        #define WLED_PIN_MISO MISO
      #endif
      #ifndef WLED_PIN_MOSI
        #define WLED_PIN_MOSI MOSI
      #endif
      #ifndef WLED_PIN_SS
        #define WLED_PIN_SS SS
      #endif
      #define SD_ADAPTER SD
    #else
      #define SD_ADAPTER SD_MMC
    #endif
  #endif
#endif

#ifndef REALTIME_MODE_FSEQ
  #define REALTIME_MODE_FSEQ 3
#endif

#ifdef WLED_USE_SD_SPI
  #ifndef SPI_PORT_DEFINED
    inline SPIClass spiPort = SPIClass(VSPI);
    #define SPI_PORT_DEFINED
  #endif
#endif

#include "wled.h"
#ifdef WLED_USE_SD_SPI
  #include <SPI.h>
  #include <SD.h>
#elif defined(WLED_USE_SD_MMC)
  #include "SD_MMC.h"
#endif

// --- FSEQ Playback Logic ---
#ifndef RECORDING_REPEAT_LOOP
  #define RECORDING_REPEAT_LOOP -1
#endif
#ifndef RECORDING_REPEAT_DEFAULT
  #define RECORDING_REPEAT_DEFAULT 0
#endif

class FSEQFile {
public:
  struct file_header_t {
    uint8_t  identifier[4];       
    uint16_t channel_data_offset; 
    uint8_t  minor_version;       
    uint8_t  major_version;       
    uint16_t header_length;       
    uint32_t channel_count;       
    uint32_t frame_count;         
    uint8_t  step_time;           
    uint8_t  flags;
  };

  static void handlePlayRecording();
  static void loadRecording(const char* filepath, uint16_t startLed, uint16_t stopLed);
  static void clearLastPlayback();

private:
  FSEQFile() {};
  static const int V1FSEQ_MINOR_VERSION = 0;
  static const int V1FSEQ_MAJOR_VERSION = 1;
  static const int V2FSEQ_MINOR_VERSION = 0;
  static const int V2FSEQ_MAJOR_VERSION = 2;
  static const int FSEQ_DEFAULT_STEP_TIME = 50;

  static File     recordingFile;
  static uint8_t  colorChannels;
  static int32_t  recordingRepeats;
  static uint32_t now;
  static uint32_t next_time;
  static uint16_t playbackLedStart;
  static uint16_t playbackLedStop;
  static uint32_t frame;
  static uint16_t buffer_size;
  static file_header_t file_header;

  static inline uint32_t readUInt32() {
    char buffer[4];
    if (recordingFile.readBytes(buffer, 4) < 4) return 0;
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
  }
  static inline uint32_t readUInt24() {
    char buffer[3];
    if (recordingFile.readBytes(buffer, 3) < 3) return 0;
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16);
  }
  static inline uint16_t readUInt16() {
    char buffer[2];
    if (recordingFile.readBytes(buffer, 2) < 2) return 0;
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
  }
  static inline uint8_t readUInt8() {
    char buffer[1];
    if (recordingFile.readBytes(buffer, 1) < 1) return 0;
    return (uint8_t)buffer[0];
  }

  static bool fileOnSD(const char* filepath) {
    uint8_t cardType = SD_ADAPTER.cardType();
    if(cardType == CARD_NONE) return false;
    return SD_ADAPTER.exists(filepath);
  }
  static bool fileOnFS(const char* filepath) {
    return false; // Only using SD
  }
  static void printHeaderInfo() {
    DEBUG_PRINTLN("FSEQ file_header:");
    DEBUG_PRINTF(" channel_data_offset = %d\n", file_header.channel_data_offset);
    DEBUG_PRINTF(" minor_version       = %d\n", file_header.minor_version);
    DEBUG_PRINTF(" major_version       = %d\n", file_header.major_version);
    DEBUG_PRINTF(" header_length       = %d\n", file_header.header_length);
    DEBUG_PRINTF(" channel_count       = %d\n", file_header.channel_count);
    DEBUG_PRINTF(" frame_count         = %d\n", file_header.frame_count);
    DEBUG_PRINTF(" step_time           = %d\n", file_header.step_time);
    DEBUG_PRINTF(" flags               = %d\n", file_header.flags);
  }
  static void processFrameData() {
    uint16_t packetLength = file_header.channel_count;
    uint16_t lastLed = min(playbackLedStop, uint16_t(playbackLedStart + (packetLength / 3)));
    char frame_data[buffer_size];
    CRGB* crgb = reinterpret_cast<CRGB*>(frame_data);
    uint16_t bytes_remaining = packetLength;
    uint16_t index = playbackLedStart;
    while (index < lastLed && bytes_remaining > 0) {
      uint16_t length = min(bytes_remaining, buffer_size);
      recordingFile.readBytes(frame_data, length);
      bytes_remaining -= length;
      for (uint16_t offset = 0; offset < length / 3; offset++) {
        setRealtimePixel(index, crgb[offset].r, crgb[offset].g, crgb[offset].b, 0);
        if (++index > lastLed) break;
      }
    }
    strip.show();
    realtimeLock(3000, REALTIME_MODE_FSEQ);
    next_time = now + file_header.step_time;
  }
  static bool stopBecauseAtTheEnd() {
    if (!recordingFile.available()) {
      if (recordingRepeats == RECORDING_REPEAT_LOOP) {
        recordingFile.seek(0);
      } else if (recordingRepeats > 0) {
        recordingFile.seek(0);
        recordingRepeats--;
        DEBUG_PRINTF("Repeat recording again for: %d\n", recordingRepeats);
      } else {
        DEBUG_PRINTLN("Finished playing recording, disabling realtime mode");
        realtimeLock(10, REALTIME_MODE_INACTIVE);
        recordingFile.close();
        clearLastPlayback();
        return true;
      }
    }
    return false;
  }
  static void playNextRecordingFrame() {
    if (stopBecauseAtTheEnd()) return;
    uint32_t offset = file_header.channel_count * frame++;
    offset += file_header.channel_data_offset; 
    if (!recordingFile.seek(offset)) {
      if (recordingFile.position() != offset) {
        DEBUG_PRINTLN("Failed to seek to proper offset for channel data!");
        return;
      }
    }
    processFrameData();
  }
};

// Definícia statických premenných FSEQFile
File     FSEQFile::recordingFile;
uint8_t  FSEQFile::colorChannels = 3;
int32_t  FSEQFile::recordingRepeats = RECORDING_REPEAT_DEFAULT;
uint32_t FSEQFile::now = 0;
uint32_t FSEQFile::next_time = 0;
uint16_t FSEQFile::playbackLedStart = 0;
uint16_t FSEQFile::playbackLedStop = uint16_t(-1);
uint32_t FSEQFile::frame = 0;
uint16_t FSEQFile::buffer_size = 48;
FSEQFile::file_header_t FSEQFile::file_header;

// Verejné metódy FSEQFile
void FSEQFile::handlePlayRecording() {
  now = millis();
  if (realtimeMode != REALTIME_MODE_FSEQ) return;
  if (now < next_time) return;
  playNextRecordingFrame();
}

void FSEQFile::loadRecording(const char* filepath, uint16_t startLed, uint16_t stopLed) {
  if (recordingFile.available()) {
    clearLastPlayback();
    recordingFile.close();
  }
  playbackLedStart = startLed;
  playbackLedStop = stopLed;
  if (playbackLedStart == uint16_t(-1) || playbackLedStop == uint16_t(-1)) {
    Segment sg = strip.getSegment(-1);
    playbackLedStart = sg.start;
    playbackLedStop = sg.stop;
  }
  DEBUG_PRINTF("FSEQ load animation on LED %d to %d\n", playbackLedStart, playbackLedStop);
  if (fileOnSD(filepath)) {
    DEBUG_PRINTF("Read file from SD: %s\n", filepath);
    recordingFile = SD_ADAPTER.open(filepath, "rb");
  } else if (fileOnFS(filepath)) {
    DEBUG_PRINTF("Read file from FS: %s\n", filepath);
  } else {
    DEBUG_PRINTF("File %s not found (%s)\n", filepath, USED_STORAGE_FILESYSTEMS);
    return;
  }
  if ((uint64_t)recordingFile.available() < sizeof(file_header)) {
    DEBUG_PRINTF("Invalid file size: %d\n", recordingFile.available());
    recordingFile.close();
    return;
  }
  for (int i = 0; i < 4; i++) {
    file_header.identifier[i] = readUInt8();
  }
  file_header.channel_data_offset = readUInt16();
  file_header.minor_version = readUInt8();
  file_header.major_version = readUInt8();
  file_header.header_length = readUInt16();
  file_header.channel_count = readUInt32();
  file_header.frame_count = readUInt32();
  file_header.step_time = readUInt8();
  file_header.flags = readUInt8();
  printHeaderInfo();
  if (file_header.identifier[0] != 'P' || file_header.identifier[1] != 'S' ||
      file_header.identifier[2] != 'E' || file_header.identifier[3] != 'Q') {
    DEBUG_PRINTF("Error reading FSEQ file %s header, invalid identifier\n", filepath);
    recordingFile.close();
    return;
  }
  if (((uint64_t)file_header.channel_count * (uint64_t)file_header.frame_count) + file_header.header_length > UINT32_MAX) {
    DEBUG_PRINTF("Error reading FSEQ file %s header, file too long (max 4gb)\n", filepath);
    recordingFile.close();
    return;
  }
  if (file_header.step_time < 1) {
    DEBUG_PRINTF("Invalid step time %d, using default %d instead\n", file_header.step_time, FSEQ_DEFAULT_STEP_TIME);
    file_header.step_time = FSEQ_DEFAULT_STEP_TIME;
  }
  if (realtimeOverride == REALTIME_OVERRIDE_ONCE) {
    realtimeOverride = REALTIME_OVERRIDE_NONE;
  }
  recordingRepeats = RECORDING_REPEAT_DEFAULT;
  playNextRecordingFrame();
}

void FSEQFile::clearLastPlayback() {
  for (uint16_t i = playbackLedStart; i < playbackLedStop; i++) {
    setRealtimePixel(i, 0, 0, 0, 0);
  }
  frame = 0;
}

// --- Web UI pre FSEQ prehrávanie a SD ---
class UsermodFseq : public Usermod {
private:
  bool sdInitDone = false;

#ifdef WLED_USE_SD_SPI
  // Statické konfiguračné premenné – dostupné aj inde (napr. cez getter metódy)
  static int8_t configPinSourceSelect; // CS
  static int8_t configPinSourceClock;  // SCK
  static int8_t configPinPoci;         // MISO
  static int8_t configPinPico;         // MOSI

  void init_SD_SPI() {
    if(sdInitDone) return;
    PinManagerPinType pins[4] = {
      { configPinSourceSelect, true },
      { configPinSourceClock, true },
      { configPinPoci, false },
      { configPinPico, true }
    };
    if (!PinManager::allocateMultiplePins(pins, 4, PinOwner::UM_SdCard)) {
      DEBUG_PRINTF("[%s] SPI pin allocation failed!\n", _name);
      return;
    }
    spiPort.begin(configPinSourceClock, configPinPoci, configPinPico, configPinSourceSelect);
    if(!SD_ADAPTER.begin(configPinSourceSelect, spiPort)) {
      DEBUG_PRINTF("[%s] SPI begin failed!\n", _name);
      return;
    }
    sdInitDone = true;
    DEBUG_PRINTF("[%s] SD SPI initialized\n", _name);
  }

  void deinit_SD_SPI() {
    if(!sdInitDone) return;
    SD_ADAPTER.end();
    PinManager::deallocatePin(configPinSourceSelect, PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinSourceClock,  PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinPoci,         PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinPico,         PinOwner::UM_SdCard);
    sdInitDone = false;
    DEBUG_PRINTF("[%s] SD SPI deinitalized\n", _name);
  }

  void reinit_SD_SPI() {
    deinit_SD_SPI();
    init_SD_SPI();
  }
#endif

#ifdef WLED_USE_SD_MMC
  void init_SD_MMC() {
    if(sdInitDone) return;
    if(SD_ADAPTER.begin()) {
      sdInitDone = true;
      DEBUG_PRINTF("[%s] SD MMC initialized\n", _name);
    } else {
      DEBUG_PRINTF("[%s] SD MMC begin failed!\n", _name);
    }
  }
#endif

  void listFiles(const char* dirname, String &result) {
    DEBUG_PRINTF("[%s] Listing directory: %s\n", _name, dirname);
    File root = SD_ADAPTER.open(dirname);
    if (!root) {
      result += "<li>Failed to open directory: " + String(dirname) + "</li>";
      return;
    }
    if (!root.isDirectory()){
      result += "<li>Not a directory: " + String(dirname) + "</li>";
      return;
    }
    File file = root.openNextFile();
    while (file) {
      result += "<li>" + String(file.name()) + " (" + String(file.size()) + " bytes) ";
      result += "<a href='/sd/delete?path=" + String(file.name()) + "'>Delete</a></li>";
      DEBUG_PRINTF("[%s] Found file: %s, size: %d bytes\n", _name, file.name(), file.size());
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  void handleUploadFile(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;
    String path = filename;
    if (!filename.startsWith("/")) {
      path = "/" + filename;
    }
    if(index == 0) {
      DEBUG_PRINTF("[%s] Starting upload for file: %s\n", _name, path.c_str());
      uploadFile = SD_ADAPTER.open(path.c_str(), FILE_WRITE);
      if (!uploadFile) {
        DEBUG_PRINTF("[%s] Failed to open file for writing: %s\n", _name, path.c_str());
      } else {
        DEBUG_PRINTF("[%s] File opened successfully for writing: %s\n", _name, path.c_str());
      }
    }
    if(uploadFile) {
      size_t written = uploadFile.write(data, len);
      DEBUG_PRINTF("[%s] Writing %d bytes to file: %s (written: %d bytes)\n", _name, len, path.c_str(), written);
    } else {
      DEBUG_PRINTF("[%s] Cannot write, file not open: %s\n", _name, path.c_str());
    }
    if(final) {
      if(uploadFile) {
        uploadFile.close();
        DEBUG_PRINTF("[%s] Upload complete and file closed: %s\n", _name, path.c_str());
        if(SD_ADAPTER.exists(path.c_str())) {
          DEBUG_PRINTF("[%s] File exists on SD card: %s\n", _name, path.c_str());
        } else {
          DEBUG_PRINTF("[%s] File does NOT exist on SD card after upload: %s\n", _name, path.c_str());
        }
      } else {
        DEBUG_PRINTF("[%s] Upload complete but file was not open: %s\n", _name, path.c_str());
      }
    }
  }

public:
  static const char _name[];

  // Verejné statické getter metódy, aby iné časti kódu mohli získať tieto hodnoty
#ifdef WLED_USE_SD_SPI
  static int8_t getCsPin()   { return configPinSourceSelect; }
  static int8_t getSckPin()  { return configPinSourceClock; }
  static int8_t getMisoPin() { return configPinPoci; }
  static int8_t getMosiPin() { return configPinPico; }
#endif

  void setup() {
    DEBUG_PRINTF("[%s] Usermod loaded\n", _name);
#ifdef WLED_USE_SD_SPI
    init_SD_SPI();
#elif defined(WLED_USE_SD_MMC)
    init_SD_MMC();
#endif
    if(sdInitDone) {
      DEBUG_PRINTF("[%s] SD initialization successful.\n", _name);
    } else {
      DEBUG_PRINTF("[%s] SD initialization FAILED.\n", _name);
    }
    
    // Web Endpoints pre správu SD & FSEQ
    server.on("/sd/ui", HTTP_GET, [this](AsyncWebServerRequest *request) {
      String html = "<html><head><meta charset='utf-8'><title>SD & FSEQ Manager</title>";
      html += "<style>";
      html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
      html += "h1 { margin-top: 0; }";
      html += "ul { list-style: none; padding: 0; margin: 0 0 20px 0; }";
      html += "li { margin-bottom: 10px; }";
      html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
      html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
      html += "</style></head><body>";
      html += "<h1>SD & FSEQ Manager</h1>";
      html += "<ul>";
      html += "<li><a href='/sd/list'>SD Files</a></li>";
      html += "<li><a href='/fseq/list'>FSEQ Files</a></li>";
      html += "</ul>";
      html += "<a href='/'>BACK</a>";
      html += "</body></html>";
      request->send(200, "text/html", html);
    });
    
    server.on("/sd/list", HTTP_GET, [this](AsyncWebServerRequest *request) {
      DEBUG_PRINTF("[%s] /sd/list endpoint requested\n", _name);
      String html = "<html><head><meta charset='utf-8'><title>SD Card Files</title>";
      html += "<style>";
      html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
      html += "h1 { margin-top: 0; }";
      html += "ul { list-style: none; margin: 0; padding: 0; }";
      html += "li { margin-bottom: 10px; }";
      html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
      html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
      html += ".deleteLink { border-color: #FF0000; color: #FF0000; }";
      html += ".deleteLink:hover { background-color: #FF0000; color: #000; }";
      html += ".backLink { border: 2px solid #00FF00; padding: 10px 20px; }";
      html += "</style></head><body>";
      html += "<h1>SD Card Files</h1><ul>";
    
      File root = SD_ADAPTER.open("/");
      if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while(file) {
          String name = file.name();
          html += "<li>" + name + " (" + String(file.size()) + " bytes) ";
          html += "<a href='#' class='deleteLink' onclick=\"deleteFile('" + name + "')\">Delete</a></li>";
          file.close();
          file = root.openNextFile();
        }
      }
      root.close();
    
      html += "</ul>";
      html += "<h2>Upload File</h2>";
      html += "<form id='uploadForm' enctype='multipart/form-data'>";
      html += "Select file: <input type='file' name='upload'><br><br>";
      html += "<input type='submit' value='Upload'>";
      html += "</form>";
      html += "<div id='uploadStatus'></div>";
      html += "<p><a href='/sd/ui'>BACK</a></p>";
      html += "<script>";
      html += "document.getElementById('uploadForm').addEventListener('submit', function(e) {";
      html += "  e.preventDefault();";
      html += "  var formData = new FormData(this);";
      html += "  document.getElementById('uploadStatus').innerText = 'Uploading...';";
      html += "  fetch('/sd/upload', { method: 'POST', body: formData })";
      html += "    .then(response => response.text())";
      html += "    .then(data => {";
      html += "      document.getElementById('uploadStatus').innerText = data;";
      html += "      setTimeout(function() { location.reload(); }, 1000);";
      html += "    })";
      html += "    .catch(err => {";
      html += "      document.getElementById('uploadStatus').innerText = 'Upload failed';";
      html += "    });";
      html += "});";
      html += "function deleteFile(filename) {";
      html += "  if (!confirm('Are you sure you want to delete ' + filename + '?')) return;";
      html += "  fetch('/sd/delete?path=' + encodeURIComponent(filename))";
      html += "    .then(response => response.text())";
      html += "    .then(data => { alert(data); location.reload(); })";
      html += "    .catch(err => { alert('Delete failed'); });";
      html += "}";
      html += "</script>";
      html += "</body></html>";
      request->send(200, "text/html", html);
    });
    
    server.on("/sd/upload", HTTP_POST, [this](AsyncWebServerRequest *request) {
      DEBUG_PRINTF("[%s] /sd/upload HTTP_POST endpoint requested\n", _name);
      request->send(200, "text/plain", "Upload complete");
    }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      handleUploadFile(request, filename, index, data, len, final);
    });
    
    server.on("/sd/delete", HTTP_GET, [this](AsyncWebServerRequest *request) {
      DEBUG_PRINTF("[%s] /sd/delete endpoint requested\n", _name);
      if (!request->hasArg("path")) {
        request->send(400, "text/plain", "Missing 'path' parameter");
        return;
      }
      String path = request->arg("path");
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
      bool res = SD_ADAPTER.remove(path.c_str());
      DEBUG_PRINTF("[%s] Delete file request: %s, result: %d\n", _name, path.c_str(), res);
      String msg = res ? "File deleted" : "Delete failed";
      request->send(200, "text/plain", msg);
    });
    
    server.on("/fseq/list", HTTP_GET, [this](AsyncWebServerRequest *request) {
      DEBUG_PRINTF("[%s] /fseq/list endpoint requested\n", _name);
      String html = "<html><head><meta charset='utf-8'><title>FSEQ Files</title>";
      html += "<style>";
      html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
      html += "h1 { margin-top: 0; }";
      html += "ul { list-style: none; margin: 0; padding: 0; }";
      html += "li { margin-bottom: 10px; }";
      html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
      html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
      html += "</style></head><body>";
      html += "<h1>FSEQ Files</h1><ul>";
      
      File root = SD_ADAPTER.open("/");
      if(root && root.isDirectory()){
        File file = root.openNextFile();
        while(file){
          String name = file.name();
          if(name.endsWith(".fseq") || name.endsWith(".FSEQ")){
            html += "<li>" + name + " ";
            html += "<button id='btn_" + name + "' onclick=\"toggleFseq('" + name + "')\">Play</button>";
            html += "</li>";
          }
          file.close();
          file = root.openNextFile();
        }
      }
      root.close();
      html += "</ul>";
      html += "<p><a href='/sd/ui' class='backLink'>BACK</a></p>";
      html += "<script>";
      html += "function toggleFseq(file){";
      html += "  var btn = document.getElementById('btn_' + file);";
      html += "  if(btn.innerText === 'Play'){";
      html += "    fetch('/fseq/start?file=' + encodeURIComponent(file))";
      html += "      .then(response => response.text())";
      html += "      .then(data => { btn.innerText = 'Stop'; });";
      html += "  } else {";
      html += "    fetch('/fseq/stop?file=' + encodeURIComponent(file))";
      html += "      .then(response => response.text())";
      html += "      .then(data => { btn.innerText = 'Play'; });";
      html += "  }";
      html += "}";
      html += "</script>";
      html += "</body></html>";
      request->send(200, "text/html", html);
    });
    
    server.on("/fseq/start", HTTP_GET, [this](AsyncWebServerRequest *request) {
      if (!request->hasArg("file")) {
        request->send(400, "text/plain", "Missing 'file' parameter");
        return;
      }
      String filepath = request->arg("file");
      if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
      }
      uint16_t startLed = 0;
      uint16_t stopLed  = uint16_t(-1);
      DEBUG_PRINTF("[%s] Starting FSEQ file: %s\n", _name, filepath.c_str());
      FSEQFile::loadRecording(filepath.c_str(), startLed, stopLed);
      request->send(200, "text/plain", "FSEQ started: " + filepath);
    });
    
    server.on("/fseq/stop", HTTP_GET, [this](AsyncWebServerRequest *request) {
      FSEQFile::clearLastPlayback();
      realtimeLock(10, REALTIME_MODE_INACTIVE);
      DEBUG_PRINTF("[%s] FSEQ playback stopped\n", _name);
      request->send(200, "text/plain", "FSEQ stopped");
    });
  }

  void loop() {
    FSEQFile::handlePlayRecording();
  }

  uint16_t getId() {
    return USERMOD_ID_SD_CARD;  // Uistite sa, že máte unikátne ID
  }

  // Pridanie nastavení usermodu do WLED config
  void addToConfig(JsonObject &root) {
    #ifdef WLED_USE_SD_SPI
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top["csPin"]  = configPinSourceSelect;
    top["sckPin"] = configPinSourceClock;
    top["misoPin"] = configPinPoci;
    top["mosiPin"] = configPinPico;
    #endif
  }

  bool readFromConfig(JsonObject &root) {
    #ifdef WLED_USE_SD_SPI
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) return false;
    if (top["csPin"].is<int>())   configPinSourceSelect = top["csPin"].as<int>();
    if (top["sckPin"].is<int>())  configPinSourceClock  = top["sckPin"].as<int>();
    if (top["misoPin"].is<int>()) configPinPoci         = top["misoPin"].as<int>();
    if (top["mosiPin"].is<int>()) configPinPico         = top["mosiPin"].as<int>();

    // Re-inicializácia SD pre použitie nových pinov
    reinit_SD_SPI();
    return true;
    #else
    return false;
    #endif
  }
};

const char UsermodFseq::_name[] PROGMEM = "SD & FSEQ Web";

// Definícia statických konfiguračných premenných (platí pre SD SPI)
#ifdef WLED_USE_SD_SPI
int8_t UsermodFseq::configPinSourceSelect = 5;
int8_t UsermodFseq::configPinSourceClock  = 18;
int8_t UsermodFseq::configPinPoci         = 19;
int8_t UsermodFseq::configPinPico         = 23;
#endif