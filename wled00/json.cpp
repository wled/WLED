#include "wled.h"

#include "palettes.h"

/*
 * JSON API (De)serialization
 */

bool getVal(JsonVariant elem, byte* val, byte vmin=0, byte vmax=255) {
  if (elem.is<int>()) {
    if (elem < 0) return false; //ignore e.g. {"ps":-1}
    *val = elem;
    return true;
  } else if (elem.is<const char*>()) {
    const char* str = elem;
    size_t len = strnlen(str, 12);
    if (len == 0 || len > 10) return false;
    parseNumber(str, val, vmin, vmax);
    return true;
  }
  return false; //key does not exist
}

void deserializeSegment(JsonObject elem, byte it, byte presetId)
{
  byte id = elem["id"] | it;
  if (id >= strip.getMaxSegments()) return;

  WS2812FX::Segment& seg = strip.getSegment(id);
  WS2812FX::Segment prev = seg; //make a backup so we can tell if something changed

  uint16_t start = elem["start"] | seg.start;
  int stop = elem["stop"] | -1;
  if (stop < 0) {
    uint16_t len = elem["len"];
    stop = (len > 0) ? start + len : seg.stop;
  }

  //repeat, multiplies segment until all LEDs are used, or max segments reached
  bool repeat = elem["rpt"] | false;
  if (repeat && stop>0) {
    elem.remove("id");  // remove for recursive call
    elem.remove("rpt"); // remove for recursive call
    elem.remove("n");   // remove for recursive call
    uint16_t len = stop - start;
    for (byte i=id+1; i<strip.getMaxSegments(); i++) {
      start = start + len;
      if (start >= strip.getLengthTotal()) break;
      elem["start"] = start;
      elem["stop"]  = start + len;
      elem["rev"]   = !elem["rev"]; // alternate reverse on even/odd segments
      deserializeSegment(elem, i, presetId); // recursive call with new id
    }
    return;
  }

  if (elem["n"]) {
    // name field exists
    if (seg.name) { //clear old name
      delete[] seg.name;
      seg.name = nullptr;
    }

    const char * name = elem["n"].as<const char*>();
    size_t len = 0;
    if (name != nullptr) len = strlen(name);
    if (len > 0 && len < 33) {
      seg.name = new char[len+1];
      if (seg.name) strlcpy(seg.name, name, 33);
    } else {
      // but is empty (already deleted above)
      elem.remove("n");
    }
  } else if (start != seg.start || stop != seg.stop) {
    // clearing or setting segment without name field
    if (seg.name) {
      delete[] seg.name;
      seg.name = nullptr;
    }
  }

  uint16_t grp = elem["grp"] | seg.grouping;
  uint16_t spc = elem[F("spc")] | seg.spacing;
  uint16_t of = seg.offset;

  uint16_t len = 1;
  if (stop > start) len = stop - start;
  int offset = elem[F("of")] | INT32_MAX;
  if (offset != INT32_MAX) {
    int offsetAbs = abs(offset);
    if (offsetAbs > len - 1) offsetAbs %= len;
    if (offset < 0) offsetAbs = len - offsetAbs;
    of = offsetAbs;
  }
  if (stop > start && of > len -1) of = len -1;
  strip.setSegment(id, start, stop, grp, spc, of);

  byte segbri = seg.opacity;
  if (getVal(elem["bri"], &segbri)) {
    if (segbri > 0) seg.setOpacity(segbri, id);
    seg.setOption(SEG_OPTION_ON, segbri, id);
  }

  bool on = elem["on"] | seg.getOption(SEG_OPTION_ON);
  if (elem["on"].is<const char*>() && elem["on"].as<const char*>()[0] == 't') on = !on;
  seg.setOption(SEG_OPTION_ON, on, id);
  bool frz = elem["frz"] | seg.getOption(SEG_OPTION_FREEZE);
  if (elem["frz"].is<const char*>() && elem["frz"].as<const char*>()[0] == 't') frz = !seg.getOption(SEG_OPTION_FREEZE);
  seg.setOption(SEG_OPTION_FREEZE, frz, id);

  seg.setCCT(elem["cct"] | seg.cct, id);

  JsonArray colarr = elem["col"];
  if (!colarr.isNull())
  {
    for (uint8_t i = 0; i < 3; i++)
    {
      int rgbw[] = {0,0,0,0};
      bool colValid = false;
      JsonArray colX = colarr[i];
      if (colX.isNull()) {
        byte brgbw[] = {0,0,0,0};
        const char* hexCol = colarr[i];
        if (hexCol == nullptr) { //Kelvin color temperature (or invalid), e.g 2400
          int kelvin = colarr[i] | -1;
          if (kelvin <  0) continue;
          if (kelvin == 0) seg.setColor(i, 0, id);
          if (kelvin >  0) colorKtoRGB(kelvin, brgbw);
          colValid = true;
        } else { //HEX string, e.g. "FFAA00"
          colValid = colorFromHexString(brgbw, hexCol);
        }
        for (uint8_t c = 0; c < 4; c++) rgbw[c] = brgbw[c];
      } else { //Array of ints (RGB or RGBW color), e.g. [255,160,0]
        byte sz = colX.size();
        if (sz == 0) continue; //do nothing on empty array

        copyArray(colX, rgbw, 4);
        colValid = true;
      }

      if (!colValid) continue;

      seg.setColor(i, RGBW32(rgbw[0],rgbw[1],rgbw[2],rgbw[3]), id);
      if (seg.mode == FX_MODE_STATIC) strip.trigger(); //instant refresh
    }
  }

  // lx parser
  #ifdef WLED_ENABLE_LOXONE
  int lx = elem[F("lx")] | -1;
  if (lx > 0) {
    parseLxJson(lx, id, false);
  }
  int ly = elem[F("ly")] | -1;
  if (ly > 0) {
    parseLxJson(ly, id, true);
  }
  #endif

  seg.setOption(SEG_OPTION_SELECTED, elem[F("sel")] | seg.getOption(SEG_OPTION_SELECTED));
  seg.setOption(SEG_OPTION_REVERSED, elem["rev"]    | seg.getOption(SEG_OPTION_REVERSED));
  seg.setOption(SEG_OPTION_MIRROR  , elem[F("mi")]  | seg.getOption(SEG_OPTION_MIRROR  ));

  byte fx = seg.mode;
  if (getVal(elem["fx"], &fx, 1, strip.getModeCount())) { //load effect ('r' random, '~' inc/dec, 1-255 exact value)
    if (!presetId && currentPlaylist>=0) unloadPlaylist();
    strip.setMode(id, fx);
  }

  //getVal also supports inc/decrementing and random
  getVal(elem[F("sx")], &seg.speed, 0, 255);
  getVal(elem[F("ix")], &seg.intensity, 0, 255);
  getVal(elem["pal"], &seg.palette, 1, strip.getPaletteCount());

  JsonArray iarr = elem[F("i")]; //set individual LEDs
  if (!iarr.isNull()) {
    uint8_t oldSegId = strip.setPixelSegment(id);

    // set brightness immediately and disable transition
    transitionDelayTemp = 0;
    jsonTransitionOnce = true;
    strip.setBrightness(scaledBri(bri), true);

    // freeze and init to black
    if (!seg.getOption(SEG_OPTION_FREEZE)) {
      seg.setOption(SEG_OPTION_FREEZE, true);
      strip.fill(0);
    }

    uint16_t start = 0, stop = 0;
    byte set = 0; //0 nothing set, 1 start set, 2 range set

    for (uint16_t i = 0; i < iarr.size(); i++) {
      if(iarr[i].is<JsonInteger>()) {
        if (!set) {
          start = iarr[i];
          set = 1;
        } else {
          stop = iarr[i];
          set = 2;
        }
      } else { //color
        int rgbw[] = {0,0,0,0};
        JsonArray icol = iarr[i];
        if (!icol.isNull()) { //array, e.g. [255,0,0]
          byte sz = icol.size();
          if (sz > 0 && sz < 5) copyArray(icol, rgbw);
        } else { //hex string, e.g. "FF0000"
          byte brgbw[] = {0,0,0,0};
          const char* hexCol = iarr[i];
          if (colorFromHexString(brgbw, hexCol)) {
            for (uint8_t c = 0; c < 4; c++) rgbw[c] = brgbw[c];
          }
        }

        if (set < 2) stop = start + 1;
        for (uint16_t i = start; i < stop; i++) {
          if (strip.gammaCorrectCol) {
            strip.setPixelColor(i, strip.gamma8(rgbw[0]), strip.gamma8(rgbw[1]), strip.gamma8(rgbw[2]), strip.gamma8(rgbw[3]));
          } else {
            strip.setPixelColor(i, rgbw[0], rgbw[1], rgbw[2], rgbw[3]);
          }
        }
        if (!set) start++;
        set = 0;
      }
    }
    strip.setPixelSegment(oldSegId);
    strip.trigger();
//  } else if (!elem["frz"] && iarr.isNull()) { //return to regular effect
//    seg.setOption(SEG_OPTION_FREEZE, false);
  }
  // send UDP if not in preset and something changed that is not just selection
  //if (!presetId && (seg.differs(prev) & 0x7F)) stateChanged = true;
  // send UDP if something changed that is not just selection
  if (seg.differs(prev) & 0x7F) stateChanged = true;
  return;
}

// deserializes WLED state (fileDoc points to doc object if called from web server)
bool deserializeState(JsonObject root, byte callMode, byte presetId)
{
  bool stateResponse = root[F("v")] | false;

  bool onBefore = bri;
  getVal(root["bri"], &bri);

  bool on = root["on"] | (bri > 0);
  if (!on != !bri) toggleOnOff();

  if (root["on"].is<const char*>() && root["on"].as<const char*>()[0] == 't') toggleOnOff();

  if (bri && !onBefore) { // unfreeze all segments when turning on
    for (uint8_t s=0; s < strip.getMaxSegments(); s++) {
      strip.getSegment(s).setOption(SEG_OPTION_FREEZE, false, s);
    }
    if (realtimeMode && !realtimeOverride && useMainSegmentOnly) { // keep live segment frozen if live
      strip.getMainSegment().setOption(SEG_OPTION_FREEZE, true, strip.getMainSegmentId());
    }
  }

  int tr = -1;
  if (!presetId || currentPlaylist < 0) { //do not apply transition time from preset if playlist active, as it would override playlist transition times
    tr = root[F("transition")] | -1;
    if (tr >= 0)
    {
      transitionDelay = tr;
      transitionDelay *= 100;
      transitionDelayTemp = transitionDelay;
    }
  }

  tr = root[F("tt")] | -1;
  if (tr >= 0)
  {
    transitionDelayTemp = tr;
    transitionDelayTemp *= 100;
    jsonTransitionOnce = true;
  }
  strip.setTransition(transitionDelayTemp); // required here for color transitions to have correct duration

  tr = root[F("tb")] | -1;
  if (tr >= 0) strip.timebase = ((uint32_t)tr) - millis();

  JsonObject nl       = root["nl"];
  nightlightActive    = nl["on"]      | nightlightActive;
  nightlightDelayMins = nl["dur"]     | nightlightDelayMins;
  nightlightMode      = nl["mode"]    | nightlightMode;
  nightlightTargetBri = nl[F("tbri")] | nightlightTargetBri;

  JsonObject udpn      = root["udpn"];
  notifyDirect         = udpn["send"] | notifyDirect;
  receiveNotifications = udpn["recv"] | receiveNotifications;
  if ((bool)udpn[F("nn")]) callMode = CALL_MODE_NO_NOTIFY; //send no notification just for this request

  unsigned long timein = root[F("time")] | UINT32_MAX; //backup time source if NTP not synced
  if (timein != UINT32_MAX) {
    setTimeFromAPI(timein);
    if (presetsModifiedTime == 0) presetsModifiedTime = timein;
  }

  doReboot = root[F("rb")] | doReboot;

  strip.setMainSegmentId(root[F("mainseg")] | strip.getMainSegmentId()); // must be before realtimeLock() if "live"

  realtimeOverride = root[F("lor")] | realtimeOverride;
  if (realtimeOverride > 2) realtimeOverride = REALTIME_OVERRIDE_ALWAYS;
  if (realtimeMode && useMainSegmentOnly) {
    strip.getMainSegment().setOption(SEG_OPTION_FREEZE, !realtimeOverride, strip.getMainSegmentId());
  }

  if (root.containsKey("live")) {
    if (root["live"].as<bool>()) {
      transitionDelayTemp = 0;
      jsonTransitionOnce = true;
      realtimeLock(65000);
    } else {
      exitRealtime();
    }
  }

  int it = 0;
  JsonVariant segVar = root["seg"];
  if (segVar.is<JsonObject>())
  {
    int id = segVar["id"] | -1;
    //if "seg" is not an array and ID not specified, apply to all selected/checked segments
    if (id < 0) {
      //apply all selected segments
      bool didSet = false;
      for (byte s = 0; s < strip.getMaxSegments(); s++) {
        WS2812FX::Segment &sg = strip.getSegment(s);
        if (sg.isActive()) {
          if (sg.isSelected()) {
            deserializeSegment(segVar, s, presetId);
            didSet = true;
          }
        }
      }
      //if none selected, apply to the main segment
      if (!didSet) deserializeSegment(segVar, strip.getMainSegmentId(), presetId);
    } else {
      deserializeSegment(segVar, id, presetId); //apply only the segment with the specified ID
    }
  } else {
    JsonArray segs = segVar.as<JsonArray>();
    for (JsonObject elem : segs)
    {
      deserializeSegment(elem, it, presetId);
      it++;
    }
  }

  usermods.readFromJsonState(root);

  loadLedmap = root[F("ledmap")] | loadLedmap;

  byte ps = root[F("psave")];
  if (ps > 0) {
    savePreset(ps, nullptr, root);
  } else {
    ps = root[F("pdel")]; //deletion
    if (ps > 0) {
      deletePreset(ps);
    }

    ps = presetCycCurr;
    if (getVal(root["ps"], &ps, presetCycMin, presetCycMax)) { //load preset (clears state request!)
      if (!presetId) unloadPlaylist(); //stop playlist if preset changed manually
      if (ps >= presetCycMin && ps <= presetCycMax) presetCycCurr = ps;
      applyPreset(ps, callMode);
      return stateResponse;
    }

    //HTTP API commands
    const char* httpwin = root["win"];
    if (httpwin) {
      String apireq = "win&";
      apireq += httpwin;
      handleSet(nullptr, apireq, false);
    }
  }

  JsonObject playlist = root[F("playlist")];
  if (!playlist.isNull() && loadPlaylist(playlist, presetId)) {
    //do not notify here, because the first playlist entry will do
    if (root["on"].isNull()) callMode = CALL_MODE_NO_NOTIFY;
    else callMode = CALL_MODE_DIRECT_CHANGE;  // possible bugfix for playlist only containing HTTP API preset FX=~
  } else {
    interfaceUpdateCallMode = CALL_MODE_WS_SEND;
  }

  stateUpdated(callMode);

  return stateResponse;
}

void serializeSegment(JsonObject& root, WS2812FX::Segment& seg, byte id, bool forPreset, bool segmentBounds)
{
  root["id"] = id;
  if (segmentBounds) {
    root["start"] = seg.start;
    root["stop"] = seg.stop;
  }
  if (!forPreset) root["len"] = seg.stop - seg.start;
  root["grp"] = seg.grouping;
  root[F("spc")] = seg.spacing;
  root[F("of")] = seg.offset;
  root["on"] = seg.getOption(SEG_OPTION_ON);
  root["frz"] = seg.getOption(SEG_OPTION_FREEZE);
  byte segbri = seg.opacity;
  root["bri"] = (segbri) ? segbri : 255;
  root["cct"] = seg.cct;

  if (segmentBounds && seg.name != nullptr) root["n"] = reinterpret_cast<const char *>(seg.name); //not good practice, but decreases required JSON buffer

  // to conserve RAM we will serialize the col array manually
  // this will reduce RAM footprint from ~300 bytes to 84 bytes per segment
  char colstr[70]; colstr[0] = '['; colstr[1] = '\0';  //max len 68 (5 chan, all 255)
  const char *format = strip.hasWhiteChannel() ? PSTR("[%u,%u,%u,%u]") : PSTR("[%u,%u,%u]");
  for (uint8_t i = 0; i < 3; i++)
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

  root["fx"]     = seg.mode;
  root[F("sx")]  = seg.speed;
  root[F("ix")]  = seg.intensity;
  root["pal"]    = seg.palette;
  root[F("sel")] = seg.isSelected();
  root["rev"]    = seg.getOption(SEG_OPTION_REVERSED);
  root[F("mi")]  = seg.getOption(SEG_OPTION_MIRROR);
}

void serializeState(JsonObject root, bool forPreset, bool includeBri, bool segmentBounds)
{
  if (includeBri) {
    root["on"] = (bri > 0);
    root["bri"] = briLast;
    root[F("transition")] = transitionDelay/100; //in 100ms
  }

  if (!forPreset) {
    if (errorFlag) {root[F("error")] = errorFlag; errorFlag = ERR_NONE;} //prevent error message to persist on screen

    root["ps"] = (currentPreset > 0) ? currentPreset : -1;
    root[F("pl")] = currentPlaylist;

    usermods.addToJsonState(root);

    JsonObject nl = root.createNestedObject("nl");
    nl["on"] = nightlightActive;
    nl["dur"] = nightlightDelayMins;
    nl["mode"] = nightlightMode;
    nl[F("tbri")] = nightlightTargetBri;
    if (nightlightActive) {
      nl[F("rem")] = (nightlightDelayMs - (millis() - nightlightStartTime)) / 1000; // seconds remaining
    } else {
      nl[F("rem")] = -1;
    }

    JsonObject udpn = root.createNestedObject("udpn");
    udpn["send"] = notifyDirect;
    udpn["recv"] = receiveNotifications;

    root[F("lor")] = realtimeOverride;
  }

  root[F("mainseg")] = strip.getMainSegmentId();

  JsonArray seg = root.createNestedArray("seg");
  for (byte s = 0; s < strip.getMaxSegments(); s++) {
    WS2812FX::Segment &sg = strip.getSegment(s);
    if (sg.isActive()) {
      JsonObject seg0 = seg.createNestedObject();
      serializeSegment(seg0, sg, s, forPreset, segmentBounds);
    } else if (forPreset && segmentBounds) { //disable segments not part of preset
      JsonObject seg0 = seg.createNestedObject();
      seg0["stop"] = 0;
    }
  }
}

//by https://github.com/tzapu/WiFiManager/blob/master/WiFiManager.cpp
int getSignalQuality(int rssi)
{
    int quality = 0;

    if (rssi <= -100)
    {
        quality = 0;
    }
    else if (rssi >= -50)
    {
        quality = 100;
    }
    else
    {
        quality = 2 * (rssi + 100);
    }
    return quality;
}

void serializeInfo(JsonObject root)
{
  root[F("ver")] = versionString;
  root[F("vid")] = VERSION;
  //root[F("cn")] = WLED_CODENAME;

  JsonObject leds = root.createNestedObject("leds");
  leds[F("count")] = strip.getLengthTotal();

  leds[F("pwr")] = strip.currentMilliamps;
  leds["fps"] = strip.getFps();
  leds[F("maxpwr")] = (strip.currentMilliamps)? strip.ablMilliampsMax : 0;
  leds[F("maxseg")] = strip.getMaxSegments();
  //leds[F("seglock")] = false; //might be used in the future to prevent modifications to segment config
  
  uint8_t totalLC = 0;
  JsonArray lcarr = leds.createNestedArray(F("seglc"));
  uint8_t nSegs = strip.getLastActiveSegmentId();
  for (byte s = 0; s <= nSegs; s++) {
    uint8_t lc = strip.getSegment(s).getLightCapabilities();
    totalLC |= lc;
    lcarr.add(lc);
  }

  leds["lc"] = totalLC;

  leds[F("rgbw")] = strip.hasRGBWBus(); // deprecated, use info.leds.lc
  leds[F("wv")]   = totalLC & 0x02;     // deprecated, true if white slider should be displayed for any segment
  leds["cct"]     = totalLC & 0x04;     // deprecated, use info.leds.lc

  root[F("str")] = syncToggleReceive;

  root[F("name")] = serverDescription;
  root[F("udpport")] = udpPort;
  root["live"] = (bool)realtimeMode;
  root[F("liveseg")] = useMainSegmentOnly ? strip.getMainSegmentId() : -1;  // if using main segment only for live
  //root[F("mso")] = useMainSegmentOnly;  // using main segment only for live

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

  if (realtimeIP[0] == 0)
  {
    root[F("lip")] = "";
  } else {
    root[F("lip")] = realtimeIP.toString();
  }

  #ifdef WLED_ENABLE_WEBSOCKETS
  root[F("ws")] = ws.count();
  #else
  root[F("ws")] = -1;
  #endif

  root[F("fxcount")] = strip.getModeCount();
  root[F("palcount")] = strip.getPaletteCount();

  JsonObject wifi_info = root.createNestedObject("wifi");
  wifi_info[F("bssid")] = WiFi.BSSIDstr();
  int qrssi = WiFi.RSSI();
  wifi_info[F("rssi")] = qrssi;
  wifi_info[F("signal")] = getSignalQuality(qrssi);
  wifi_info[F("channel")] = WiFi.channel();

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
  root[F("arch")] = "esp32";
  root[F("core")] = ESP.getSdkVersion();
  //root[F("maxalloc")] = ESP.getMaxAllocHeap();
  #ifdef WLED_DEBUG
    root[F("resetReason0")] = (int)rtc_get_reset_reason(0);
    root[F("resetReason1")] = (int)rtc_get_reset_reason(1);
  #endif
  root[F("lwip")] = 0; //deprecated
  #else
  root[F("arch")] = "esp8266";
  root[F("core")] = ESP.getCoreVersion();
  //root[F("maxalloc")] = ESP.getMaxFreeBlockSize();
  #ifdef WLED_DEBUG
    root[F("resetReason")] = (int)ESP.getResetInfoPtr()->reason;
  #endif
  root[F("lwip")] = LWIP_VERSION_MAJOR;
  #endif

  root[F("freeheap")] = ESP.getFreeHeap();
  #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_PSRAM)
  if (psramFound()) root[F("psram")] = ESP.getFreePsram();
  #endif
  root[F("uptime")] = millis()/1000 + rolloverMillis*4294967;

  usermods.addToJsonInfo(root);

  byte os = 0;
  #ifdef WLED_DEBUG
  os  = 0x80;
  #endif
  #ifndef WLED_DISABLE_ALEXA
  os += 0x40;
  #endif
  #ifndef WLED_DISABLE_BLYNK
  os += 0x20;
  #endif
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

  root[F("brand")] = "WLED";
  root[F("product")] = F("FOSS");
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
    uint16_t count = 0;
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

void serializePalettes(JsonObject root, AsyncWebServerRequest* request)
{
  #ifdef ESP8266
  int itemPerPage = 5;
  #else
  int itemPerPage = 8;
  #endif

  int page = 0;
  if (request->hasParam("page")) {
    page = request->getParam("page")->value().toInt();
  }

  int palettesCount = strip.getPaletteCount();

  int maxPage = (palettesCount -1) / itemPerPage;
  if (page > maxPage) page = maxPage;

  int start = itemPerPage * page;
  int end = start + itemPerPage;
  if (end >= palettesCount) end = palettesCount;

  root[F("m")] = maxPage;
  JsonObject palettes  = root.createNestedObject("p");

  for (int i = start; i < end; i++) {
    JsonArray curPalette = palettes.createNestedArray(String(i));
    switch (i) {
      case 0: //default palette
        setPaletteColors(curPalette, PartyColors_p); 
        break;
      case 1: //random
          curPalette.add("r");
          curPalette.add("r");
          curPalette.add("r");
          curPalette.add("r");
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
      case 5: {//primary + secondary (+tert if not off), more distinct
      
        curPalette.add("c1");
        curPalette.add("c1");
        curPalette.add("c1");
        curPalette.add("c1");
        curPalette.add("c1");
        curPalette.add("c2");
        curPalette.add("c2");
        curPalette.add("c2");
        curPalette.add("c2");
        curPalette.add("c2");
        curPalette.add("c3");
        curPalette.add("c3");
        curPalette.add("c3");
        curPalette.add("c3");
        curPalette.add("c3");
        curPalette.add("c1");
        break;}
      case 6: //Party colors
        setPaletteColors(curPalette, PartyColors_p);
        break;
      case 7: //Cloud colors
        setPaletteColors(curPalette, CloudColors_p);
        break;
      case 8: //Lava colors
        setPaletteColors(curPalette, LavaColors_p);
        break;
      case 9: //Ocean colors
        setPaletteColors(curPalette, OceanColors_p);
        break;
      case 10: //Forest colors
        setPaletteColors(curPalette, ForestColors_p);
        break;
      case 11: //Rainbow colors
        setPaletteColors(curPalette, RainbowColors_p);
        break;
      case 12: //Rainbow stripe colors
        setPaletteColors(curPalette, RainbowStripeColors_p);
        break;

      default:
        if (i < 13) {
          break;
        }
        byte tcp[72];
        memcpy_P(tcp, (byte*)pgm_read_dword(&(gGradientPalettes[i - 13])), 72);
        setPaletteColors(curPalette, tcp);
        break;
    }
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

void serveJson(AsyncWebServerRequest* request)
{
  byte subJson = 0;
  const String& url = request->url();
  if      (url.indexOf("state") > 0) subJson = 1;
  else if (url.indexOf("info")  > 0) subJson = 2;
  else if (url.indexOf("si")    > 0) subJson = 3;
  else if (url.indexOf("nodes") > 0) subJson = 4;
  else if (url.indexOf("palx")  > 0) subJson = 5;
  #ifdef WLED_ENABLE_JSONLIVE
  else if (url.indexOf("live")  > 0) {
    serveLiveLeds(request);
    return;
  }
  #endif
  else if (url.indexOf(F("eff")) > 0) {
    request->send_P(200, "application/json", JSON_mode_names);
    return;
  }
  else if (url.indexOf("pal") > 0) {
    request->send_P(200, "application/json", JSON_palette_names);
    return;
  }
  else if (url.indexOf("cfg") > 0 && handleFileRead(request, "/cfg.json")) {
    return;
  }
  else if (url.length() > 6) { //not just /json
    request->send(  501, "application/json", F("{\"error\":\"Not implemented\"}"));
    return;
  }

  #ifdef WLED_USE_DYNAMIC_JSON
  AsyncJsonResponse* response = new AsyncJsonResponse(JSON_BUFFER_SIZE);
  #else
  if (!requestJSONBufferLock(17)) return;
  AsyncJsonResponse *response = new AsyncJsonResponse(&doc);
  #endif

  JsonObject lDoc = response->getRoot();

  switch (subJson)
  {
    case 1: //state
      serializeState(lDoc); break;
    case 2: //info
      serializeInfo(lDoc); break;
    case 4: //node list
      serializeNodes(lDoc); break;
    case 5: //palettes
      serializePalettes(lDoc, request); break;
    default: //all
      JsonObject state = lDoc.createNestedObject("state");
      serializeState(state);
      JsonObject info = lDoc.createNestedObject("info");
      serializeInfo(info);
      if (subJson != 3)
      {
        doc[F("effects")]  = serialized((const __FlashStringHelper*)JSON_mode_names);
        doc[F("palettes")] = serialized((const __FlashStringHelper*)JSON_palette_names);
      }
  }

  DEBUG_PRINT("JSON buffer size: ");
  DEBUG_PRINTLN(lDoc.memoryUsage());

  response->setLength();
  request->send(response);
  releaseJSONBufferLock();
}

#ifdef WLED_ENABLE_JSONLIVE
#define MAX_LIVE_LEDS 180

bool serveLiveLeds(AsyncWebServerRequest* request, uint32_t wsClient)
{
  #ifdef WLED_ENABLE_WEBSOCKETS
  AsyncWebSocketClient * wsc = nullptr;
  if (!request) { //not HTTP, use Websockets
    wsc = ws.client(wsClient);
    if (!wsc || wsc->queueLength() > 0) return false; //only send if queue free
  }
  #endif

  uint16_t used = strip.getLengthTotal();
  uint16_t n = (used -1) /MAX_LIVE_LEDS +1; //only serve every n'th LED if count over MAX_LIVE_LEDS
  char buffer[2000];
  strcpy_P(buffer, PSTR("{\"leds\":["));
  obuf = buffer;
  olen = 9;

  for (uint16_t i= 0; i < used; i += n)
  {
    uint32_t c = strip.getPixelColor(i);
    uint8_t r = qadd8(W(c), R(c)); //add white channel to RGB channels as a simple RGBW -> RGB map
    uint8_t g = qadd8(W(c), G(c));
    uint8_t b = qadd8(W(c), B(c));
    olen += sprintf(obuf + olen, "\"%06X\",", RGBW32(r,g,b,0));
  }
  olen -= 1;
  oappend((const char*)F("],\"n\":"));
  oappendi(n);
  oappend("}");
  if (request) {
    request->send(200, "application/json", buffer);
  }
  #ifdef WLED_ENABLE_WEBSOCKETS
  else {
    wsc->text(obuf, olen);
  }
  #endif
  return true;
}
#endif