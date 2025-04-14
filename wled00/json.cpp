#include "wled.h"

#include "palettes.h"

/*
 * JSON API (De)serialization
 */

bool deserializeSegment(JsonObject elem, byte it, byte presetId)
{
  byte id = elem["id"] | it;
  if (id >= strip.getMaxSegments()) return false;

  bool newSeg = false;
  int stop = elem["stop"] | -1;

  // append segment
  if (id >= strip.getSegmentsNum()) {
    if (stop <= 0) return false; // ignore empty/inactive segments
    strip.appendSegment(Segment(0, strip.getLengthTotal()));
    id = strip.getSegmentsNum()-1; // segments are added at the end of list
    newSeg = true;
  }

  //DEBUG_PRINTLN(F("-- JSON deserialize segment."));
  Segment& seg = strip.getSegment(id);
  //DEBUG_PRINTF_P(PSTR("--  Original segment: %p (%p)\n"), &seg, seg.data);
  const Segment prev = seg; //make a backup so we can tell if something changed (calling copy constructor)
  //DEBUG_PRINTF_P(PSTR("--  Duplicate segment: %p (%p)\n"), &prev, prev.data);

  int start = elem["start"] | seg.start;
  if (stop < 0) {
    int len = elem["len"];
    stop = (len > 0) ? start + len : seg.stop;
  }
  // 2D segments
  int startY = elem["startY"] | seg.startY;
  int stopY = elem["stopY"] | seg.stopY;

  //repeat, multiplies segment until all LEDs are used, or max segments reached
  bool repeat = elem["rpt"] | false;
  if (repeat && stop>0) {
    elem.remove("id");  // remove for recursive call
    elem.remove("rpt"); // remove for recursive call
    elem.remove("n");   // remove for recursive call
    unsigned len = stop - start;
    for (size_t i=id+1; i<strip.getMaxSegments(); i++) {
      start = start + len;
      if (start >= strip.getLengthTotal()) break;
      //TODO: add support for 2D
      elem["start"] = start;
      elem["stop"]  = start + len;
      elem["rev"]   = !elem["rev"]; // alternate reverse on even/odd segments
      deserializeSegment(elem, i, presetId); // recursive call with new id
    }
    return true;
  }

  if (elem["n"]) {
    // name field exists
    if (seg.name) { //clear old name
      free(seg.name);
      seg.name = nullptr;
    }

    const char * name = elem["n"].as<const char*>();
    size_t len = 0;
    if (name != nullptr) len = strlen(name);
    if (len > 0) {
      if (len > WLED_MAX_SEGNAME_LEN) len = WLED_MAX_SEGNAME_LEN;
      seg.name = static_cast<char*>(malloc(len+1));
      if (seg.name) strlcpy(seg.name, name, WLED_MAX_SEGNAME_LEN+1);
    } else {
      // but is empty (already deleted above)
      elem.remove("n");
    }
  } else if (start != seg.start || stop != seg.stop) {
    // clearing or setting segment without name field
    if (seg.name) {
      free(seg.name);
      seg.name = nullptr;
    }
  }

  uint16_t grp       = elem["grp"] | seg.grouping;
  uint16_t spc       = elem[F("spc")] | seg.spacing;
  uint16_t of        = seg.offset;
  uint8_t  soundSim  = elem["si"] | seg.soundSim;
  uint8_t  map1D2D   = elem["m12"] | seg.map1D2D;
  uint8_t  set       = elem[F("set")] | seg.set;
  bool     selected  = getBoolVal(elem["sel"], seg.selected);
  bool     reverse   = getBoolVal(elem["rev"], seg.reverse);
  bool     mirror    = getBoolVal(elem["mi"] , seg.mirror);
  #ifndef WLED_DISABLE_2D
  bool     reverse_y = getBoolVal(elem["rY"]   , seg.reverse_y);
  bool     mirror_y  = getBoolVal(elem["mY"]   , seg.mirror_y);
  bool     transpose = getBoolVal(elem[F("tp")], seg.transpose);
  #endif

  int len = (stop > start) ? stop - start : 1;
  int offset = elem[F("of")] | INT32_MAX;
  if (offset != INT32_MAX) {
    int offsetAbs = abs(offset);
    if (offsetAbs > len - 1) offsetAbs %= len;
    if (offset < 0) offsetAbs = len - offsetAbs;
    of = offsetAbs;
  }
  if (stop > start && of > len -1) of = len -1;

  // update segment (delete if necessary)
  seg.setGeometry(start, stop, grp, spc, of, startY, stopY, map1D2D); // strip needs to be suspended for this to work without issues

  if (newSeg) seg.refreshLightCapabilities(); // fix for #3403

  if (seg.reset && seg.stop == 0) {
    if (id == strip.getMainSegmentId()) strip.setMainSegmentId(0); // fix for #3403
    return true; // segment was deleted & is marked for reset, no need to change anything else
  }

  byte segbri = seg.opacity;
  if (getVal(elem["bri"], &segbri)) {
    if (segbri > 0) seg.setOpacity(segbri);
    seg.setOption(SEG_OPTION_ON, segbri); // use transition
  }

  seg.setOption(SEG_OPTION_ON, getBoolVal(elem["on"], seg.on)); // use transition
  seg.freeze = getBoolVal(elem["frz"], seg.freeze);

  seg.setCCT(elem["cct"] | seg.cct);

  JsonArray colarr = elem["col"];
  if (!colarr.isNull())
  {
    if (seg.getLightCapabilities() & 3) {
      // segment has RGB or White
      for (size_t i = 0; i < NUM_COLORS; i++) {
        // JSON "col" array can contain the following values for each of segment's colors (primary, background, custom):
        // "col":[int|string|object|array, int|string|object|array, int|string|object|array]
        //   int = Kelvin temperature or 0 for black
        //   string = hex representation of [WW]RRGGBB
        //   object = individual channel control {"r":0,"g":127,"b":255,"w":255}, each being optional (valid to send {})
        //   array = direct channel values [r,g,b,w] (w element being optional)
        int rgbw[] = {0,0,0,0};
        bool colValid = false;
        JsonArray colX = colarr[i];
        if (colX.isNull()) {
          JsonObject oCol = colarr[i];
          if (!oCol.isNull()) {
            // we have a JSON object for color {"w":123,"r":123,...}; allows individual channel control
            rgbw[0] = oCol["r"] | R(seg.colors[i]);
            rgbw[1] = oCol["g"] | G(seg.colors[i]);
            rgbw[2] = oCol["b"] | B(seg.colors[i]);
            rgbw[3] = oCol["w"] | W(seg.colors[i]);
            colValid = true;
          } else {
            byte brgbw[] = {0,0,0,0};
            const char* hexCol = colarr[i];
            if (hexCol == nullptr) { //Kelvin color temperature (or invalid), e.g 2400
              int kelvin = colarr[i] | -1;
              if (kelvin <  0) continue;
              if (kelvin == 0) seg.setColor(i, 0);
              if (kelvin >  0) colorKtoRGB(kelvin, brgbw);
              colValid = true;
            } else { //HEX string, e.g. "FFAA00"
              colValid = colorFromHexString(brgbw, hexCol);
            }
            for (size_t c = 0; c < 4; c++) rgbw[c] = brgbw[c];
          }
        } else { //Array of ints (RGB or RGBW color), e.g. [255,160,0]
          byte sz = colX.size();
          if (sz == 0) continue; //do nothing on empty array
          copyArray(colX, rgbw, 4);
          colValid = true;
        }

        if (!colValid) continue;

        seg.setColor(i, RGBW32(rgbw[0],rgbw[1],rgbw[2],rgbw[3]));
        if (seg.mode == FX_MODE_STATIC) strip.trigger(); //instant refresh
      }
    } else {
      // non RGB & non White segment (usually On/Off bus)
      seg.setColor(0, ULTRAWHITE);
      seg.setColor(1, BLACK);
    }
  }

  // lx parser
  #ifdef WLED_ENABLE_LOXONE
  int lx = elem[F("lx")] | -1;
  if (lx >= 0) {
    parseLxJson(lx, id, false);
  }
  int ly = elem[F("ly")] | -1;
  if (ly >= 0) {
    parseLxJson(ly, id, true);
  }
  #endif

  //seg.map1D2D   = constrain(map1D2D, 0, 7); // done in setGeometry()
  seg.set       = constrain(set, 0, 3);
  seg.soundSim  = constrain(soundSim, 0, 3);
  seg.selected  = selected;
  seg.reverse   = reverse;
  seg.mirror    = mirror;
  #ifndef WLED_DISABLE_2D
  seg.reverse_y = reverse_y;
  seg.mirror_y  = mirror_y;
  seg.transpose = transpose;
  #endif

  byte fx = seg.mode;
  if (getVal(elem["fx"], &fx, 0, strip.getModeCount())) {
    if (!presetId && currentPlaylist>=0) unloadPlaylist();
    if (fx != seg.mode) seg.setMode(fx, elem[F("fxdef")]);
  }

  getVal(elem["sx"], &seg.speed);
  getVal(elem["ix"], &seg.intensity);

  uint8_t pal = seg.palette;
  if (seg.getLightCapabilities() & 1) {  // ignore palette for White and On/Off segments
    if (getVal(elem["pal"], &pal, 0, strip.getPaletteCount())) seg.setPalette(pal);
  }

  getVal(elem["c1"], &seg.custom1);
  getVal(elem["c2"], &seg.custom2);
  uint8_t cust3 = seg.custom3;
  getVal(elem["c3"], &cust3, 0, 31); // we can't pass reference to bitfield
  seg.custom3 = constrain(cust3, 0, 31);

  seg.check1 = getBoolVal(elem["o1"], seg.check1);
  seg.check2 = getBoolVal(elem["o2"], seg.check2);
  seg.check3 = getBoolVal(elem["o3"], seg.check3);

  JsonArray iarr = elem[F("i")]; //set individual LEDs
  if (!iarr.isNull()) {
    uint8_t oldMap1D2D = seg.map1D2D;
    seg.map1D2D = M12_Pixels; // no mapping

    // set brightness immediately and disable transition
    jsonTransitionOnce = true;
    seg.stopTransition();
    strip.setTransition(0);
    strip.setBrightness(scaledBri(bri), true);

    // freeze and init to black
    if (!seg.freeze) {
      seg.freeze = true;
      seg.fill(BLACK);
    }

    start = 0, stop = 0;
    set = 0; //0 nothing set, 1 start set, 2 range set

    for (size_t i = 0; i < iarr.size(); i++) {
      if(iarr[i].is<JsonInteger>()) {
        if (!set) {
          start = abs(iarr[i].as<int>());
          set++;
        } else {
          stop = abs(iarr[i].as<int>());
          set++;
        }
      } else { //color
        uint8_t rgbw[] = {0,0,0,0};
        JsonArray icol = iarr[i];
        if (!icol.isNull()) { //array, e.g. [255,0,0]
          byte sz = icol.size();
          if (sz > 0 && sz < 5) copyArray(icol, rgbw);
        } else { //hex string, e.g. "FF0000"
          byte brgbw[] = {0,0,0,0};
          const char* hexCol = iarr[i];
          if (colorFromHexString(brgbw, hexCol)) {
            for (size_t c = 0; c < 4; c++) rgbw[c] = brgbw[c];
          }
        }

        if (set < 2 || stop <= start) stop = start + 1;
        uint32_t c = gamma32(RGBW32(rgbw[0], rgbw[1], rgbw[2], rgbw[3]));
        while (start < stop) seg.setPixelColor(start++, c);
        set = 0;
      }
    }
    seg.map1D2D = oldMap1D2D; // restore mapping
    strip.trigger(); // force segment update
  }
  // send UDP/WS if segment options changed (except selection; will also deselect current preset)
  if (seg.differs(prev) & 0x7F) stateChanged = true;

  return true;
}

// deserializes WLED state
// presetId is non-0 if called from handlePreset()
bool deserializeState(JsonObject root, byte callMode, byte presetId)
{
  bool stateResponse = root[F("v")] | false;

  #if defined(WLED_DEBUG) && defined(WLED_DEBUG_HOST)
  netDebugEnabled = root[F("debug")] | netDebugEnabled;
  #endif

  bool onBefore = bri;
  getVal(root["bri"], &bri);

  bool on = root["on"] | (bri > 0);
  if (!on != !bri) toggleOnOff();

  if (root["on"].is<const char*>() && root["on"].as<const char*>()[0] == 't') {
    if (onBefore || !bri) toggleOnOff(); // do not toggle off again if just turned on by bri (makes e.g. "{"on":"t","bri":32}" work)
  }

  if (bri && !onBefore) { // unfreeze all segments when turning on
    for (size_t s=0; s < strip.getSegmentsNum(); s++) {
      strip.getSegment(s).freeze = false;
    }
    if (realtimeMode && !realtimeOverride && useMainSegmentOnly) { // keep live segment frozen if live
      strip.getMainSegment().freeze = true;
    }
  }

  long tr = -1;
  if (!presetId || currentPlaylist < 0) { //do not apply transition time from preset if playlist active, as it would override playlist transition times
    tr = root[F("transition")] | -1;
    if (tr >= 0) {
      transitionDelay = tr * 100;
      strip.setTransition(transitionDelay);
    }
  }

#ifndef WLED_DISABLE_MODE_BLEND
  blendingStyle = root[F("bs")] | blendingStyle;
  blendingStyle &= 0x1F;
#endif

  // temporary transition (applies only once)
  tr = root[F("tt")] | -1;
  if (tr >= 0) {
    jsonTransitionOnce = true;
    strip.setTransition(tr * 100);
  }

  tr = root[F("tb")] | -1;
  if (tr >= 0) strip.timebase = (unsigned long)tr - millis();

  JsonObject nl       = root["nl"];
  nightlightActive    = getBoolVal(nl["on"], nightlightActive);
  nightlightDelayMins = nl["dur"]     | nightlightDelayMins;
  nightlightMode      = nl["mode"]    | nightlightMode;
  nightlightTargetBri = nl[F("tbri")] | nightlightTargetBri;

  JsonObject udpn      = root["udpn"];
  sendNotificationsRT  = getBoolVal(udpn[F("send")], sendNotificationsRT);
  syncGroups           = udpn[F("sgrp")] | syncGroups;
  receiveGroups        = udpn[F("rgrp")] | receiveGroups;
  if ((bool)udpn[F("nn")]) callMode = CALL_MODE_NO_NOTIFY; //send no notification just for this request

  unsigned long timein = root["time"] | UINT32_MAX; //backup time source if NTP not synced
  if (timein != UINT32_MAX) {
    setTimeFromAPI(timein);
    if (presetsModifiedTime == 0) presetsModifiedTime = timein;
  }

  if (root[F("psave")].isNull()) doReboot = root[F("rb")] | doReboot;

  // do not allow changing main segment while in realtime mode (may get odd results else)
  if (!realtimeMode) strip.setMainSegmentId(root[F("mainseg")] | strip.getMainSegmentId()); // must be before realtimeLock() if "live"

  realtimeOverride = root[F("lor")] | realtimeOverride;
  if (realtimeOverride > 2) realtimeOverride = REALTIME_OVERRIDE_ALWAYS;
  if (realtimeMode && useMainSegmentOnly) {
    strip.getMainSegment().freeze = !realtimeOverride;
  }

  if (root.containsKey("live")) {
    if (root["live"].as<bool>()) {
      jsonTransitionOnce = true;
      strip.setTransition(0);
      realtimeLock(65000);
    } else {
      exitRealtime();
    }
  }

  int it = 0;
  JsonVariant segVar = root["seg"];
  if (!segVar.isNull()) {
    // we may be called during strip.service() so we must not modify segments while effects are executing
    strip.suspend();
    const unsigned long start = millis();
    while (strip.isServicing() && millis() - start < strip.getFrameTime()) yield(); // wait until frame is over
    #ifdef WLED_DEBUG
    if (millis() - start > 0) DEBUG_PRINTLN(F("JSON: Waited for strip to finish servicing."));
    #endif
    if (segVar.is<JsonObject>()) {
      int id = segVar["id"] | -1;
      //if "seg" is not an array and ID not specified, apply to all selected/checked segments
      if (id < 0) {
        //apply all selected segments
        for (size_t s = 0; s < strip.getSegmentsNum(); s++) {
          Segment &sg = strip.getSegment(s);
          if (sg.isActive() && sg.isSelected()) {
            deserializeSegment(segVar, s, presetId);
          }
        }
      } else {
        deserializeSegment(segVar, id, presetId); //apply only the segment with the specified ID
      }
    } else {
      size_t deleted = 0;
      JsonArray segs = segVar.as<JsonArray>();
      for (JsonObject elem : segs) {
        if (deserializeSegment(elem, it++, presetId) && !elem["stop"].isNull() && elem["stop"]==0) deleted++;
      }
      if (strip.getSegmentsNum() > 3 && deleted >= strip.getSegmentsNum()/2U) strip.purgeSegments(); // batch deleting more than half segments
    }
    strip.resume();
  }

  UsermodManager::readFromJsonState(root);

  loadLedmap = root[F("ledmap")] | loadLedmap;

  byte ps = root[F("psave")];
  if (ps > 0 && ps < 251) savePreset(ps, nullptr, root);

  ps = root[F("pdel")]; //deletion
  if (ps > 0 && ps < 251) deletePreset(ps);

  // HTTP API commands (must be handled before "ps")
  const char* httpwin = root["win"];
  if (httpwin) {
    String apireq = "win"; apireq += '&'; // reduce flash string usage
    apireq += httpwin;
    handleSet(nullptr, apireq, false);    // may set stateChanged
  }

  // Applying preset from JSON API has 2 cases: a) "pd" AKA "preset direct" and b) "ps" AKA "preset select"
  // a) "preset direct" can only be an integer value representing preset ID. "preset direct" assumes JSON API contains the rest of preset content (i.e. from UI call)
  //    "preset direct" JSON can contain "ps" API (i.e. call from UI to cycle presets) in such case stateChanged has to be false (i.e. no "win" or "seg" API)
  // b) "preset select" can be cycling ("1~5~""), random ("r" or "1~5r"), ID, etc. value allowed from JSON API. This type of call assumes no state changing content in API call
  byte presetToRestore = 0;
  if (!root[F("pd")].isNull() && stateChanged) {
    // a) already applied preset content (requires "seg" or "win" but will ignore the rest)
    currentPreset = root[F("pd")] | currentPreset;
    if (root["win"].isNull()) presetCycCurr = currentPreset; // otherwise presetCycCurr was set in handleSet() [set.cpp]
    presetToRestore = currentPreset; // stateUpdated() will clear the preset, so we need to restore it after
    DEBUG_PRINTF_P(PSTR("Preset direct: %d\n"), currentPreset);
  } else if (!root["ps"].isNull()) {
    // we have "ps" call (i.e. from button or external API call) or "pd" that includes "ps" (i.e. from UI call)
    if (root["win"].isNull() && getVal(root["ps"], &presetCycCurr, 1, 250) && presetCycCurr > 0 && presetCycCurr < 251 && presetCycCurr != currentPreset) {
      DEBUG_PRINTF_P(PSTR("Preset select: %d\n"), presetCycCurr);
      // b) preset ID only or preset that does not change state (use embedded cycling limits if they exist in getVal())
      applyPreset(presetCycCurr, callMode); // async load from file system (only preset ID was specified)
      return stateResponse;
    } else presetCycCurr = currentPreset; // restore presetCycCurr
  }

  JsonObject playlist = root[F("playlist")];
  if (!playlist.isNull() && loadPlaylist(playlist, presetId)) {
    //do not notify here, because the first playlist entry will do
    if (root["on"].isNull()) callMode = CALL_MODE_NO_NOTIFY;
    else callMode = CALL_MODE_DIRECT_CHANGE;  // possible bugfix for playlist only containing HTTP API preset FX=~
  }

  if (root.containsKey(F("rmcpal")) && root[F("rmcpal")].as<bool>()) {
    if (strip.customPalettes.size()) {
      char fileName[32];
      sprintf_P(fileName, PSTR("/palette%d.json"), strip.customPalettes.size()-1);
      if (WLED_FS.exists(fileName)) WLED_FS.remove(fileName);
      strip.loadCustomPalettes();
    }
  }

  doAdvancePlaylist = root[F("np")] | doAdvancePlaylist; //advances to next preset in playlist when true
  
  JsonObject wifi = root[F("wifi")];
  if (!wifi.isNull()) {
    bool apMode = getBoolVal(wifi[F("ap")], apActive);
    if (!apActive && apMode) WLED::instance().initAP();  // start AP mode immediately
    else if (apActive && !apMode) { // stop AP mode immediately
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      apActive = false;
    }
    //bool restart = wifi[F("restart")] | false;
    //if (restart) forceReconnect = true;
  }

  stateUpdated(callMode);
  if (presetToRestore) currentPreset = presetToRestore;

  return stateResponse;
}

void serializeSegment(const JsonObject& root, const Segment& seg, byte id, bool forPreset, bool segmentBounds)
{
  root["id"] = id;
  if (segmentBounds) {
    root["start"] = seg.start;
    root["stop"] = seg.stop;
    #ifndef WLED_DISABLE_2D
    if (strip.isMatrix) {
      root[F("startY")] = seg.startY;
      root[F("stopY")]  = seg.stopY;
    }
    #endif
  }
  if (!forPreset) root["len"] = seg.stop - seg.start;
  root["grp"]    = seg.grouping;
  root[F("spc")] = seg.spacing;
  root[F("of")]  = seg.offset;
  root["on"]     = seg.on;
  root["frz"]    = seg.freeze;
  byte segbri    = seg.opacity;
  root["bri"]    = (segbri) ? segbri : 255;
  root["cct"]    = seg.cct;
  root[F("set")] = seg.set;

  if (seg.name != nullptr) root["n"] = reinterpret_cast<const char *>(seg.name); //not good practice, but decreases required JSON buffer
  else if (forPreset) root["n"] = "";

  // to conserve RAM we will serialize the col array manually
  // this will reduce RAM footprint from ~300 bytes to 84 bytes per segment
  char colstr[70]; colstr[0] = '['; colstr[1] = '\0';  //max len 68 (5 chan, all 255)
  const char *format = strip.hasWhiteChannel() ? PSTR("[%u,%u,%u,%u]") : PSTR("[%u,%u,%u]");
  for (size_t i = 0; i < 3; i++)
  {
    byte segcol[4]; byte* c = segcol;
    segcol[0] = R(seg.colors[i]);
    segcol[1] = G(seg.colors[i]);
    segcol[2] = B(seg.colors[i]);
    segcol[3] = W(seg.colors[i]);
    char tmpcol[22];
    sprintf_P(tmpcol, format, (unsigned)c[0], (unsigned)c[1], (unsigned)c[2], (unsigned)c[3]);
    strcat(colstr, i<2 ? strcat(tmpcol, ",") : tmpcol);
  }
  strcat(colstr, "]");
  root["col"] = serialized(colstr);

  root["fx"]  = seg.mode;
  root["sx"]  = seg.speed;
  root["ix"]  = seg.intensity;
  root["pal"] = seg.palette;
  root["c1"]  = seg.custom1;
  root["c2"]  = seg.custom2;
  root["c3"]  = seg.custom3;
  root["sel"] = seg.isSelected();
  root["rev"] = seg.reverse;
  root["mi"]  = seg.mirror;
  #ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    root["rY"] = seg.reverse_y;
    root["mY"] = seg.mirror_y;
    root[F("tp")] = seg.transpose;
  }
  #endif
  root["o1"]  = seg.check1;
  root["o2"]  = seg.check2;
  root["o3"]  = seg.check3;
  root["si"]  = seg.soundSim;
  root["m12"] = seg.map1D2D;
}

void serializeState(JsonObject root, bool forPreset, bool includeBri, bool segmentBounds, bool selectedSegmentsOnly)
{
  if (includeBri) {
    root["on"] = (bri > 0);
    root["bri"] = briLast;
    root[F("transition")] = transitionDelay/100; //in 100ms
#ifndef WLED_DISABLE_MODE_BLEND
    root[F("bs")] = blendingStyle;
#endif
  }

  if (!forPreset) {
    if (errorFlag) {root[F("error")] = errorFlag; errorFlag = ERR_NONE;} //prevent error message to persist on screen

    root["ps"] = (currentPreset > 0) ? currentPreset : -1;
    root[F("pl")] = currentPlaylist;
    root[F("ledmap")] = currentLedmap;

    UsermodManager::addToJsonState(root);

    JsonObject nl = root.createNestedObject("nl");
    nl["on"] = nightlightActive;
    nl["dur"] = nightlightDelayMins;
    nl["mode"] = nightlightMode;
    nl[F("tbri")] = nightlightTargetBri;
    nl[F("rem")] = nightlightActive ? (int)(nightlightDelayMs - (millis() - nightlightStartTime)) / 1000 : -1; // seconds remaining

    JsonObject udpn = root.createNestedObject("udpn");
    udpn[F("send")] = sendNotificationsRT;
    udpn[F("recv")] = receiveGroups != 0;
    udpn[F("sgrp")] = syncGroups;
    udpn[F("rgrp")] = receiveGroups;

    root[F("lor")] = realtimeOverride;
  }

  root[F("mainseg")] = strip.getMainSegmentId();

  JsonArray seg = root.createNestedArray("seg");
  for (size_t s = 0; s < strip.getMaxSegments(); s++) {
    if (s >= strip.getSegmentsNum()) {
      if (forPreset && segmentBounds && !selectedSegmentsOnly) { //disable segments not part of preset
        JsonObject seg0 = seg.createNestedObject();
        seg0["stop"] = 0;
        continue;
      } else
        break;
    }
    Segment &sg = strip.getSegment(s);
    if (forPreset && selectedSegmentsOnly && !sg.isSelected()) continue;
    if (sg.isActive()) {
      JsonObject seg0 = seg.createNestedObject();
      serializeSegment(seg0, sg, s, forPreset, segmentBounds);
    } else if (forPreset && segmentBounds) { //disable segments not part of preset
      JsonObject seg0 = seg.createNestedObject();
      seg0["stop"] = 0;
    }
  }
}

void serializeInfo(JsonObject root)
{
  root[F("ver")] = versionString;
  root[F("vid")] = VERSION;
  root[F("cn")] = F(WLED_CODENAME);
  root[F("release")] = releaseString;

  JsonObject leds = root.createNestedObject(F("leds"));
  leds[F("count")] = strip.getLengthTotal();
  leds[F("pwr")] = BusManager::currentMilliamps();
  leds["fps"] = strip.getFps();
  leds[F("maxpwr")] = BusManager::currentMilliamps()>0 ? BusManager::ablMilliampsMax() : 0;
  leds[F("maxseg")] = strip.getMaxSegments();
  //leds[F("actseg")] = strip.getActiveSegmentsNum();
  //leds[F("seglock")] = false; //might be used in the future to prevent modifications to segment config
  leds[F("bootps")] = bootPreset;

  #ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    JsonObject matrix = leds.createNestedObject(F("matrix"));
    matrix["w"] = Segment::maxWidth;
    matrix["h"] = Segment::maxHeight;
  }
  #endif

  unsigned totalLC = 0;
  JsonArray lcarr = leds.createNestedArray(F("seglc"));
  size_t nSegs = strip.getSegmentsNum();
  for (size_t s = 0; s < nSegs; s++) {
    if (!strip.getSegment(s).isActive()) continue;
    unsigned lc = strip.getSegment(s).getLightCapabilities();
    totalLC |= lc;
    lcarr.add(lc);
  }

  leds["lc"] = totalLC;

  leds[F("rgbw")] = strip.hasRGBWBus(); // deprecated, use info.leds.lc
  leds[F("wv")]   = totalLC & 0x02;     // deprecated, true if white slider should be displayed for any segment
  leds["cct"]     = totalLC & 0x04;     // deprecated, use info.leds.lc

  #ifdef WLED_DEBUG
  JsonArray i2c = root.createNestedArray(F("i2c"));
  i2c.add(i2c_sda);
  i2c.add(i2c_scl);
  JsonArray spi = root.createNestedArray(F("spi"));
  spi.add(spi_mosi);
  spi.add(spi_sclk);
  spi.add(spi_miso);
  #endif

  root[F("str")] = false; //syncToggleReceive;

  root[F("name")] = serverDescription;
  root[F("udpport")] = udpPort;
  root[F("simplifiedui")] = simplifiedUI;
  root["live"] = (bool)realtimeMode;
  root[F("liveseg")] = useMainSegmentOnly ? strip.getMainSegmentId() : -1;  // if using main segment only for live

  switch (realtimeMode) {
    case REALTIME_MODE_INACTIVE: root["lm"] = ""; break;
    case REALTIME_MODE_GENERIC:  root["lm"] = ""; break;
    case REALTIME_MODE_UDP:      root["lm"] = F("UDP"); break;
    case REALTIME_MODE_HYPERION: root["lm"] = F("Hyperion"); break;
    case REALTIME_MODE_E131:     root["lm"] = F("E1.31"); break;
    case REALTIME_MODE_ADALIGHT: root["lm"] = F("USB Adalight/TPM2"); break;
    case REALTIME_MODE_ARTNET:   root["lm"] = F("Art-Net"); break;
    case REALTIME_MODE_TPM2NET:  root["lm"] = F("tpm2.net"); break;
    case REALTIME_MODE_DDP:      root["lm"] = F("DDP"); break;
  }

  root[F("lip")] = realtimeIP[0] == 0 ? "" : realtimeIP.toString();

  #ifdef WLED_ENABLE_WEBSOCKETS
  root[F("ws")] = ws.count();
  #else
  root[F("ws")] = -1;
  #endif

  root[F("fxcount")] = strip.getModeCount();
  root[F("palcount")] = strip.getPaletteCount();
  root[F("cpalcount")] = strip.customPalettes.size(); //number of custom palettes

  JsonArray ledmaps = root.createNestedArray(F("maps"));
  for (size_t i=0; i<WLED_MAX_LEDMAPS; i++) {
    if ((ledMaps>>i) & 0x00000001U) {
      JsonObject ledmaps0 = ledmaps.createNestedObject();
      ledmaps0["id"] = i;
      #ifndef ESP8266
      if (i && ledmapNames[i-1]) ledmaps0["n"] = ledmapNames[i-1];
      #endif
    }
  }

  JsonObject wifi_info = root.createNestedObject(F("wifi"));
  wifi_info[F("bssid")] = WiFi.BSSIDstr();
  int qrssi = WiFi.RSSI();
  wifi_info[F("rssi")] = qrssi;
  wifi_info[F("signal")] = getSignalQuality(qrssi);
  wifi_info[F("channel")] = WiFi.channel();
  wifi_info[F("ap")] = apActive;

  JsonObject fs_info = root.createNestedObject("fs");
  fs_info["u"] = fsBytesUsed / 1000;
  fs_info["t"] = fsBytesTotal / 1000;
  fs_info[F("pmt")] = presetsModifiedTime;

  root[F("ndc")] = nodeListEnabled ? (int)Nodes.size() : -1;

#ifdef ARDUINO_ARCH_ESP32
  #ifdef WLED_DEBUG
    wifi_info[F("txPower")] = (int) WiFi.getTxPower();
    wifi_info[F("sleep")] = (bool) WiFi.getSleep();
  #endif
  #if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)
    root[F("arch")] = "esp32";
  #else
    root[F("arch")] = ESP.getChipModel();
  #endif
  root[F("core")] = ESP.getSdkVersion();
  root[F("clock")] = ESP.getCpuFreqMHz();
  root[F("flash")] = (ESP.getFlashChipSize()/1024)/1024;
  #ifdef WLED_DEBUG
  root[F("maxalloc")] = ESP.getMaxAllocHeap();
  root[F("resetReason0")] = (int)rtc_get_reset_reason(0);
  root[F("resetReason1")] = (int)rtc_get_reset_reason(1);
  #endif
  root[F("lwip")] = 0; //deprecated
#else
  root[F("arch")] = "esp8266";
  root[F("core")] = ESP.getCoreVersion();
  root[F("clock")] = ESP.getCpuFreqMHz();
  root[F("flash")] = (ESP.getFlashChipSize()/1024)/1024;
  #ifdef WLED_DEBUG
  root[F("maxalloc")] = ESP.getMaxFreeBlockSize();
  root[F("resetReason")] = (int)ESP.getResetInfoPtr()->reason;
  #endif
  root[F("lwip")] = LWIP_VERSION_MAJOR;
#endif

  root[F("freeheap")] = ESP.getFreeHeap();
  #if defined(ARDUINO_ARCH_ESP32)
  if (psramFound()) root[F("psram")] = ESP.getFreePsram();
  #endif
  root[F("uptime")] = millis()/1000 + rolloverMillis*4294967;

  char time[32];
  getTimeString(time);
  root[F("time")] = time;

  UsermodManager::addToJsonInfo(root);

  uint16_t os = 0;
  #ifdef WLED_DEBUG
  os  = 0x80;
    #ifdef WLED_DEBUG_HOST
    os |= 0x0100;
    if (!netDebugEnabled) os &= ~0x0080;
    #endif
  #endif
  #ifndef WLED_DISABLE_ALEXA
  os += 0x40;
  #endif

  //os += 0x20; // indicated now removed Blynk support, may be reused to indicate another build-time option

  #ifdef USERMOD_CRONIXIE
  os += 0x10;
  #endif
  #ifndef WLED_DISABLE_FILESYSTEM
  os += 0x08;
  #endif
  #ifndef WLED_DISABLE_HUESYNC
  os += 0x04;
  #endif
  #ifdef WLED_ENABLE_ADALIGHT
  os += 0x02;
  #endif
  #ifndef WLED_DISABLE_OTA
  os += 0x01;
  #endif
  root[F("opt")] = os;

  root[F("brand")] = F(WLED_BRAND);
  root[F("product")] = F(WLED_PRODUCT_NAME);
  root["mac"] = escapedMac;
  char s[16] = "";
  if (Network.isConnected())
  {
    IPAddress localIP = Network.localIP();
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
  }
  root["ip"] = s;
}

void setPaletteColors(JsonArray json, CRGBPalette16 palette)
{
    for (int i = 0; i < 16; i++) {
      JsonArray colors =  json.createNestedArray();
      CRGB color = palette[i];
      colors.add(i<<4);
      colors.add(color.red);
      colors.add(color.green);
      colors.add(color.blue);
    }
}

void setPaletteColors(JsonArray json, byte* tcp)
{
    TRGBGradientPaletteEntryUnion* ent = (TRGBGradientPaletteEntryUnion*)(tcp);
    TRGBGradientPaletteEntryUnion u;

    // Count entries
    unsigned count = 0;
    do {
        u = *(ent + count);
        count++;
    } while ( u.index != 255);

    u = *ent;
    int indexstart = 0;
    while( indexstart < 255) {
      indexstart = u.index;

      JsonArray colors =  json.createNestedArray();
      colors.add(u.index);
      colors.add(u.r);
      colors.add(u.g);
      colors.add(u.b);

      ent++;
      u = *ent;
    }
}

void serializePalettes(JsonObject root, int page)
{
  byte tcp[72];
  #ifdef ESP8266
  int itemPerPage = 5;
  #else
  int itemPerPage = 8;
  #endif

  int customPalettes = strip.customPalettes.size();
  int palettesCount = strip.getPaletteCount() - customPalettes;

  int maxPage = (palettesCount + customPalettes -1) / itemPerPage;
  if (page > maxPage) page = maxPage;

  int start = itemPerPage * page;
  int end = start + itemPerPage;
  if (end > palettesCount + customPalettes) end = palettesCount + customPalettes;

  root[F("m")] = maxPage; // inform caller how many pages there are
  JsonObject palettes  = root.createNestedObject("p");

  for (int i = start; i < end; i++) {
    JsonArray curPalette = palettes.createNestedArray(String(i>=palettesCount ? 255 - i + palettesCount : i));
    switch (i) {
      case 0: //default palette
        setPaletteColors(curPalette, PartyColors_p);
        break;
      case 1: //random
           for (int j = 0; j < 4; j++) curPalette.add("r");
        break;
      case 2: //primary color only
        curPalette.add("c1");
        break;
      case 3: //primary + secondary
        curPalette.add("c1");
        curPalette.add("c1");
        curPalette.add("c2");
        curPalette.add("c2");
        break;
      case 4: //primary + secondary + tertiary
        curPalette.add("c3");
        curPalette.add("c2");
        curPalette.add("c1");
        break;
      case 5: //primary + secondary (+tertiary if not off), more distinct
        for (int j = 0; j < 5; j++) curPalette.add("c1");
        for (int j = 0; j < 5; j++) curPalette.add("c2");
        for (int j = 0; j < 5; j++) curPalette.add("c3");
        curPalette.add("c1");
        break;
      default:
        if (i >= palettesCount)
          setPaletteColors(curPalette, strip.customPalettes[i - palettesCount]);
        else if (i < 13) // palette 6 - 12, fastled palettes
          setPaletteColors(curPalette, *fastledPalettes[i-6]);
        else {
          memcpy_P(tcp, (byte*)pgm_read_dword(&(gGradientPalettes[i - 13])), 72);
          setPaletteColors(curPalette, tcp);
        }
        break;
    }
  }
}

void serializeNetworks(JsonObject root)
{
  JsonArray networks = root.createNestedArray(F("networks"));
  int16_t status = WiFi.scanComplete();

  switch (status) {
    case WIFI_SCAN_FAILED:
      WiFi.scanNetworks(true);
      return;
    case WIFI_SCAN_RUNNING:
      return;
  }

  for (int i = 0; i < status; i++) {
    JsonObject node = networks.createNestedObject();
    node[F("ssid")]    = WiFi.SSID(i);
    node[F("rssi")]    = WiFi.RSSI(i);
    node[F("bssid")]   = WiFi.BSSIDstr(i);
    node[F("channel")] = WiFi.channel(i);
    node[F("enc")]     = WiFi.encryptionType(i);
  }

  WiFi.scanDelete();

  if (WiFi.scanComplete() == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
  }
}

void serializeNodes(JsonObject root)
{
  JsonArray nodes = root.createNestedArray("nodes");

  for (NodesMap::iterator it = Nodes.begin(); it != Nodes.end(); ++it)
  {
    if (it->second.ip[0] != 0)
    {
      JsonObject node = nodes.createNestedObject();
      node[F("name")] = it->second.nodeName;
      node["type"]    = it->second.nodeType;
      node["ip"]      = it->second.ip.toString();
      node[F("age")]  = it->second.age;
      node[F("vid")]  = it->second.build;
    }
  }
}

// deserializes mode data string into JsonArray
void serializeModeData(JsonArray fxdata)
{
  char lineBuffer[256];
  for (size_t i = 0; i < strip.getModeCount(); i++) {
    strncpy_P(lineBuffer, strip.getModeData(i), sizeof(lineBuffer)/sizeof(char)-1);
    lineBuffer[sizeof(lineBuffer)/sizeof(char)-1] = '\0'; // terminate string
    if (lineBuffer[0] != 0) {
      char* dataPtr = strchr(lineBuffer,'@');
      if (dataPtr) fxdata.add(dataPtr+1);
      else         fxdata.add("");
    }
  }
}

// deserializes mode names string into JsonArray
// also removes effect data extensions (@...) from deserialised names
void serializeModeNames(JsonArray arr)
{
  char lineBuffer[256];
  for (size_t i = 0; i < strip.getModeCount(); i++) {
    strncpy_P(lineBuffer, strip.getModeData(i), sizeof(lineBuffer)/sizeof(char)-1);
    lineBuffer[sizeof(lineBuffer)/sizeof(char)-1] = '\0'; // terminate string
    if (lineBuffer[0] != 0) {
      char* dataPtr = strchr(lineBuffer,'@');
      if (dataPtr) *dataPtr = 0; // terminate mode data after name
      arr.add(lineBuffer);
    }
  }
}

// Global buffer locking response helper class (to make sure lock is released when AsyncJsonResponse is destroyed)
class LockedJsonResponse: public AsyncJsonResponse {
  bool _holding_lock;
  public:
  // WARNING: constructor assumes requestJSONBufferLock() was successfully acquired externally/prior to constructing the instance
  // Not a good practice with C++. Unfortunately AsyncJsonResponse only has 2 constructors - for dynamic buffer or existing buffer,
  // with existing buffer it clears its content during construction
  // if the lock was not acquired (using JSONBufferGuard class) previous implementation still cleared existing buffer
  inline LockedJsonResponse(JsonDocument* doc, bool isArray) : AsyncJsonResponse(doc, isArray), _holding_lock(true) {};

  virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) { 
    size_t result = AsyncJsonResponse::_fillBuffer(buf, maxLen);
    // Release lock as soon as we're done filling content
    if (((result + _sentLength) >= (_contentLength)) && _holding_lock) {
      releaseJSONBufferLock();
      _holding_lock = false;
    }
    return result;
  }

  // destructor will remove JSON buffer lock when response is destroyed in AsyncWebServer
  virtual ~LockedJsonResponse() { if (_holding_lock) releaseJSONBufferLock(); };
};

void serveJson(AsyncWebServerRequest* request)
{
  enum class json_target {
    all, state, info, state_info, nodes, effects, palettes, fxdata, networks, config
  };
  json_target subJson = json_target::all;

  const String& url = request->url();
  if      (url.indexOf("state")    > 0) subJson = json_target::state;
  else if (url.indexOf("info")     > 0) subJson = json_target::info;
  else if (url.indexOf("si")       > 0) subJson = json_target::state_info;
  else if (url.indexOf(F("nodes")) > 0) subJson = json_target::nodes;
  else if (url.indexOf(F("eff"))   > 0) subJson = json_target::effects;
  else if (url.indexOf(F("palx"))  > 0) subJson = json_target::palettes;
  else if (url.indexOf(F("fxda"))  > 0) subJson = json_target::fxdata;
  else if (url.indexOf(F("net"))   > 0) subJson = json_target::networks;
  else if (url.indexOf(F("cfg"))   > 0) subJson = json_target::config;
  #ifdef WLED_ENABLE_JSONLIVE
  else if (url.indexOf("live")     > 0) {
    serveLiveLeds(request);
    return;
  }
  #endif
  else if (url.indexOf("pal") > 0) {
    request->send_P(200, FPSTR(CONTENT_TYPE_JSON), JSON_palette_names);
    return;
  }
  else if (url.length() > 6) { //not just /json
    serveJsonError(request, 501, ERR_NOT_IMPL);
    return;
  }

  if (!requestJSONBufferLock(17)) {
    request->deferResponse();    
    return;
  }
  // releaseJSONBufferLock() will be called when "response" is destroyed (from AsyncWebServer)
  // make sure you delete "response" if no "request->send(response);" is made
  LockedJsonResponse *response = new LockedJsonResponse(pDoc, subJson==json_target::fxdata || subJson==json_target::effects); // will clear and convert JsonDocument into JsonArray if necessary

  JsonVariant lDoc = response->getRoot();

  switch (subJson)
  {
    case json_target::state:
      serializeState(lDoc); break;
    case json_target::info:
      serializeInfo(lDoc); break;
    case json_target::nodes:
      serializeNodes(lDoc); break;
    case json_target::palettes:
      serializePalettes(lDoc, request->hasParam(F("page")) ? request->getParam(F("page"))->value().toInt() : 0); break;
    case json_target::effects:
      serializeModeNames(lDoc); break;
    case json_target::fxdata:
      serializeModeData(lDoc); break;
    case json_target::networks:
      serializeNetworks(lDoc); break;
    case json_target::config:
      serializeConfig(lDoc); break;
    case json_target::state_info:
    case json_target::all:
      JsonObject state = lDoc.createNestedObject("state");
      serializeState(state);
      JsonObject info = lDoc.createNestedObject("info");
      serializeInfo(info);
      if (subJson == json_target::all)
      {
        JsonArray effects = lDoc.createNestedArray(F("effects"));
        serializeModeNames(effects); // remove WLED-SR extensions from effect names
        lDoc[F("palettes")] = serialized((const __FlashStringHelper*)JSON_palette_names);
      }
      //lDoc["m"] = lDoc.memoryUsage(); // JSON buffer usage, for remote debugging
  }

  DEBUG_PRINTF_P(PSTR("JSON buffer size: %u for request: %d\n"), lDoc.memoryUsage(), subJson);

  [[maybe_unused]] size_t len = response->setLength();
  DEBUG_PRINTF_P(PSTR("JSON content length: %u\n"), len);

  request->send(response);
}

#ifdef WLED_ENABLE_JSONLIVE
#define MAX_LIVE_LEDS 256

bool serveLiveLeds(AsyncWebServerRequest* request, uint32_t wsClient)
{
  #ifdef WLED_ENABLE_WEBSOCKETS
  AsyncWebSocketClient * wsc = nullptr;
  if (!request) { //not HTTP, use Websockets
    wsc = ws.client(wsClient);
    if (!wsc || wsc->queueLength() > 0) return false; //only send if queue free
  }
  #endif

  unsigned used = strip.getLengthTotal();
  unsigned n = (used -1) /MAX_LIVE_LEDS +1; //only serve every n'th LED if count over MAX_LIVE_LEDS
#ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    // ignore anything behid matrix (i.e. extra strip)
    used = Segment::maxWidth*Segment::maxHeight; // always the size of matrix (more or less than strip.getLengthTotal())
    n = 1;
    if (used > MAX_LIVE_LEDS) n = 2;
    if (used > MAX_LIVE_LEDS*4) n = 4;
  }
#endif

  DynamicBuffer buffer(9 + (9*(1+(used/n))) + 7 + 5 + 6 + 5 + 6 + 5 + 2);  
  char* buf = buffer.data();      // assign buffer for oappnd() functions
  strncpy_P(buffer.data(), PSTR("{\"leds\":["), buffer.size());
  buf += 9; // sizeof(PSTR()) from last line

  for (size_t i = 0; i < used; i += n)
  {
#ifndef WLED_DISABLE_2D
    if (strip.isMatrix && n>1 && (i/Segment::maxWidth)%n) i += Segment::maxWidth * (n-1);
#endif
    uint32_t c = strip.getPixelColor(i);
    uint8_t r = R(c);
    uint8_t g = G(c);
    uint8_t b = B(c);
    uint8_t w = W(c);
    r = scale8(qadd8(w, r), strip.getBrightness()); //R, add white channel to RGB channels as a simple RGBW -> RGB map
    g = scale8(qadd8(w, g), strip.getBrightness()); //G
    b = scale8(qadd8(w, b), strip.getBrightness()); //B
    buf += sprintf_P(buf, PSTR("\"%06X\","), RGBW32(r,g,b,0));
  }
  buf--;  // remove last comma
  buf += sprintf_P(buf, PSTR("],\"n\":%d"), n);
#ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    buf += sprintf_P(buf, PSTR(",\"w\":%d"), Segment::maxWidth/n);
    buf += sprintf_P(buf, PSTR(",\"h\":%d"), Segment::maxHeight/n);
  }
#endif
  (*buf++) = '}';
  (*buf++) = 0;
  
  if (request) {
    request->send(200, FPSTR(CONTENT_TYPE_JSON), toString(std::move(buffer)));
  }
  #ifdef WLED_ENABLE_WEBSOCKETS
  else {
    wsc->text(toString(std::move(buffer)));
  }
  #endif  
  return true;
}
#endif
