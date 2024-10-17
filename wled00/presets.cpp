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
static char quickLoad[9];
static char saveName[33];
static bool includeBri = true, segBounds = true, selectedOnly = false, playlistSave = false;

static const char *getFileName(bool persist = true) {
  return persist ? "/presets.json" : "/tmp.json";
}

bool presetsSavePending(void) {  // WLEDMM true if presetToSave, playlistSave or saveLedmap
  if (presetToSave > 0) return(true);
  if (playlistSave == true) return(true);
  if (saveLedmap >= 0) return(true);
  return(false);
}
bool presetsActionPending(void) {  // WLEDMM true if presetToApply, presetToSave, playlistSave or saveLedmap
  if (presetToApply > 0) return(true);
  if (presetToSave > 0) return(true);
  if (playlistSave == true) return(true);
  if (saveLedmap >= 0) return(true);
  return(false);
}

static void doSaveState() {
  bool persist = (presetToSave < 251);
  const char *filename = getFileName(persist);

  if (!requestJSONBufferLock(10)) return; // will set fileDoc

  initPresetsFile(); // just in case if someone deleted presets.json using /edit
  JsonObject sObj = doc.to<JsonObject>();

  DEBUG_PRINTLN(F("Serialize current state"));
  if (playlistSave) {
    serializePlaylist(sObj);
    if (includeBri) sObj["on"] = true;
  } else {
    serializeState(sObj, true, includeBri, segBounds, selectedOnly);
  }
  sObj["n"] = saveName;
  if (quickLoad[0]) sObj[F("ql")] = quickLoad;
  if (saveLedmap >= 0) sObj[F("ledmap")] = saveLedmap;
/*
  #ifdef WLED_DEBUG
    DEBUG_PRINTLN(F("Serialized preset"));
    serializeJson(doc,Serial);
    DEBUG_PRINTLN();
  #endif
*/
  #if defined(ARDUINO_ARCH_ESP32)
  if (!persist) {
    if (tmpRAMbuffer!=nullptr) free(tmpRAMbuffer);
    size_t len = measureJson(*fileDoc) + 1;
    DEBUG_PRINTLN(len);
    // if possible use SPI RAM on ESP32
    #if defined(BOARD_HAS_PSRAM) && (defined(WLED_USE_PSRAM) || defined(WLED_USE_PSRAM_JSON))        // WLEDMM
    if (psramFound())
      tmpRAMbuffer = (char*) ps_malloc(len);
    else
    #endif
      tmpRAMbuffer = (char*) malloc(len);
    if (tmpRAMbuffer!=nullptr) {
      serializeJson(*fileDoc, tmpRAMbuffer, len);
    } else {
      writeObjectToFileUsingId(filename, presetToSave, fileDoc);
    }
  } else
  #endif
  writeObjectToFileUsingId(filename, presetToSave, fileDoc);

  if (persist) presetsModifiedTime = toki.second(); //unix time
  releaseJSONBufferLock();
  updateFSInfo();

  // clean up
  saveLedmap   = -1;
  presetToSave = 0;
  saveName[0]  = '\0';
  quickLoad[0] = '\0';
  playlistSave = false;
}

bool getPresetName(byte index, String& name)
{
  if (!requestJSONBufferLock(19)) return false;
  bool presetExists = false;
  if (readObjectFromFileUsingId(getFileName(), index, &doc))
  {
    JsonObject fdo = doc.as<JsonObject>();
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
  if (WLED_FS.exists(getFileName())) return;

  StaticJsonDocument<64> doc;
  JsonObject sObj = doc.to<JsonObject>();
  sObj.createNestedObject("0");
  File f = WLED_FS.open(getFileName(), "w");
  if (!f) {
    errorFlag = ERR_FS_GENERAL;
    return;
  }
  serializeJson(doc, f);
  f.close();
}

bool applyPreset(byte index, byte callMode)
{
  DEBUG_PRINT(F("Request to apply preset: "));
  DEBUG_PRINTLN(index);
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
  if (presetToSave) {
    doSaveState();
    return;
  }

  if (presetToApply == 0 || fileDoc) return; // no preset waiting to apply, or JSON buffer is already allocated, return to loop until free

  bool changePreset = false;
  uint8_t tmpPreset = presetToApply; // store temporary since deserializeState() may call applyPreset()
  uint8_t tmpMode   = callModeToApply;

  JsonObject fdo;
  const char *filename = getFileName(tmpPreset < 255);

/*
 * The following code is no longer needed as handlePreset() is never run from
 * network callback.
 * **************************************************************************
 * 
  //crude way to determine if this was called by a network request
  uint8_t core = 1;
  #ifdef ARDUINO_ARCH_ESP32
    #if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2)
    // this does not make sense on single core
    core = xPortGetCoreID();
    // begin WLEDMM specific
	  //      loopTask (arduino main loop) sometimes runs on core #1
	  if ((core == 1) && (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) == 0)) {
		  DEBUG_PRINTF("[applyPreset] called from loopTask on core %d; forcing core = 0\n", (int)core); 
		  core = 0;
	  }
	  //      async_tcp (network requests) sometimes runs on core #0
	  if ((core == 0) && (strncmp(pcTaskGetTaskName(NULL), "async_tcp", 9) == 0)) {
		  DEBUG_PRINTF("[applyPreset] called from async_tcp on core %d; forcing core = 1\n", (int)core); 
		  core = 1;
	  }
	  // end WLEDMM specific
    #endif
  #endif
  //only allow use of fileDoc from the core responsible for network requests (AKA HTTP JSON API)
  //do not use active network request doc from preset called by main loop (playlist, schedule, ...)
  if (fileDoc && core && force && tmpPreset < 255) {
    DEBUG_PRINT(F("Force applying preset: "));
    DEBUG_PRINTLN(presetToApply);

    presetToApply     = 0; //clear request for preset
    callModeToApply   = 0;

    // this will overwrite doc with preset content but applyPreset() is the last in such case and content of doc is no longer needed
    errorFlag = readObjectFromFileUsingId(filename, tmpPreset, fileDoc) ? ERR_NONE : ERR_FS_PLOAD;

    JsonObject fdo = fileDoc->as<JsonObject>();

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
      if (!fdo["seg"].isNull()) unloadPlaylist(); // if preset contains "seg" we must unload playlist
      if (!fdo["seg"].isNull() || !fdo["on"].isNull() || !fdo["bri"].isNull() || !fdo["ps"].isNull() || !fdo[F("playlist")].isNull()) changePreset = true;
      fdo.remove("ps"); //remove load request for presets to prevent recursive crash

      deserializeState(fdo, tmpMode, tmpPreset);  // may call applyPreset() which will overwrite presetToApply
    }

    if (!errorFlag && changePreset) presetCycCurr = currentPreset = tmpPreset;

    colorUpdated(tmpMode);
    return;
  }

  if (force) return; // something went wrong with force option (most likely WS request), quit and wait for async load
*/
  // allocate buffer
  if (!requestJSONBufferLock(9)) return;  // will also assign fileDoc

  presetToApply = 0; //clear request for preset
  callModeToApply = 0;
  byte presetErrorFlag = ERR_NONE;

  DEBUG_PRINT(F("Applying preset: "));
  DEBUG_PRINTLN(tmpPreset);

  #ifdef ARDUINO_ARCH_ESP32
  if (tmpPreset==255 && tmpRAMbuffer!=nullptr) {
    deserializeJson(*fileDoc,tmpRAMbuffer);
    if ((errorFlag == ERR_FS_PLOAD) || (errorFlag == ERR_JSON)) errorFlag = ERR_NONE;  // WLEDMM only reset our own error
  } else
  #endif
  {
    presetErrorFlag = readObjectFromFileUsingId(filename, tmpPreset, fileDoc) ? ERR_NONE : ERR_FS_PLOAD;
    if ((errorFlag == ERR_FS_PLOAD) || (errorFlag == ERR_JSON)) errorFlag = ERR_NONE;  // WLEDMM only reset our own error
    if (presetErrorFlag == ERR_FS_PLOAD) errorFlag = presetErrorFlag;
  }
  fdo = fileDoc->as<JsonObject>();

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
  if (!presetErrorFlag && tmpPreset < 255 && changePreset) currentPreset = tmpPreset;

  #if defined(ARDUINO_ARCH_ESP32)
  //Aircoookie recommended not to delete buffer
  if (tmpPreset==255 && tmpRAMbuffer!=nullptr) {
    free(tmpRAMbuffer);
    tmpRAMbuffer = nullptr;
  }
  #endif

  releaseJSONBufferLock(); // will also clear fileDoc
  if (changePreset) notify(tmpMode); // force UDP notification
  stateUpdated(tmpMode);  // was colorUpdated() if anything breaks
  updateInterfaces(tmpMode);
}

//called from handleSet(PS=) [network callback (fileDoc==nullptr), IR (irrational), deserializeState, UDP] and deserializeState() [network callback (filedoc!=nullptr)]
void savePreset(byte index, const char* pname, JsonObject sObj)
{
  if (index == 0 || (index > 250 && index < 255)) return;
  if (pname) strlcpy(saveName, pname, 33);
  else {
    if (sObj["n"].is<const char*>()) strlcpy(saveName, sObj["n"].as<const char*>(), 33);
    else                             sprintf_P(saveName, PSTR("Preset %d"), index);
  }

  DEBUG_PRINT(F("Saving preset (")); DEBUG_PRINT(index); DEBUG_PRINT(F(") ")); DEBUG_PRINTLN(saveName);

  presetToSave = index;
  playlistSave = false;
  if (sObj[F("ql")].is<const char*>()) strlcpy(quickLoad, sObj[F("ql")].as<const char*>(), 9); // client limits QL to 2 chars, buffer for 8 bytes to allow unicode

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
      if (index > 250 || !fileDoc) return; // cannot save API calls to temporary preset (255)
      sObj.remove("o");
      sObj.remove("v");
      sObj.remove("time");
      sObj.remove(F("error"));
      sObj.remove(F("psave"));
      if (sObj["n"].isNull()) sObj["n"] = saveName;
      initPresetsFile(); // just in case if someone deleted presets.json using /edit
      writeObjectToFileUsingId(getFileName(index<255), index, fileDoc);
      presetsModifiedTime = toki.second(); //unix time
      updateFSInfo();
    } else {
      // store playlist
      // WARNING: playlist will be loaded in json.cpp after this call and will have repeat counter increased by 1
      includeBri   = true; // !sObj["on"].isNull();
      playlistSave = true;
    }
  }
}

void deletePreset(byte index) {
  StaticJsonDocument<24> empty;
  writeObjectToFileUsingId(getFileName(), index, &empty);
  presetsModifiedTime = toki.second(); //unix time
  updateFSInfo();
}