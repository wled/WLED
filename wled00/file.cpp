#include "wled.h"

/*
 * Utility for SPIFFS filesystem
 */

#ifdef ARDUINO_ARCH_ESP32 //FS info bare IDF function until FS wrapper is available for ESP32
#if WLED_FS != LITTLEFS && ESP_IDF_VERSION_MAJOR < 4
  #include "esp_spiffs.h"
#endif
#endif

#define FS_BUFSIZE 256

/*
 * Structural requirements for files managed by writeObjectToFile() and readObjectFromFile() utilities:
 * 1. File must be a string representation of a valid JSON object
 * 2. File must have '{' as first character
 * 3. There must not be any additional characters between a root-level key and its value object (e.g. space, tab, newline)
 * 4. There must not be any characters between an root object-separating ',' and the next object key string
 * 5. There may be any number of spaces, tabs, and/or newlines before such object-separating ','
 * 6. There must not be more than 5 consecutive spaces at any point except for those permitted in condition 5
 * 7. If it is desired to delete the first usable object (e.g. preset file), a dummy object '"0":{}' is inserted at the beginning.
 *    It shall be disregarded by receiving software.
 *    The reason for it is that deleting the first preset would require special code to handle commas between it and the 2nd preset
 */

// There are no consecutive spaces longer than this in the file, so if more space is required, findSpace() can return false immediately
// Actual space may be lower
constexpr size_t MAX_SPACE = UINT16_MAX * 2U;           // smallest supported config has 128Kb flash size
static volatile size_t knownLargestSpace = MAX_SPACE;

static File f; // don't export to other cpp files

//wrapper to find out how long closing takes
void closeFile() {
  #ifdef WLED_DEBUG_FS
    DEBUGFS_PRINT(F("Close -> "));
    uint32_t s = millis();
  #endif
  f.close();
  DEBUGFS_PRINTF("took %lu ms\n", millis() - s);
  doCloseFile = false;
}

//find() that reads and buffers data from file stream in 256-byte blocks.
//Significantly faster, f.find(key) can take SECONDS for multi-kB files
static bool bufferedFind(const char *target, bool fromStart = true) {
  #ifdef WLED_DEBUG_FS
    DEBUGFS_PRINT("Find ");
    DEBUGFS_PRINTLN(target);
    uint32_t s = millis();
  #endif

  if (!f || !f.size()) return false;
  size_t targetLen = strlen(target);

  size_t index = 0;
  byte buf[FS_BUFSIZE];
  if (fromStart) f.seek(0);

  while (f.position() < f.size() -1) {
    size_t bufsize = f.read(buf, FS_BUFSIZE); // better to use size_t instead if uint16_t
    size_t count = 0;
    while (count < bufsize) {
      if(buf[count] != target[index])
      index = 0; // reset index if any char does not match

      if(buf[count] == target[index]) {
        if(++index >= targetLen) { // return true if all chars in the target match
          f.seek((f.position() - bufsize) + count +1);
          DEBUGFS_PRINTF("Found at pos %d, took %lu ms", f.position(), millis() - s);
          return true;
        }
      }
      count++;
    }
  }
  DEBUGFS_PRINTF("No match, took %lu ms\n", millis() - s);
  return false;
}

//find empty spots in file stream in 256-byte blocks.
static bool bufferedFindSpace(size_t targetLen, bool fromStart = true) {

  #ifdef WLED_DEBUG_FS
    DEBUGFS_PRINTF("Find %d spaces\n", targetLen);
    uint32_t s = millis();
  #endif

  if (knownLargestSpace < targetLen) {
    DEBUGFS_PRINT(F("No match, KLS "));
    DEBUGFS_PRINTLN(knownLargestSpace);
    return false;
  }

  if (!f || !f.size()) return false;

  size_t index = 0; // better to use size_t instead if uint16_t
  byte buf[FS_BUFSIZE];
  if (fromStart) f.seek(0);

  while (f.position() < f.size() -1) {
    size_t bufsize = f.read(buf, FS_BUFSIZE);
    size_t count = 0;

    while (count < bufsize) {
      if(buf[count] == ' ') {
        if(++index >= targetLen) { // return true if space long enough
          if (fromStart) {
            f.seek((f.position() - bufsize) + count +1 - targetLen);
            knownLargestSpace = MAX_SPACE; //there may be larger spaces after, so we don't know
          }
          DEBUGFS_PRINTF("Found at pos %d, took %lu ms", f.position(), millis() - s);
          return true;
        }
      } else {
        if (!fromStart) return false;
        if (index) {
          if (knownLargestSpace < index || (knownLargestSpace == MAX_SPACE)) knownLargestSpace = index;
          index = 0; // reset index if not space
        }
      }

      count++;
    }
  }
  DEBUGFS_PRINTF("No match, took %lu ms\n", millis() - s);
  return false;
}

//find the closing bracket corresponding to the opening bracket at the file pos when calling this function
static bool bufferedFindObjectEnd() {
  #ifdef WLED_DEBUG_FS
    DEBUGFS_PRINTLN(F("Find obj end"));
    uint32_t s = millis();
  #endif

  if (!f || !f.size()) return false;

  unsigned objDepth = 0; //num of '{' minus num of '}'. return once 0
  //size_t start = f.position();
  byte buf[FS_BUFSIZE];

  while (f.position() < f.size() -1) {
    size_t bufsize = f.read(buf, FS_BUFSIZE); // better to use size_t instead of uint16_t
    size_t count = 0;

    while (count < bufsize) {
      if (buf[count] == '{') objDepth++;
      if (buf[count] == '}') objDepth--;
      if (objDepth == 0) {
        f.seek((f.position() - bufsize) + count +1);
        DEBUGFS_PRINTF("} at pos %d, took %lu ms", f.position(), millis() - s);
        return true;
      }
      count++;
    }
  }
  DEBUGFS_PRINTF("No match, took %lu ms\n", millis() - s);
  return false;
}

//fills n bytes from current file pos with ' ' characters
static void writeSpace(size_t l)
{
  byte buf[FS_BUFSIZE];
  memset(buf, ' ', FS_BUFSIZE);

  while (l > 0) {
    size_t block = (l>FS_BUFSIZE) ? FS_BUFSIZE : l;
    f.write(buf, block);
    l -= block;
  }

  if (knownLargestSpace < l) knownLargestSpace = l;
}

static bool appendObjectToFile(const char* key, const JsonDocument* content, uint32_t s, uint32_t contentLen = 0)
{
  #ifdef WLED_DEBUG_FS
    DEBUGFS_PRINTLN(F("Append"));
    uint32_t s1 = millis();
  #endif
  uint32_t pos = 0;
  if (!f) return false;

  if (f.size() < 3) {
    char init[10];
    strcpy_P(init, PSTR("{\"0\":{}}"));
    f.print(init);
  }

  if (content->isNull()) {
    doCloseFile = true;
    return true; //nothing  to append
  }

  //if there is enough empty space in file, insert there instead of appending
  if (!contentLen) contentLen = measureJson(*content);
  DEBUGFS_PRINTF("CLen %d\n", contentLen);
  if (bufferedFindSpace(contentLen + strlen(key) + 1)) {
    if (f.position() > 2) f.write(','); //add comma if not first object
    f.print(key);
    serializeJson(*content, f);
    DEBUGFS_PRINTF("Inserted, took %lu ms (total %lu)", millis() - s1, millis() - s);
    doCloseFile = true;
    return true;
  }

  //not enough space, append at end

  //permitted space for presets exceeded
  updateFSInfo();

  if (f.size() + 9000 > (fsBytesTotal - fsBytesUsed)) { //make sure there is enough space to at least copy the file once
    errorFlag = ERR_FS_QUOTA;
    doCloseFile = true;
    return false;
  }

  //check if last character in file is '}' (typical)
  uint32_t eof = f.size() -1;
  f.seek(eof, SeekSet);
  if (f.read() == '}') pos = eof;

  if (pos == 0) //not found
  {
    DEBUGFS_PRINTLN(F("not }"));
    f.seek(0);
    while (bufferedFind("}",false)) //find last closing bracket in JSON if not last char
    {
      pos = f.position();
    }
    if (pos > 0) pos--;
  }
  DEBUGFS_PRINT("pos "); DEBUGFS_PRINTLN(pos);
  if (pos > 2)
  {
    f.seek(pos, SeekSet);
    f.write(',');
  } else { //file content is not valid JSON object
    f.seek(0, SeekSet);
    f.print('{'); //start JSON
  }

  f.print(key);

  //Append object
  serializeJson(*content, f);
  f.write('}');

  doCloseFile = true;
  DEBUGFS_PRINTF("Appended, took %lu ms (total %lu)", millis() - s1, millis() - s);
  return true;
}

bool writeObjectToFileUsingId(const char* file, uint16_t id, const JsonDocument* content)
{
  char objKey[10];
  sprintf(objKey, "\"%d\":", id);
  return writeObjectToFile(file, objKey, content);
}

bool writeObjectToFile(const char* file, const char* key, const JsonDocument* content)
{
  uint32_t s = 0; //timing
  #ifdef WLED_DEBUG_FS
    DEBUGFS_PRINTF("Write to %s with key %s >>>\n", file, (key==nullptr)?"nullptr":key);
    serializeJson(*content, Serial); DEBUGFS_PRINTLN();
    s = millis();
  #endif

  size_t pos = 0;
  char fileName[129]; strncpy_P(fileName, file, 128); fileName[128] = 0; //use PROGMEM safe copy as FS.open() does not
  f = WLED_FS.open(fileName, WLED_FS.exists(fileName) ? "r+" : "w+");
  if (!f) {
    DEBUGFS_PRINTLN(F("Failed to open!"));
    return false;
  }

  if (!bufferedFind(key)) //key does not exist in file
  {
    return appendObjectToFile(key, content, s);
  }

  //an object with this key already exists, replace or delete it
  pos = f.position();
  //measure out end of old object
  bufferedFindObjectEnd();
  size_t pos2 = f.position();

  uint32_t oldLen = pos2 - pos;
  DEBUGFS_PRINTF("Old obj len %d\n", oldLen);

  //Three cases:
  //1. The new content is null, overwrite old obj with spaces
  //2. The new content is smaller than the old, overwrite and fill diff with spaces
  //3. The new content is larger than the old, but smaller than old + trailing spaces, overwrite with new
  //4. The new content is larger than old + trailing spaces, delete old and append

  size_t contentLen = 0;
  if (!content->isNull()) contentLen = measureJson(*content);

  if (contentLen && contentLen <= oldLen) { //replace and fill diff with spaces
    DEBUGFS_PRINTLN(F("replace"));
    f.seek(pos);
    serializeJson(*content, f);
    writeSpace(pos2 - f.position());
  } else if (contentLen && bufferedFindSpace(contentLen - oldLen, false)) { //enough leading spaces to replace
    DEBUGFS_PRINTLN(F("replace (trailing)"));
    f.seek(pos);
    serializeJson(*content, f);
  } else {
    DEBUGFS_PRINTLN(F("delete"));
    pos -= strlen(key);
    if (pos > 3) pos--; //also delete leading comma if not first object
    f.seek(pos);
    writeSpace(pos2 - pos);
    if (contentLen) return appendObjectToFile(key, content, s, contentLen);
  }

  doCloseFile = true;
  DEBUGFS_PRINTF("Replaced/deleted, took %lu ms\n", millis() - s);
  return true;
}

bool readObjectFromFileUsingId(const char* file, uint16_t id, JsonDocument* dest, const JsonDocument* filter)
{
  char objKey[10];
  sprintf(objKey, "\"%d\":", id);
  return readObjectFromFile(file, objKey, dest, filter);
}

//if the key is a nullptr, deserialize entire object
bool readObjectFromFile(const char* file, const char* key, JsonDocument* dest, const JsonDocument* filter)
{
  if (doCloseFile) closeFile();
  #ifdef WLED_DEBUG_FS
    DEBUGFS_PRINTF("Read from %s with key %s >>>\n", file, (key==nullptr)?"nullptr":key);
    uint32_t s = millis();
  #endif
  char fileName[129]; strncpy_P(fileName, file, 128); fileName[128] = 0; //use PROGMEM safe copy as FS.open() does not
  f = WLED_FS.open(fileName, "r");
  if (!f) return false;

  if (key != nullptr && !bufferedFind(key)) //key does not exist in file
  {
    f.close();
    dest->clear();
    DEBUGFS_PRINTLN(F("Obj not found."));
    return false;
  }

  if (filter) deserializeJson(*dest, f, DeserializationOption::Filter(*filter));
  else        deserializeJson(*dest, f);

  f.close();
  DEBUGFS_PRINTF("Read, took %lu ms\n", millis() - s);
  return true;
}

void updateFSInfo() {
  #ifdef ARDUINO_ARCH_ESP32
    #if WLED_FS == LITTLEFS || ESP_IDF_VERSION_MAJOR >= 4
    fsBytesTotal = WLED_FS.totalBytes();
    fsBytesUsed = WLED_FS.usedBytes();
    #else
    esp_spiffs_info(nullptr, &fsBytesTotal, &fsBytesUsed);
    #endif
  #else
    FSInfo fsi;
    WLED_FS.info(fsi);
    fsBytesUsed  = fsi.usedBytes;
    fsBytesTotal = fsi.totalBytes;
  #endif
}


#ifdef ARDUINO_ARCH_ESP32
// caching presets in PSRAM may prevent occasional flashes seen when HomeAssitant polls WLED
// original idea by @akaricchi (https://github.com/Akaricchi)
// returns a pointer to the PSRAM buffer, updates size parameter
static const uint8_t *getPresetCache(size_t &size) {
  static unsigned long presetsCachedTime = 0;
  static uint8_t *presetsCached = nullptr;
  static size_t presetsCachedSize = 0;
  static byte presetsCachedValidate = 0;

  //if (presetsModifiedTime != presetsCachedTime) DEBUG_PRINTLN(F("getPresetCache(): presetsModifiedTime changed."));
  //if (presetsCachedValidate != cacheInvalidate) DEBUG_PRINTLN(F("getPresetCache(): cacheInvalidate changed."));

  if ((presetsModifiedTime != presetsCachedTime) || (presetsCachedValidate != cacheInvalidate)) {
    if (presetsCached) {
      p_free(presetsCached);
      presetsCached = nullptr;
    }
  }

  if (!presetsCached) {
    File file = WLED_FS.open(FPSTR(getPresetsFileName()), "r");
    if (file) {
      presetsCachedTime = presetsModifiedTime;
      presetsCachedValidate = cacheInvalidate;
      presetsCachedSize = 0;
      presetsCached = (uint8_t*)p_malloc(file.size() + 1);
      if (presetsCached) {
        presetsCachedSize = file.size();
        file.read(presetsCached, presetsCachedSize);
        presetsCached[presetsCachedSize] = 0;
        file.close();
      }
    }
  }

  size = presetsCachedSize;
  return presetsCached;
}
#endif

bool handleFileRead(AsyncWebServerRequest* request, String path){
  DEBUGFS_PRINT(F("WS FileRead: ")); DEBUGFS_PRINTLN(path);
  if(path.endsWith("/")) path += "index.htm";
  if(path.indexOf(F("sec")) > -1) return false;
  #ifdef ARDUINO_ARCH_ESP32
  if (psramSafe && psramFound() && path.endsWith(FPSTR(getPresetsFileName()))) {
    size_t psize;
    const uint8_t *presets = getPresetCache(psize);
    if (presets) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, FPSTR(CONTENT_TYPE_JSON), presets, psize);
      request->send(response);
      return true;
    }
  }
  #endif
  if(WLED_FS.exists(path) || WLED_FS.exists(path + ".gz")) {
    request->send(request->beginResponse(WLED_FS, path, {}, request->hasArg(F("download")), {}));
    return true;
  }
  return false;
}

// copy a file, delete destination file if incomplete to prevent corrupted files
bool copyFile(const char* src_path, const char* dst_path) {
  DEBUG_PRINTF("copyFile from %s to %s\n", src_path, dst_path);
  if(!WLED_FS.exists(src_path)) {
   DEBUG_PRINTLN(F("file not found"));
   return false;
  }

  bool success = true; // is set to false on error
  File src = WLED_FS.open(src_path, "r");
  File dst = WLED_FS.open(dst_path, "w");

  if (src && dst) {
    uint8_t buf[128]; // copy file in 128-byte blocks
    while (src.available() > 0) {
      size_t bytesRead = src.read(buf, sizeof(buf));
      if (bytesRead == 0) {
        success = false;
        break; // error, no data read
      }
      size_t bytesWritten = dst.write(buf, bytesRead);
      if (bytesWritten != bytesRead) {
        success = false;
        break; // error, not all data written
      }
    }
  } else {
    success = false; // error, could not open files
  }
  if(src) src.close();
  if(dst) dst.close();
  if (!success) {
    DEBUG_PRINTLN(F("copy failed"));
    WLED_FS.remove(dst_path); // delete incomplete file
  }
  return success;
}

// compare two files, return true if identical
bool compareFiles(const char* path1, const char* path2) {
  DEBUG_PRINTF("compareFile %s and %s\n", path1, path2);
  if (!WLED_FS.exists(path1) || !WLED_FS.exists(path2)) {
    DEBUG_PRINTLN(F("file not found"));
    return false;
  }

  bool identical = true; // set to false on mismatch
  File f1 = WLED_FS.open(path1, "r");
  File f2 = WLED_FS.open(path2, "r");

  if (f1 && f2) {
    uint8_t buf1[128], buf2[128];
    while (f1.available() > 0 || f2.available() > 0) {
      size_t len1 = f1.read(buf1, sizeof(buf1));
      size_t len2 = f2.read(buf2, sizeof(buf2));

      if (len1 != len2) {
        identical = false;
        break; // files differ in size or read failed
      }

      if (memcmp(buf1, buf2, len1) != 0) {
        identical = false;
        break; // files differ in content
      }
    }
  } else {
    identical = false; // error opening files
  }

  if (f1) f1.close();
  if (f2) f2.close();
  return identical;
}

static const char s_backup_fmt[] PROGMEM = "/bkp.%s";

bool backupFile(const char* filename) {
  DEBUG_PRINTF("backup %s \n", filename);
  if (!validateJsonFile(filename)) {
    DEBUG_PRINTLN(F("broken file"));
    return false;
  }
  char backupname[32];
  snprintf_P(backupname, sizeof(backupname), s_backup_fmt, filename + 1); // skip leading '/' in filename

  if (copyFile(filename, backupname)) {
    DEBUG_PRINTLN(F("backup ok"));
    return true;
  }
  DEBUG_PRINTLN(F("backup failed"));
  return false;
}

bool restoreFile(const char* filename) {
  DEBUG_PRINTF("restore %s \n", filename);
  char backupname[32];
  snprintf_P(backupname, sizeof(backupname), s_backup_fmt, filename + 1); // skip leading '/' in filename

  if (!WLED_FS.exists(backupname)) {
    DEBUG_PRINTLN(F("no backup found"));
    return false;
  }

  if (!validateJsonFile(backupname)) {
    DEBUG_PRINTLN(F("broken backup"));
    return false;
  }

  if (copyFile(backupname, filename)) {
    DEBUG_PRINTLN(F("restore ok"));
    return true;
  }
  DEBUG_PRINTLN(F("restore failed"));
  return false;
}

bool validateJsonFile(const char* filename) {
  if (!WLED_FS.exists(filename)) return false;
  File file = WLED_FS.open(filename, "r");
  if (!file) return false;
  StaticJsonDocument<0> doc, filter; // https://arduinojson.org/v6/how-to/validate-json/
  bool result = deserializeJson(doc, file, DeserializationOption::Filter(filter)) == DeserializationError::Ok;
  file.close();
  if (!result) {
    DEBUG_PRINTF_P(PSTR("Invalid JSON file %s\n"), filename);
  } else {
    DEBUG_PRINTF_P(PSTR("Valid JSON file %s\n"), filename);
  }
  return result;
}

// print contents of all files in root dir to Serial except wsec files
void dumpFilesToSerial() {
  File rootdir = WLED_FS.open("/", "r");
  File rootfile = rootdir.openNextFile();
  while (rootfile) {
    size_t len = strlen(rootfile.name());
    // skip files starting with "wsec" and dont end in .json
    if (strncmp(rootfile.name(), "wsec", 4) != 0 && len >= 6 && strcmp(rootfile.name() + len - 5, ".json") == 0) {
      Serial.println(rootfile.name());
      while (rootfile.available()) {
        Serial.write(rootfile.read());
      }
      Serial.println();
      Serial.println();
    }
    rootfile.close();
    rootfile = rootdir.openNextFile();
  }
}

