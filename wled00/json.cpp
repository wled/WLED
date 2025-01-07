#include "wled.h"

#include "palettes.h"

#define JSON_PATH_STATE      1
#define JSON_PATH_INFO       2
#define JSON_PATH_STATE_INFO 3
#define JSON_PATH_NODES      4
#define JSON_PATH_PALETTES   5
#define JSON_PATH_FXDATA     6
#define JSON_PATH_NETWORKS   7
#define JSON_PATH_EFFECTS    8

// begin WLEDMM
#ifdef ARDUINO_ARCH_ESP32
#include <Esp.h>
// get the right RTC.H for each MCU
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#if CONFIG_IDF_TARGET_ESP32S2
#include <esp32s2/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32C3
#include <esp32c3/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32S3
#include <esp32s3/rom/rtc.h>
#elif CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include <esp32/rom/rtc.h>
#endif
#else // ESP32 Before IDF 4.0
#include <rom/rtc.h>
#endif
#else // for 8266
#include <Esp.h>
#include <user_interface.h>

#include <core_esp8266_features.h>
#include <core_version.h>
#include <spi_vendors.h>

#include <flash_utils.h>
#include <memory>
#include <cont.h>
#include <coredecls.h>
#endif
// end WLEDMM

/*
 * JSON API (De)serialization
 */

static bool inDeepCall = false; // WLEDMM needed so that recursive deserializeSegment() does not remove locks too early

// WLEDMM caution - this function may run outside of arduino loop context (async_tcp with priority=3)
bool deserializeSegment(JsonObject elem, byte it, byte presetId)
{
  const bool iAmGroot = !inDeepCall;  // WLEDMM will only be true if this is the toplevel of the recursion.
  //WLEDMM add USER_PRINT
  #ifdef WLED_DEBUG
  if (elem.size()!=1 || elem["stop"] != 0) { // not for {"stop":0}
    String temp;
    serializeJson(elem, temp);
    USER_PRINTF("deserializeSegment %s\n", temp.c_str());
  }
  #endif

  byte id = elem["id"] | it;
  if (id >= strip.getMaxSegments()) return false;

  //WLEDMM: add compatibility for SR presets
  #ifndef WLED_DISABLE_2D
    // Serial.printf("before %d: %s %s %s %s\n", id, elem["start"].as<std::string>().c_str(), elem["stop"].as<std::string>().c_str(), elem["startY"].as<std::string>().c_str(), elem["stopY"].as<std::string>().c_str());
  if (strip.isMatrix && !elem["start"].isNull() && !elem["stop"].isNull() && elem["startY"].isNull() && elem["stopY"].isNull()) {
    uint16_t start1=elem["start"], stop1=elem["stop"];
    elem["start"] = start1%Segment::maxWidth;
    elem["startY"]= Segment::maxWidth?(start1 / Segment::maxWidth):0;
    elem["stop"] = (stop1-1)%Segment::maxWidth + 1;
    elem["stopY"]= Segment::maxWidth?((stop1-1) / Segment::maxWidth) + 1:0;
    // Serial.printf("after %s %s %s %s\n", elem["start"].as<std::string>().c_str(), elem["stop"].as<std::string>().c_str(), elem["startY"].as<std::string>().c_str(), elem["stopY"].as<std::string>().c_str());
  }
  #endif
  if (!elem["c1x"].isNull()) elem["c1"] = elem["c1x"];
  if (!elem["c2x"].isNull()) elem["c2"] = elem["c2x"];
  if (!elem["c3x"].isNull()) elem["c3"] = elem["c3x"];
  if (!elem["rev2D"].isNull()) elem["rY"] = elem["rev2D"];
  if (!elem["rot2D"].isNull()) elem["tp"] = elem["rot2D"];

  bool newSeg = false;
  int stop = elem["stop"] | -1;

  // if using vectors use this code to append segment
  if (id >= strip.getSegmentsNum()) {
    if (stop <= 0) return false; // ignore empty/inactive segments
    strip.appendSegment(Segment(0, strip.getLengthTotal()));
    id = strip.getSegmentsNum()-1; // segments are added at the end of list
    newSeg = true;
  }

  // WLEDMM: before changing segments, make sure our strip is _not_ servicing effects in parallel
  suspendStripService = true; // temporarily lock out strip updates
  if (strip.isServicing()) {
    USER_PRINTLN(F("deserializeSegment(): strip is still drawing effects."));
    strip.waitUntilIdle();
  }

  Segment& seg = strip.getSegment(id);
  Segment prev = seg; //make a backup so we can tell if something changed // WLEDMM fixMe: copy constructor = waste of memory

  uint16_t start = elem["start"] | seg.start;
  if (stop < 0) {
    int len = elem["len"]; // WLEDMM bugfix for broken presets with len < 0
    stop = (len > 0) ? start + len : seg.stop;
  }
  // 2D segments
  uint16_t startY = elem["startY"] | seg.startY;
  uint16_t stopY = elem["stopY"] | seg.stopY;

  //repeat, multiplies segment until all LEDs are used, or max segments reached
  bool repeat = elem["rpt"] | false;
  if (repeat && stop>0) {
    elem.remove("id");  // remove for recursive call
    elem.remove("rpt"); // remove for recursive call
    elem.remove("n");   // remove for recursive call
    uint16_t len = (stop >= start) ? (stop - start) : 0;  // WLEDMM stop < 1 is allowed, so we need to avoid underflow
    for (size_t i=id+1; i<strip.getMaxSegments(); i++) {
      start = start + len;
      if (start >= strip.getLengthTotal()) break;
      //TODO: add support for 2D
      elem["start"] = start;
      elem["stop"]  = start + len;
      elem["rev"]   = !elem["rev"]; // alternate reverse on even/odd segments
      inDeepCall = true;  // WLEDMM remember that we are going into recursion
      deserializeSegment(elem, i, presetId); // recursive call with new id // WLEDMM expect problems like heap overflow
      if (iAmGroot) inDeepCall = false;  // WLEDMM toplevel -> reset recursion flag
    }
    if (iAmGroot) suspendStripService = false; // WLEDMM release lock
    return true;
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
    if (len > 0 && len < 32) {
      seg.name = new(std::nothrow) char[len+1];
      if (seg.name) strlcpy(seg.name, name, len+1);
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
  uint16_t of  = seg.offset;
  uint8_t  soundSim = elem["si"] | seg.soundSim;
  uint8_t  map1D2D  = elem["m12"] | seg.map1D2D;

  //WLEDMM jMap
  if (map1D2D == M12_jMap && !seg.jMap)
    seg.createjMap();
  if (map1D2D != M12_jMap && seg.jMap)
    seg.deletejMap();

  if ((spc>0 && spc!=seg.spacing) || seg.map1D2D!=map1D2D) seg.markForBlank(); // clear spacing gaps // WLEDMM softhack007: this line sometimes crashes with "Stack canary watchpoint triggered (async_tcp)"

  seg.map1D2D  = constrain(map1D2D, 0, 7);
  seg.soundSim = constrain(soundSim, 0, 1);

  uint8_t set = elem[F("set")] | seg.set;
  seg.set = constrain(set, 0, 3);

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
  seg.setUp(start, stop, grp, spc, of, startY, stopY);
	if (newSeg) seg.refreshLightCapabilities(); // fix for #3403

  if (seg.reset && seg.stop == 0) {
    if (iAmGroot) suspendStripService = false; // WLEDMM release lock

    if (id == strip.getMainSegmentId()) strip.setMainSegmentId(0); // fix for #3403
    return true; // segment was deleted & is marked for reset, no need to change anything else
  }

  byte segbri = seg.opacity;
  if (getVal(elem["bri"], &segbri)) {
    if (segbri > 0) seg.setOpacity(segbri);
    seg.setOption(SEG_OPTION_ON, segbri); // use transition
  }

  bool on = elem["on"] | seg.on;
  if (elem["on"].is<const char*>() && elem["on"].as<const char*>()[0] == 't') on = !on;
  seg.setOption(SEG_OPTION_ON, on); // use transition

  //WLEDMM ARTIFX (but general usable)
  bool reset = elem["reset"];
  if (reset)
    seg.markForReset();

  bool frz = elem["frz"] | seg.freeze;
  if (elem["frz"].is<const char*>() && elem["frz"].as<const char*>()[0] == 't') frz = !seg.freeze;
  seg.freeze = frz;

  seg.setCCT(elem["cct"] | seg.cct);

  JsonArray colarr = elem["col"];
  if (!colarr.isNull())
  {
    if (seg.getLightCapabilities() & 3) {
      // segment has RGB or White
      for (size_t i = 0; i < 3; i++)
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
            if (kelvin == 0) seg.setColor(i, 0);
            if (kelvin >  0) colorKtoRGB(kelvin, brgbw);
            colValid = true;
          } else { //HEX string, e.g. "FFAA00"
            colValid = colorFromHexString(brgbw, hexCol);
          }
          for (size_t c = 0; c < 4; c++) rgbw[c] = brgbw[c];
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
  if (lx > 0) {
    parseLxJson(lx, id, false);
  }
  int ly = elem[F("ly")] | -1;
  if (ly > 0) {
    parseLxJson(ly, id, true);
  }
  #endif

  #ifndef WLED_DISABLE_2D
  bool reverse  = seg.reverse;
  bool mirror   = seg.mirror;
  #endif
  seg.selected  = elem["sel"] | seg.selected;
  seg.reverse   = elem["rev"] | seg.reverse;
  seg.mirror    = elem["mi"]  | seg.mirror;
  #ifndef WLED_DISABLE_2D
  bool reverse_y = seg.reverse_y;
  bool mirror_y  = seg.mirror_y;
  seg.reverse_y  = elem["rY"]  | seg.reverse_y;
  seg.mirror_y   = elem["mY"]  | seg.mirror_y;
  seg.transpose  = elem[F("tp")] | seg.transpose;
  if (seg.is2D() && (seg.map1D2D == M12_pArc || seg.map1D2D == M12_sCircle) && (reverse != seg.reverse || reverse_y != seg.reverse_y || mirror != seg.mirror || mirror_y != seg.mirror_y)) seg.markForBlank(); // clear entire segment (in case of Arc 1D to 2D expansion) WLEDMM: also Circle
  #endif

  byte fx = seg.mode;
  byte last = strip.getModeCount();
  // partial fix for #3605
  if (!elem["fx"].isNull() && elem["fx"].is<const char*>()) {
    const char *tmp = elem["fx"].as<const char *>();
    if (strlen(tmp) > 3 && (strchr(tmp,'r') || strchr(tmp,'~') != strrchr(tmp,'~'))) last = 0; // we have "X~Y(r|[w]~[-])" form
  }
  // end fix
  if (getVal(elem["fx"], &fx, 0, last)) { //load effect ('r' random, '~' inc/dec, 0-255 exact value, 5~10r pick random between 5 & 10)
    if (!presetId && currentPlaylist>=0) unloadPlaylist();
    if (fx != seg.mode) seg.setMode(fx, elem[F("fxdef")], elem[F("fxdef2")]); // WLEDMM fxdef2 added
  }

  //getVal also supports inc/decrementing and random
  getVal(elem["sx"], &seg.speed);
  getVal(elem["ix"], &seg.intensity);

  uint8_t pal = seg.palette;
  if (seg.getLightCapabilities() & 1) {  // ignore palette for White and On/Off segments
    if (getVal(elem["pal"], &pal)) seg.setPalette(pal);
  }

  getVal(elem["c1"], &seg.custom1);
  getVal(elem["c2"], &seg.custom2);
  uint8_t cust3 = seg.custom3;
  getVal(elem["c3"], &cust3); // we can't pass reference to bitfield
  seg.custom3 = constrain(cust3, 0, 31);

  seg.check1 = elem["o1"] | seg.check1;
  seg.check2 = elem["o2"] | seg.check2;
  seg.check3 = elem["o3"] | seg.check3;

  JsonArray iarr = elem[F("i")]; //set individual LEDs
  if (!iarr.isNull()) {
    uint8_t oldMap1D2D = seg.map1D2D;
    seg.map1D2D = M12_Pixels; // no mapping
    // WLEDMM begin - we need to init segment caches before putting any pixels
    if (strip.isServicing()) {
      USER_PRINTLN(F("deserializeSegment() image: strip is still drawing effects."));
      strip.waitUntilIdle();
    }
    seg.startFrame();
    // WLEDMM end

    // set brightness immediately and disable transition
    transitionDelayTemp = 0;
    jsonTransitionOnce = true;
    strip.setBrightness(scaledBri(bri), true);

    // freeze and init to black
    if (!seg.freeze) {
      seg.freeze = true;
      seg.fill(BLACK);  // WLEDMM why now?
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
  uint8_t diffresult = seg.differs(prev)  & 0x7F;
  if (diffresult > 0) {
    stateChanged = true;
    if ((seg.on == false) && (prev.on == true) && (prev.freeze == false)) prev.fill(BLACK); // WLEDMM: force BLACK if segment was turned off
    if (diffresult & (SEG_DIFFERS_BOUNDS | SEG_DIFFERS_GSO | SEG_DIFFERS_OPT)) {   // WLEDMM bouds, grouping, or options changed (mirror, reverse, transpose, mapping)
      if (!seg.freeze) seg.markForBlank();
      if (prev.isActive() && (diffresult & (SEG_DIFFERS_BOUNDS | SEG_DIFFERS_GSO)) && !prev.freeze && !seg.freeze) prev.fill(BLACK);   // WLEDMM fingers crossed
    }
  }

  if (iAmGroot) suspendStripService = false; // WLEDMM release lock
  return true;
}

// deserializes WLED state (fileDoc points to doc object if called from web server)
// presetId is non-0 if called from handlePreset()
// WLEDMM caution - this function may run outside of arduino loop context (async_tcp with priority=3)
bool deserializeState(JsonObject root, byte callMode, byte presetId)
{
  const bool iAmGroot = !inDeepCall;  // WLEDMM will only be true if this is the toplevel of the recursion.
  //WLEDMM add USER_PRINT
  #ifdef WLED_DEBUG
  String temp;
  serializeJson(root, temp);
  USER_PRINTF("deserializeState %s\n", temp.c_str());
  #endif

  bool stateResponse = root[F("v")] | false;

  //WLEDMM: store netDebug, also if not WLED_DEBUG 
  #if defined(WLED_DEBUG_HOST)
  bool oldValue = netDebugEnabled;
  netDebugEnabled = root[F("netDebug")] | netDebugEnabled;
  // USER_PRINTF("deserializeState %d (%d)\n", netDebugEnabled, oldValue);
  if (oldValue != netDebugEnabled) {
    pinManager.manageDebugTXPin();
    doSerializeConfig = true; //WLEDMM to make it will be stored in cfg.json! (tbd: check if this is the right approach)
  }
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
    if (tr >= 0)
    {
      transitionDelay = tr;
      transitionDelay *= 100;
      transitionDelayTemp = transitionDelay;
    }
  }

#ifdef ARDUINO_ARCH_ESP32
  delay(2); // WLEDMM experimental - de-serialize takes time, so allow other tasks to run
#endif

  // WLEDMM: before changing strip, make sure our strip is _not_ servicing effects in parallel
  suspendStripService = true; // temporarily lock out strip updates
  if (strip.isServicing()) {
    USER_PRINTLN(F("deserializeState(): strip is still drawing effects."));
    strip.waitUntilIdle();
  }

  // temporary transition (applies only once)
  tr = root[F("tt")] | -1;
  if (tr >= 0)
  {
    transitionDelayTemp = tr;
    transitionDelayTemp *= 100;
    jsonTransitionOnce = true;
  }
  strip.setTransition(transitionDelayTemp); // required here for color transitions to have correct duration

  tr = root[F("tb")] | -1;
  if (tr >= 0) strip.timebase = ((unsigned long)tr) - millis();

  JsonObject nl       = root["nl"];
  nightlightActive    = nl["on"]      | nightlightActive;
  nightlightDelayMins = nl["dur"]     | nightlightDelayMins;
  nightlightMode      = nl["mode"]    | nightlightMode;
  nightlightTargetBri = nl[F("tbri")] | nightlightTargetBri;

  JsonObject udpn      = root["udpn"];
  notifyDirect         = udpn["send"] | notifyDirect;
  syncGroups           = udpn["sgrp"] | syncGroups;
  receiveNotifications = udpn["recv"] | receiveNotifications;
  receiveGroups        = udpn["rgrp"] | receiveGroups;
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
      //bool didSet = false;
      for (size_t s = 0; s < strip.getSegmentsNum(); s++) {
        Segment &sg = strip.getSegment(s);
        if (sg.isActive() && sg.isSelected()) {
          inDeepCall = true;  // WLEDMM remember that we are going into recursion
          deserializeSegment(segVar, s, presetId);
          if (iAmGroot) inDeepCall = false;  // WLEDMM toplevel -> reset recursion flag
          //didSet = true;
        }
      }
      //TODO: not sure if it is good idea to change first active but unselected segment
      //if (!didSet) deserializeSegment(segVar, strip.getMainSegmentId(), presetId);
    } else {
      inDeepCall = true;  // WLEDMM remember that we are going into recursion
      deserializeSegment(segVar, id, presetId); //apply only the segment with the specified ID
      if (iAmGroot) inDeepCall = false;  // WLEDMM toplevel -> reset recursion flag
    }
  } else {
    size_t deleted = 0;
    JsonArray segs = segVar.as<JsonArray>();
    inDeepCall = true;  // WLEDMM remember that we are going into recursion
    for (JsonObject elem : segs) {
      if (deserializeSegment(elem, it++, presetId) && !elem["stop"].isNull() && elem["stop"]==0) deleted++;
    }
    if (strip.getSegmentsNum() > 3 && deleted >= strip.getSegmentsNum()/2U) strip.purgeSegments(); // batch deleting more than half segments
    if (iAmGroot) inDeepCall = false;  // WLEDMM toplevel -> reset recursion flag
  }

  usermods.readFromJsonState(root);

  //WLEDMM
  loadedLedmap = root[F("ledmap")] | loadedLedmap;
  loadLedmap = loadedLedmap>=0; //WLEDMM included 0 to switch back to default

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

  // applying preset (2 cases: a) API call includes all preset values ("pd"), b) API only specifies preset ID ("ps"))
  byte presetToRestore = 0;
  // a) already applied preset content (requires "seg" or "win" but will ignore the rest)
  if (!root["pd"].isNull() && stateChanged) {
    currentPreset = root[F("pd")] | currentPreset;
    if (root["win"].isNull()) presetCycCurr = currentPreset;
    presetToRestore = currentPreset; // stateUpdated() will clear the preset, so we need to restore it after
    //unloadPlaylist(); // applying a preset unloads the playlist, may be needed here too?
  } else if (!root["ps"].isNull()) {
    ps = presetCycCurr;
    if (root["win"].isNull() && getVal(root["ps"], &ps, 0, 0) && ps > 0 && ps < 251 && ps != currentPreset) {
      // b) preset ID only or preset that does not change state (use embedded cycling limits if they exist in getVal())
      presetCycCurr = ps;
      unloadPlaylist();          // applying a preset unloads the playlist
      applyPreset(ps, callMode); // async load from file system (only preset ID was specified)
      if (iAmGroot) suspendStripService = false; // WLEDMM release lock
      return stateResponse;
    }
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
  
  stateUpdated(callMode);
  if (presetToRestore) currentPreset = presetToRestore;

  if (iAmGroot) suspendStripService = false; // WLEDMM release lock
  return stateResponse;
}

void serializeSegment(JsonObject& root, Segment& seg, byte id, bool forPreset, bool segmentBounds)
{
  //WLEDMM add DEBUG_PRINT (not USER_PRINT)
  String temp;
  serializeJson(root, temp);
  DEBUG_PRINTF("serializeSegment %s\n", temp.c_str());

  root["id"] = id;
  if (segmentBounds) {
    root["start"] = seg.start;
    root["stop"] = seg.stop;
    if (strip.isMatrix) {
      root[F("startY")] = seg.startY;
      root[F("stopY")]  = seg.stopY;
    }
  }
  if (!forPreset) root["len"] = (seg.stop >= seg.start) ? (seg.stop - seg.start) : 0; // WLEDMM correct handling for stop=0
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
  //WLEDMM add DEBUG_PRINT (not USER_PRINT)
  String temp;
  serializeJson(root, temp);
  DEBUG_PRINTF("serializeState %d %s\n", forPreset, temp.c_str());

  if (includeBri) {
    root["on"] = (bri > 0);
    root["bri"] = briLast;
    root[F("transition")] = transitionDelay/100; //in 100ms
  }

  if (!forPreset) {
    //WLEDMM: store netDebug 
    #if defined(WLED_DEBUG_HOST)
      root[F("netDebug")] = netDebugEnabled;
    // USER_PRINTF("serializeState %d\n", netDebugEnabled);
    #endif

    // WLEDMM print error message to netDebug - esp32 only, as 8266 flash is very limited
#if defined(ARDUINO_ARCH_ESP32) && !defined(WLEDMM_SAVE_FLASH)
    String errPrefix = F("\nWLED error: ");
    String warnPrefix = F("WLED warning: ");
    switch(errorFlag) {
      case ERR_NONE: break;
      case ERR_DENIED:    USER_PRINTLN(errPrefix + F("Permission denied.")); break;
      case ERR_NOBUF:     USER_PRINTLN(warnPrefix + F("JSON buffer was not released in time, request timeout.")); break;
      case ERR_JSON:      USER_PRINTLN(errPrefix + F("JSON parsing failed (input too large?).")); break;
      case ERR_FS_BEGIN:  USER_PRINTLN(errPrefix + F("Could not init filesystem (no partition?).")); break;
      case ERR_FS_QUOTA:  USER_PRINTLN(errPrefix + F("FS is full or the maximum file size is reached.")); break;
      case ERR_FS_PLOAD:  USER_PRINTLN(warnPrefix + F("Tried loading a preset that does not exist.")); break;
      case ERR_FS_IRLOAD: USER_PRINTLN(warnPrefix + F("Tried loading an IR JSON cmd, but \"ir.json\" file does not exist.")); break;
      case ERR_FS_RMLOAD: USER_PRINTLN(warnPrefix + F("Tried loading a remote JSON cmd, but \"remote.json\" file does not exist.")); break;
      case ERR_FS_GENERAL: USER_PRINTLN(errPrefix + F("general unspecified filesystem error.")); break;
      default: USER_PRINT(errPrefix + F("error code = ")); USER_PRINTLN(errorFlag); break;
    }
#else
    if (errorFlag) { USER_PRINT(F("\nWLED error code = ")); USER_PRINTLN(errorFlag); }
#endif

    if (errorFlag) {root[F("error")] = errorFlag; errorFlag = ERR_NONE;} //prevent error message to persist on screen

    root["ps"] = (currentPreset > 0) ? currentPreset : -1;
    root[F("pl")] = currentPlaylist;

    usermods.addToJsonState(root);

    JsonObject nl = root.createNestedObject("nl");
    nl["on"] = nightlightActive;
    nl["dur"] = nightlightDelayMins;
    nl["mode"] = nightlightMode;
    nl[F("tbri")] = nightlightTargetBri;
    nl[F("rem")] = nightlightActive ? (int)(nightlightDelayMs - (millis() - nightlightStartTime)) / 1000 : -1; // seconds remaining

    JsonObject udpn = root.createNestedObject("udpn");
    udpn["send"] = notifyDirect;
    udpn["recv"] = receiveNotifications;
    udpn["sgrp"] = syncGroups;
    udpn["rgrp"] = receiveGroups;

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
  root[F("ledmap")] = loadedLedmap; //WLEDMM ledmaps will be stored in json so dropdown can display it
}

// begin WLEDMM
#ifdef ARDUINO_ARCH_ESP32
int getCoreResetReason(int core) {
  if (core >= ESP.getChipCores()) return 0;
  return((int)rtc_get_reset_reason(core));
}

String resetCode2Info(int reason) {
  switch(reason) {

    case 1 : //  1 =  Vbat power on reset
      return F("power-on"); break;
    case 2 : // 2 = this code is not defined on ESP32
      return F("exception"); break;
    case 3 : // 3 = Software reset digital core
       return F("SW reset"); break;
    case 12: //12 = Software reset CPU
       return F("SW restart"); break;
    case 5 : // 5 = Deep Sleep wakeup reset digital core
       return F("wakeup"); break;
    case 14:  //14 = for APP CPU, reset by PRO CPU
      return F("restart"); break;
    case 15: //15 = Reset when the vdd voltage is not stable (brownout)
      return F("brown-out"); break;

    // watchdog resets
    case 4 : // 4 = Legacy watch dog reset digital core
    case 6 : // 6 = Reset by SLC module, reset digital core
    case 7 : // 7 = Timer Group0 Watch dog reset digital core
    case 8 : // 8 = Timer Group1 Watch dog reset digital core
    case 9 : // 9 = RTC Watch dog Reset digital core
    case 11: //11 = Time Group watchdog reset CPU
    case 13: //13 = RTC Watch dog Reset CPU
    case 16: //16 = RTC Watch dog reset digital core and rtc module
    case 17: //17 = Time Group1 reset CPU
      return F("watchdog"); break;
    case 18: //18 = super watchdog reset digital core and rtc module
      return F("super watchdog"); break;

    // misc
    case 10: // 10 = Instrusion tested to reset CPU
      return F("intrusion"); break;
    case 19: //19 = glitch reset digital core and rtc module
      return F("glitch"); break;
    case 20: //20 = efuse reset digital core
      return F("EFUSE reset"); break;
    case 21: //21 = usb uart reset digital core
      return F("USB UART reset"); break;
    case 22: //22 = usb jtag reset digital core
     return F("JTAG reset"); break;
    case 23: //23 = power glitch reset digital core and rtc module
      return F("power glitch"); break;

    // unknown reason code
    case 0:
      return F(""); break;
    default: 
      return F("unknown"); break;
  }
}

esp_reset_reason_t getRestartReason() {
  return(esp_reset_reason());
}
String restartCode2InfoLong(esp_reset_reason_t reason) {
    switch (reason) {
#if !defined(WLEDMM_SAVE_FLASH)
      case ESP_RST_UNKNOWN:  return(F("Reset reason can not be determined")); break;
      case ESP_RST_POWERON:  return(F("Restart due to power-on event")); break;
      case ESP_RST_EXT:      return(F("Reset by external pin (not applicable for ESP32)")); break;
      case ESP_RST_SW:       return(F("Software restart via esp_restart()")); break;
      case ESP_RST_PANIC:    return(F("Software reset due to panic or unhandled exception (SW error)")); break;
      case ESP_RST_INT_WDT:  return(F("Reset (software or hardware) due to interrupt watchdog")); break;
      case ESP_RST_TASK_WDT: return(F("Reset due to task watchdog")); break;
      case ESP_RST_WDT:      return(F("Reset due to other watchdogs")); break;
      case ESP_RST_DEEPSLEEP:return(F("Restart after exiting deep sleep mode")); break;
      case ESP_RST_BROWNOUT: return(F("Brownout Reset (software or hardware)")); break;
      case ESP_RST_SDIO:     return(F("Reset over SDIO")); break;
#else
      case ESP_RST_UNKNOWN:  return(F("ESP_RST_UNKNOWN")); break;
      case ESP_RST_POWERON:  return(F("ESP_RST_POWERON")); break;
      case ESP_RST_EXT:      return(F("ESP_RST_EXT")); break;
      case ESP_RST_SW:       return(F("esp_restart()")); break;
      case ESP_RST_PANIC:    return(F("SW Panic or Exception")); break;
      case ESP_RST_INT_WDT:  return(F("ESP_RST_INT_WDT")); break;
      case ESP_RST_TASK_WDT: return(F("ESP_RST_TASK_WDT")); break;
      case ESP_RST_WDT:      return(F("ESP_RST_WDT")); break;
      case ESP_RST_DEEPSLEEP:return(F("ESP_RST_DEEPSLEEP")); break;
      case ESP_RST_BROWNOUT: return(F("Brownout Reset")); break;
      case ESP_RST_SDIO:     return(F("ESP_RST_SDIO")); break;
#endif
    }
  return(F("unknown"));
}
String restartCode2Info(esp_reset_reason_t reason) {
    switch (reason) {
#if !defined(WLEDMM_SAVE_FLASH)
      case ESP_RST_UNKNOWN:  return(F("unknown reason")); break;
      case ESP_RST_POWERON:  return(F("power-on event")); break;
      case ESP_RST_EXT:      return(F("external pin reset")); break;
      case ESP_RST_SW:       return(F("SW restart by esp_restart()")); break;
      case ESP_RST_PANIC:    return(F("SW error (panic or exception)")); break;
      case ESP_RST_INT_WDT:  return(F("interrupt watchdog")); break;
      case ESP_RST_TASK_WDT: return(F("task watchdog")); break;
      case ESP_RST_WDT:      return(F("other watchdog")); break;
      case ESP_RST_DEEPSLEEP:return(F("exit from deep sleep")); break;
      case ESP_RST_BROWNOUT: return(F("Brownout Reset")); break;
      case ESP_RST_SDIO:     return(F("Reset over SDIO")); break;
#else
      case ESP_RST_UNKNOWN:  return(F("unknown")); break;
      case ESP_RST_POWERON:  return(F("power-on")); break;
      case ESP_RST_EXT:      return(F("ext. pin reset")); break;
      case ESP_RST_SW:       return(F("SW restart")); break;
      case ESP_RST_PANIC:    return(F("SW panic or exception")); break;
      case ESP_RST_INT_WDT:  return(F("int. watchdog")); break;
      case ESP_RST_TASK_WDT: return(F("task watchdog")); break;
      case ESP_RST_WDT:      return(F("other watchdog")); break;
      case ESP_RST_DEEPSLEEP:return(F("deep sleep")); break;
      case ESP_RST_BROWNOUT: return(F("Brownout")); break;
      case ESP_RST_SDIO:     return(F("SDIO reset")); break;
#endif
    }
  return(F("unknown"));
}
#endif
// end WLEDMM

void serializeInfo(JsonObject root)
{
  root[F("ver")] = versionString;
  root[F("vid")] = VERSION;
  //root[F("cn")] = F(WLED_CODENAME);    //WLEDMM removed
  root[F("release")] = FPSTR(releaseString);
  root[F("rel")] = FPSTR(releaseString); //WLEDMM to add bin name

  JsonObject leds = root.createNestedObject("leds");
  leds[F("count")] = strip.getLengthTotal();
  leds[F("countP")] = strip.getLengthPhysical2(); //WLEDMM - getLengthPhysical plus plysical busses not supporting ABL (i.e. HUB75)
  leds[F("pwr")] = strip.currentMilliamps > 100 ? strip.currentMilliamps : 0; // WLEDMM show "not calculated" for HUB75, or when all LEDs are out
  leds["fps"] = strip.getFps();
  leds[F("maxpwr")] = (strip.currentMilliamps)? strip.ablMilliampsMax : 0;
  leds[F("maxseg")] = strip.getMaxSegments();
  //leds[F("actseg")] = strip.getActiveSegmentsNum();
  //leds[F("seglock")] = false; //might be used in the future to prevent modifications to segment config

  #ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    JsonObject matrix = leds.createNestedObject("matrix");
    matrix["w"] = Segment::maxWidth;
    matrix["h"] = Segment::maxHeight;
  }
  #endif

  uint8_t totalLC = 0;
  JsonArray lcarr = leds.createNestedArray(F("seglc"));
  size_t nSegs = strip.getSegmentsNum();
  for (size_t s = 0; s < nSegs; s++) {
    if (!strip.getSegment(s).isActive()) continue;
    uint8_t lc = strip.getSegment(s).getLightCapabilities();
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

  root[F("str")] = syncToggleReceive;

  root[F("name")] = serverDescription;
  root[F("udpport")] = udpPort;
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
    case REALTIME_MODE_DMX:      root["lm"] = F("DMX"); break;
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

  //WLEDMM: add busses.length to outputs
  JsonArray outputs = root.createNestedArray(F("outputs"));
  for (int8_t b = 0; b < busses.getNumBusses(); b++) {
    outputs.add(busses.getBus(b)->getLength());
  }

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
  //#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)
  #if CONFIG_IDF_TARGET_ESP32
    root[F("arch")] = "esp32";
  #else
    root[F("arch")] = ESP.getChipModel();
  #endif
  root[F("core")] = ESP.getSdkVersion();
  root[F("clock")] = ESP.getCpuFreqMHz();
  root[F("flash")] = (ESP.getFlashChipSize()/1024)/1024;
  //root[F("maxalloc")] = ESP.getMaxAllocHeap();
  #ifdef WLED_DEBUG
    root[F("resetReason0")] = (int)rtc_get_reset_reason(0);
    if(ESP.getChipCores() > 1)    // WLEDMM
   	  root[F("resetReason1")] = (int)rtc_get_reset_reason(1);
  #endif

  #if defined(ARDUINO_ARCH_ESP32)
  unsigned long t_wait = millis();
  while(strip.isUpdating() && (millis() - t_wait < 125)) delay(1); // WLEDMM try to catch a moment when strip is idle
  while(strip.isUpdating() && (millis() - t_wait < 160)) yield();  //        try harder
  //if (strip.isUpdating()) USER_PRINTLN("serializeInfo: strip still updating.");
  #endif

  root[F("lwip")] = 0; //deprecated
  root[F("totalheap")] = ESP.getHeapSize(); //WLEDMM
  #else
  root[F("arch")] = "esp8266";
  root[F("core")] = ESP.getCoreVersion();
  root[F("clock")] = ESP.getCpuFreqMHz();
  root[F("flash")] = (ESP.getFlashChipSize()/1024)/1024;
  //root[F("maxalloc")] = ESP.getMaxFreeBlockSize();
  #ifdef WLED_DEBUG
    root[F("resetReason")] = (int)ESP.getResetInfoPtr()->reason;
  #endif
  root[F("lwip")] = LWIP_VERSION_MAJOR;
  #endif
  root[F("getflash")] = ESP.getFlashChipSize(); //WLEDMM and Athom, works for both ESP32 and ESP8266

  root[F("freeheap")] = ESP.getFreeHeap();
  //WLEDMM: conditional on esp32
  #if defined(ARDUINO_ARCH_ESP32)
    root[F("freestack")] = uxTaskGetStackHighWaterMark(NULL); //WLEDMM
    root[F("minfreeheap")] = ESP.getMinFreeHeap();
  #endif
  #if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM)
  if (psramFound()) {
    root[F("tpram")] = ESP.getPsramSize(); //WLEDMM
    root[F("psram")] = ESP.getFreePsram();
    root[F("psusedram")] = ESP.getMinFreePsram();
    #if CONFIG_ESP32S3_SPIRAM_SUPPORT  // WLEDMM -S3 has "qspi" or "opi" PSRAM mode
    #if CONFIG_SPIRAM_MODE_OCT
      root[F("psrmode")]  = F("🚀 OPI");
    #elif CONFIG_SPIRAM_MODE_QUAD
      root[F("psrmode")]  = F("qspi");
    #endif
    #endif
  }
  #else
  // for testing
  //  root[F("tpram")] = 4194304; //WLEDMM
  //  root[F("psram")] = 4193000;
  //  root[F("psusedram")] = 3083000;
  #endif

  // begin WLEDMM
  #ifdef ARDUINO_ARCH_ESP32
  root[F("e32core0code")] = (int)rtc_get_reset_reason(0);
  root[F("e32core0text")] = resetCode2Info(rtc_get_reset_reason(0));
  if(ESP.getChipCores() > 1) {
    root[F("e32core1code")] = (int)rtc_get_reset_reason(1);
    root[F("e32core1text")] = resetCode2Info(rtc_get_reset_reason(1));
  }
  root[F("e32code")] = (int)getRestartReason();
  root[F("e32text")] = restartCode2Info(getRestartReason());

  static char msgbuf[32];
  snprintf(msgbuf, sizeof(msgbuf)-1, "%s rev.%d", ESP.getChipModel(), ESP.getChipRevision());
  root[F("e32model")] = msgbuf;
  root[F("e32cores")] = ESP.getChipCores();
  root[F("e32speed")] = ESP.getCpuFreqMHz();
  root[F("e32flash")] = int((ESP.getFlashChipSize()/1024)/1024);
  root[F("e32flashspeed")] = int(ESP.getFlashChipSpeed()/1000000);
  root[F("e32flashmode")] = int(ESP.getFlashChipMode());
  switch (ESP.getFlashChipMode()) {
    // missing: Octal modes
    case FM_QIO:  root[F("e32flashtext")] = F(" (QIO)"); break;
    case FM_QOUT: root[F("e32flashtext")] = F(" (QOUT)");break;
    case FM_DIO:  root[F("e32flashtext")] = F(" (DIO)"); break;
    case FM_DOUT: root[F("e32flashtext")] = F(" (DOUT or other)");break;
    #if defined(CONFIG_IDF_TARGET_ESP32S3) && CONFIG_ESPTOOLPY_FLASHMODE_OPI
      case FM_FAST_READ: root[F("e32flashtext")] = F(" (🚀OPI)");break;
    #else
      case FM_FAST_READ: root[F("e32flashtext")] = F(" (fast_read)");break;
    #endif
    case FM_SLOW_READ: root[F("e32flashtext")] = F(" (slow_read)");break;
    default: root[F("e32flashtext")] = F(" (other)"); break;
  }

  #else // for 8266
  root[F("e32core0code")] = (int)ESP.getResetInfoPtr()->reason;
  root[F("e32core0text")] = ESP.getResetReason();

  root[F("e32model")] = F("ESP8266  (id 0x") + String(ESP.getChipId(), 16) + String(") ");      // can only be "ESP8266EX" or "ESP8285"
  root[F("e32cores")] = 1;
  root[F("e32speed")] = ESP.getCpuFreqMHz();
  root[F("e32flash")] = int((ESP.getFlashChipRealSize()/1024)/1024);
  root[F("e32flashspeed")] = int(ESP.getFlashChipSpeed()/1000000);
  root[F("e32flashmode")] = int(ESP.getFlashChipMode());
  switch (ESP.getFlashChipMode()) {
    case FM_QIO:  root[F("e32flashtext")] = F(" (QIO)"); break;
    case FM_QOUT: root[F("e32flashtext")] = F(" (QOUT)");break;
    case FM_DIO:  root[F("e32flashtext")] = F(" (DIO)"); break;
    case FM_DOUT: root[F("e32flashtext")] = F(" (DOUT)");break;
    default: root[F("e32flashtext")] = F(" (other)"); break;
  }
  #endif
  #if defined(WLED_DEBUG) || defined(WLED_DEBUG_HOST) || defined(SR_DEBUG) || defined(SR_STATS)
  // WLEDMM add status of Serial, including pin alloc
  root[F("serialOnline")] = Serial ? (canUseSerial()?F("Serial ready ☾"):F("Serial in use ☾")) : F("Serial disconected ☾");  // "Disconnected" may happen on boards with USB CDC
  root[F("sRX")] = pinManager.isPinAllocated(hardwareRX) ? pinManager.getPinOwnerText(hardwareRX): F("free");
  root[F("sTX")] = pinManager.isPinAllocated(hardwareTX) ? pinManager.getPinOwnerText(hardwareTX): F("free");
  #endif
  // end WLEDMM

  root[F("uptime")] = millis()/1000 + rolloverMillis*4294967;

  usermods.addToJsonInfo(root);

  uint16_t os = 0;
  #ifdef WLED_DEBUG
  os  = 0x80;
  #endif
  //WLEDMM: WLED_DEBUG_HOST independent from WLED_DEBUG
  #ifdef WLED_DEBUG_HOST
  os  = 0x80; //WLEDMM: also if not WLED_DEBUG (on off button Net Debug/Net Print)
  os |= 0x0100;
  if (!netDebugEnabled) os &= ~0x0080;
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

  root[F("brand")] = F(WLED_BRAND); //WLEDMM + Moustachauve/Wled-Native
  root[F("product")] = F(WLED_PRODUCT_NAME); //WLEDMM + Moustachauve/Wled-Native
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
  byte tcp[72 +4] = { 255 }; // WLEDMM bugfix - use extra element as "stop" marker (=255) for setPaletteColors(). And no, I won't cry over 4 bytes wasted ;-)
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
  int customPalettes = strip.customPalettes.size();

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
      case 1: //WLEDMM random MM
          curPalette.add("r");
          curPalette.add("r");
          curPalette.add("r");
          curPalette.add("r");
        break;
      case 74: //WLEDMM random AC
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
      case 5: //primary + secondary (+tertiary if not off), more distinct
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
        break;
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
        {
        if (i>=palettesCount) {
          setPaletteColors(curPalette, strip.customPalettes[i - palettesCount]);
        } else {
          // WLEDMM workaround for palettes index overflow at i=74 -> gGradientPalettes index=61 out of bounds.
          int palIndex = i-13;
          constexpr int palMax = sizeof(gGradientPalettes)/sizeof(gGradientPalettes[0]) -1;
          if ((palIndex < 0) || (palIndex > palMax)) { 
            DEBUG_PRINTF("WARNING gGradientPalettes[%d] is out of bounds! max=%d. (json.cpp)\n", palIndex, palMax);
            palIndex = palMax;  // use last valid array item
          }
          memset(tcp, 255, sizeof(tcp));  // WLEDMM pre-fill buffer with dummy values, to avoid array overrun in setPaletteColors in case of "unterminated" palette entry
          // WLEDMM end
          memcpy_P(tcp, (byte*)pgm_read_dword(&(gGradientPalettes[palIndex])), 72);
          setPaletteColors(curPalette, tcp);
        }
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
    node["ssid"]    = WiFi.SSID(i);
    node["rssi"]    = WiFi.RSSI(i);
    node["bssid"]   = WiFi.BSSIDstr(i);
    node["channel"] = WiFi.channel(i);
    node["enc"]     = WiFi.encryptionType(i);
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
  char lineBuffer[192] = { 0 };
  for (size_t i = 0; i < strip.getModeCount(); i++) {
    strncpy_P(lineBuffer, strip.getModeData(i), sizeof(lineBuffer)-1);
    if (lineBuffer[0] != 0) {
      char* dataPtr = strchr(lineBuffer,'@');
      if (dataPtr) fxdata.add(dataPtr+1);
      else         fxdata.add("");
    }
  }
}

// deserializes mode names string into JsonArray
// also removes effect data extensions (@...) from deserialized names
void serializeModeNames(JsonArray arr) {
  char lineBuffer[192] = { 0 };
  for (size_t i = 0; i < strip.getModeCount(); i++) {
    strncpy_P(lineBuffer, strip.getModeData(i), sizeof(lineBuffer)-1);
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
  byte subJson = 0;
  const String& url = request->url();
  if      (url.indexOf("state") > 0) subJson = JSON_PATH_STATE;
  else if (url.indexOf("info")  > 0) subJson = JSON_PATH_INFO;
  else if (url.indexOf("si")    > 0) subJson = JSON_PATH_STATE_INFO;
  else if (url.indexOf("nodes") > 0) subJson = JSON_PATH_NODES;
  else if (url.indexOf("eff")   > 0) subJson = JSON_PATH_EFFECTS;
  else if (url.indexOf("palx")  > 0) subJson = JSON_PATH_PALETTES;
  else if (url.indexOf("fxda")  > 0) subJson = JSON_PATH_FXDATA;
  else if (url.indexOf("net") > 0) subJson = JSON_PATH_NETWORKS;
  #ifdef WLED_ENABLE_JSONLIVE
  else if (url.indexOf("live")  > 0) {
    serveLiveLeds(request);
    return;
  }
  #endif
  else if (url.indexOf(F("eff")) > 0) {
    // this serves just effect names without FX data extensions in names
    if (requestJSONBufferLock(19)) {
      AsyncJsonResponse* response = new AsyncJsonResponse(&doc, true);  // array document
      JsonArray lDoc = response->getRoot();
      serializeModeNames(lDoc); // remove WLED-SR extensions from effect names
      response->setLength();
      request->send(response);
      releaseJSONBufferLock();
    } else {
      request->send(503, "application/json", F("{\"error\":3}"));
    }
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
    request->send(501, "application/json", F("{\"error\":\"Not implemented\"}"));
    return;
  }

  if (!requestJSONBufferLock(17)) {
    request->send(503, "application/json", F("{\"error\":3}"));
    return;
  }
  // releaseJSONBufferLock() will be called when "response" is destroyed (from AsyncWebServer)
  // make sure you delete "response" if no "request->send(response);" is made
  LockedJsonResponse *response = new LockedJsonResponse(&doc, subJson==JSON_PATH_FXDATA || subJson==JSON_PATH_EFFECTS); // will clear and convert JsonDocument into JsonArray if necessary

  JsonVariant lDoc = response->getRoot();

  switch (subJson)
  {
    case JSON_PATH_STATE:
      serializeState(lDoc); break;
    case JSON_PATH_INFO:
      serializeInfo(lDoc); break;
    case JSON_PATH_NODES:
      serializeNodes(lDoc); break;
    case JSON_PATH_PALETTES:
      serializePalettes(lDoc, request); break;
      //serializePalettes(lDoc, request->hasParam("page") ? request->getParam("page")->value().toInt() : 0); break;
    case JSON_PATH_EFFECTS:
      serializeModeNames(lDoc); break;
    case JSON_PATH_FXDATA:
      serializeModeData(lDoc.as<JsonArray>()); break;
    case JSON_PATH_NETWORKS:
      serializeNetworks(lDoc); break;
    default: //all
      JsonObject state = lDoc.createNestedObject("state");
      serializeState(state);
      JsonObject info = lDoc.createNestedObject("info");
      serializeInfo(info);
      if (subJson != JSON_PATH_STATE_INFO)
      {
        JsonArray effects = lDoc.createNestedArray(F("effects"));
        serializeModeNames(effects); // remove WLED-SR extensions from effect names
        lDoc[F("palettes")] = serialized((const __FlashStringHelper*)JSON_palette_names);
      }
      //lDoc["m"] = lDoc.memoryUsage(); // JSON buffer usage, for remote debugging
  }

  DEBUG_PRINTF("JSON buffer size: %u for request: %d (%s)\n", lDoc.memoryUsage(), subJson, url.c_str());

  response->setLength();
  request->send(response);
}

#ifdef WLED_ENABLE_JSONLIVE
#define MAX_LIVE_LEDS 180

#warning "JSON Live enabled"

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
#ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    // ignore anything behid matrix (i.e. extra strip)
    used = Segment::maxWidth*Segment::maxHeight; // always the size of matrix (more or less than strip.getLengthTotal())
    n = 1;
    if (used > MAX_LIVE_LEDS) n = 2;
    if (used > MAX_LIVE_LEDS*4) n = 4;
  }
#endif

  DynamicBuffer buffer(9 + (9*MAX_LIVE_LEDS) + 7 + 5 + 6 + 5 + 6 + 5 + 2);  
  char* buf = buffer.data();      // assign buffer for oappnd() functions
  strncpy_P(buffer.data(), PSTR("{\"leds\":["), buffer.size());
  buf += 9; // sizeof(PSTR()) from last line

  for (size_t i= 0; i < used; i += n)
  {
    uint32_t c = strip.getPixelColor(i);
    // WLEDMM begin: live leds with color gamma correction
    uint8_t w = W(c);  // not sure why, but it looks better if always using "white" without corrections
    uint8_t r,g,b;
    if (gammaCorrectPreview) {
      r = qadd8(w, unGamma8(R(c))); //R, add white channel to RGB channels as a simple RGBW -> RGB map
      g = qadd8(w, unGamma8(G(c))); //G
      b = qadd8(w, unGamma8(B(c))); //B
    } else {
    // WLEDMM end
      r = qadd8(w, R(c)); //add white channel to RGB channels as a simple RGBW -> RGB map
      g = qadd8(w, G(c));
      b = qadd8(w, B(c));
    }
    buf += sprintf_P(buf, PSTR("\"%06X\","), RGBW32(r,g,b,0));
  }
  buf--;  // remove last comma
  buf += sprintf_P(buf, PSTR("],\"n\":%d"), n);
  (*buf++) = '}';
  (*buf++) = 0;
  
  if (request) {
    request->send(200, "application/json", toString(std::move(buffer)));
  }
  #ifdef WLED_ENABLE_WEBSOCKETS
  else {
    wsc->text(toString(std::move(buffer)));
  }
  #endif  
  return true;
}
#endif
