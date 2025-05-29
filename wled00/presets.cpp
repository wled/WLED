#include "wled.h"

/*
 * Methods to handle saving and loading presets to/from the filesystem
 */

#ifdef ARDUINO_ARCH_ESP32
static char *tmpRAMbuffer = nullptr;
#endif

static volatile byte presetToApply = 0;
static volatile byte callModeToApply = 0;
static volatile byte presetToSave = 0;
static volatile int8_t saveLedmap = -1;
static char *quickLoad = nullptr;
static char *saveName = nullptr;
static bool includeBri = true, segBounds = true, selectedOnly = false, playlistSave = false;;

static const char presets_json[] PROGMEM = "/presets.json";
static const char tmp_json[] PROGMEM = "/tmp.json";
const char *getPresetsFileName(bool persistent) {
  return persistent ? presets_json : tmp_json;
}

bool presetNeedsSaving() {
  return presetToSave;
}

static void doSaveState() {
  bool persist = (presetToSave < 251);

  unsigned long start = millis();
  while (strip.isUpdating() && millis()-start < (2*FRAMETIME_FIXED)+1) yield(); // wait 2 frames
  if (!requestJSONBufferLock(10)) return;

  initPresetsFile(); // just in case if someone deleted presets.json using /edit
  JsonObject sObj = pDoc->to<JsonObject>();

  DEBUG_PRINTLN(F("Serialize current state"));
  if (playlistSave) {
    serializePlaylist(sObj);
    if (includeBri) sObj["on"] = true;
  } else {
    serializeState(sObj, true, includeBri, segBounds, selectedOnly);
  }
  if (saveName) sObj["n"] = saveName;
  else          sObj["n"] = F("Unkonwn preset"); // should not happen, but just in case...
  if (quickLoad && quickLoad[0]) sObj[F("ql")] = quickLoad;
  if (saveLedmap >= 0) sObj[F("ledmap")] = saveLedmap;
/*
  #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Serialized preset"));
    serializeJson(*pDoc,Serial);
    DEBUG_PRINTLN();
  #endif
*/
  #if defined(ARDUINO_ARCH_ESP32)
  if (!persist) {
    if (tmpRAMbuffer!=nullptr) free(tmpRAMbuffer);
    size_t len = measureJson(*pDoc) + 1;
    DEBUG_PRINTLN(len);
    // if possible use SPI RAM on ESP32
    if (psramSafe && psramFound())
      tmpRAMbuffer = (char*) ps_malloc(len);
    else
      tmpRAMbuffer = (char*) malloc(len);
    if (tmpRAMbuffer!=nullptr) {
      serializeJson(*pDoc, tmpRAMbuffer, len);
    } else {
      writeObjectToFileUsingId(getPresetsFileName(persist), presetToSave, pDoc);
    }
  } else
  #endif
  writeObjectToFileUsingId(getPresetsFileName(persist), presetToSave, pDoc);

  if (persist) presetsModifiedTime = toki.second(); //unix time
  releaseJSONBufferLock();
  updateFSInfo();

  // clean up
  saveLedmap   = -1;
  presetToSave = 0;
  free(saveName);
  free(quickLoad);
  saveName = nullptr;
  quickLoad = nullptr;
  playlistSave = false;
}

bool getPresetName(byte index, String& name)
{
  if (!requestJSONBufferLock(19)) return false;
  bool presetExists = false;
  if (readObjectFromFileUsingId(getPresetsFileName(), index, pDoc)) {
    JsonObject fdo = pDoc->as<JsonObject>();
    if (fdo["n"]) {
      name = (const char*)(fdo["n"]);
      presetExists = true;
    }
  }
  releaseJSONBufferLock();
  return presetExists;
}

void initPresetsFile()
{
  char fileName[33]; strncpy_P(fileName, getPresetsFileName(), 32); fileName[32] = 0; //use PROGMEM safe copy as FS.open() does not
  if (WLED_FS.exists(fileName)) return;

  StaticJsonDocument<64> doc;
  JsonObject sObj = doc.to<JsonObject>();
  sObj.createNestedObject("0");
  File f = WLED_FS.open(fileName, "w");
  if (!f) {
    errorFlag = ERR_FS_GENERAL;
    return;
  }
  serializeJson(doc, f);
  f.close();
}

bool applyPresetFromPlaylist(byte index)
{
  DEBUG_PRINTF_P(PSTR("Request to apply preset: %d\n"), index);
  presetToApply = index;
  callModeToApply = CALL_MODE_DIRECT_CHANGE;
  return true;
}

bool applyPreset(byte index, byte callMode)
{
  unloadPlaylist(); // applying a preset unloads the playlist (#3827)
  DEBUG_PRINTF_P(PSTR("Request to apply preset: %u\n"), index);
  presetToApply = index;
  callModeToApply = callMode;
  return true;
}

// apply preset or fallback to a effect and palette if it doesn't exist
void applyPresetWithFallback(uint8_t index, uint8_t callMode, uint8_t effectID, uint8_t paletteID)
{
  applyPreset(index, callMode);
  //these two will be overwritten if preset exists in handlePresets()
  effectCurrent = effectID;
  effectPalette = paletteID;
}

void handlePresets()
{
  byte presetErrFlag = ERR_NONE;
  if (presetToSave) {
    strip.suspend();
    doSaveState();
    strip.resume();
    return;
  }

  if (presetToApply == 0 || !requestJSONBufferLock(9)) return; // no preset waiting to apply, or JSON buffer is already allocated, return to loop until free

  bool changePreset = false;
  uint8_t tmpPreset = presetToApply; // store temporary since deserializeState() may call applyPreset()
  uint8_t tmpMode   = callModeToApply;

  JsonObject fdo;

  presetToApply = 0; //clear request for preset
  callModeToApply = 0;

  DEBUG_PRINTF_P(PSTR("Applying preset: %u\n"), (unsigned)tmpPreset);

  #if defined(ARDUINO_ARCH_ESP32S3) || defined(ARDUINO_ARCH_ESP32S2) || defined(ARDUINO_ARCH_ESP32C3)
  unsigned long start = millis();
  while (strip.isUpdating() && millis() - start < FRAMETIME_FIXED) yield(); // wait for strip to finish updating, accessing FS during sendout causes glitches
  #endif

  #ifdef ARDUINO_ARCH_ESP32
  if (tmpPreset==255 && tmpRAMbuffer!=nullptr) {
    deserializeJson(*pDoc,tmpRAMbuffer);
  } else
  #endif
  {
  presetErrFlag = readObjectFromFileUsingId(getPresetsFileName(tmpPreset < 255), tmpPreset, pDoc) ? ERR_NONE : ERR_FS_PLOAD;
  }
  fdo = pDoc->as<JsonObject>();

  // only reset errorflag if previous error was preset-related
  if ((errorFlag == ERR_NONE) || (errorFlag == ERR_FS_PLOAD)) errorFlag = presetErrFlag;

  //HTTP API commands
  const char* httpwin = fdo["win"];
  if (httpwin) {
    String apireq = "win"; // reduce flash string usage
    apireq += F("&IN&"); // internal call
    apireq += httpwin;
    handleSet(nullptr, apireq, false); // may call applyPreset() via PL=
    setValuesFromFirstSelectedSeg(); // fills legacy values
    changePreset = true;
  } else {
    if (!fdo["seg"].isNull() || !fdo["on"].isNull() || !fdo["bri"].isNull() || !fdo["nl"].isNull() || !fdo["ps"].isNull() || !fdo[F("playlist")].isNull()) changePreset = true;
    if (!(tmpMode == CALL_MODE_BUTTON_PRESET && fdo["ps"].is<const char *>() && strchr(fdo["ps"].as<const char *>(),'~') != strrchr(fdo["ps"].as<const char *>(),'~')))
      fdo.remove("ps"); // remove load request for presets to prevent recursive crash (if not called by button and contains preset cycling string "1~5~")
    deserializeState(fdo, CALL_MODE_NO_NOTIFY, tmpPreset); // may change presetToApply by calling applyPreset()
  }
  if (!errorFlag && tmpPreset < 255 && changePreset) currentPreset = tmpPreset;

  #if defined(ARDUINO_ARCH_ESP32)
  //Aircoookie recommended not to delete buffer
  if (tmpPreset==255 && tmpRAMbuffer!=nullptr) {
    free(tmpRAMbuffer);
    tmpRAMbuffer = nullptr;
  }
  #endif

  releaseJSONBufferLock();
  if (changePreset) notify(tmpMode); // force UDP notification
  stateUpdated(tmpMode);  // was colorUpdated() if anything breaks
  updateInterfaces(tmpMode);
}

//called from handleSet(PS=) [network callback (sObj is empty), IR (irrational), deserializeState, UDP] and deserializeState() [network callback (filedoc!=nullptr)]
void savePreset(byte index, const char* pname, JsonObject sObj)
{
  if (!saveName) saveName = static_cast<char*>(malloc(33));
  if (!quickLoad) quickLoad = static_cast<char*>(malloc(9));
  if (!saveName || !quickLoad) return;

  if (index == 0 || (index > 250 && index < 255)) return;
  if (pname) strlcpy(saveName, pname, 33);
  else {
    if (sObj["n"].is<const char*>()) strlcpy(saveName, sObj["n"].as<const char*>(), 33);
    else                             sprintf_P(saveName, PSTR("Preset %d"), index);
  }

  DEBUG_PRINTF_P(PSTR("Saving preset (%d) %s\n"), index, saveName);

  presetToSave = index;
  playlistSave = false;
  if (sObj[F("ql")].is<const char*>()) strlcpy(quickLoad, sObj[F("ql")].as<const char*>(), 9); // client limits QL to 2 chars, buffer for 8 bytes to allow unicode
  else quickLoad[0] = 0;

  const char *bootPS = PSTR("bootps");
  if (!sObj[FPSTR(bootPS)].isNull()) {
    bootPreset = sObj[FPSTR(bootPS)] | bootPreset;
    sObj.remove(FPSTR(bootPS));
    configNeedsWrite = true;
  }

  if (sObj.size()==0 || sObj["o"].isNull()) { // no "o" means not a playlist or custom API call, saving of state is async (not immediately)
    includeBri   = sObj["ib"].as<bool>() || sObj.size()==0 || index==255; // temporary preset needs brightness
    segBounds    = sObj["sb"].as<bool>() || sObj.size()==0 || index==255; // temporary preset needs bounds
    selectedOnly = sObj[F("sc")].as<bool>();
    saveLedmap   = sObj[F("ledmap")] | -1;
  } else {
    // this is a playlist or API call
    if (sObj[F("playlist")].isNull()) {
      // we will save API call immediately (often causes presets.json corruption)
      presetToSave = 0;
      if (index <= 250) { // cannot save API calls to temporary preset (255)
        sObj.remove("o");
        sObj.remove("v");
        sObj.remove("time");
        sObj.remove(F("error"));
        sObj.remove(F("psave"));
        if (sObj["n"].isNull()) sObj["n"] = saveName;
        initPresetsFile(); // just in case if someone deleted presets.json using /edit
        writeObjectToFileUsingId(getPresetsFileName(), index, pDoc);
        presetsModifiedTime = toki.second(); //unix time
        updateFSInfo();
      }
      free(saveName);
      free(quickLoad);
      saveName = nullptr;
      quickLoad = nullptr;
    } else {
      // store playlist
      // WARNING: playlist will be loaded in json.cpp after this call and will have repeat counter increased by 1 it will also be randomised if selected
      includeBri   = true; // !sObj["on"].isNull();
      playlistSave = true;
    }
  }
}

void deletePreset(byte index) {
  StaticJsonDocument<24> empty;
  writeObjectToFileUsingId(getPresetsFileName(), index, &empty);
  presetsModifiedTime = toki.second(); //unix time
  updateFSInfo();
}