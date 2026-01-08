// 7-segment countdown usermod overlay
// Renders a 6-digit, two-separator seven-segment display as an overlay mask over the
// underlying WLED pixels. Lit mask pixels keep the original effect/color, masked pixels
// are forced to black.
#include "wled.h"
#include "7_seg_clock_countdown.h"


class SevenSegClockCountdown : public Usermod {
private:
  // Ensures the mask buffer matches the panel size.
  void ensureMaskSize() {
    if (mask.size() != TOTAL_PANEL_LEDS) mask.assign(TOTAL_PANEL_LEDS, 0);
  }
  // Clears the mask to fully transparent (all 0 → cleared when applied).
  void clearMask() {
    std::fill(mask.begin(), mask.end(), 0);
  }
  // Sets a contiguous LED range in the mask to 1 (keep underlying pixel).
  void setRangeOn(uint16_t start, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
      uint16_t idx = start + i;
      if (idx < mask.size()) mask[idx] = 1;
    }
  }

  // Time helpers --------------------------------------------------------------
  // Calculates hundredths of a second (0..99) within the current second.
  // If countDown=true: 99→0; otherwise: 0→99.
  int getHundredths(int currentSeconds, bool countDown) {
    unsigned long now = millis();

    // Resynchronize on second changes
    if (currentSeconds != lastSecondValue) {
      lastSecondValue = currentSeconds;
      lastSecondMillis = now;
      return countDown ? 99 : 0; // start at boundary
    }

    unsigned long delta = now - lastSecondMillis;

    int hundredths;
    if (countDown) {
      // 99 → 0 across the second
      hundredths = 99 - (int)(delta / 10);
    } else {
      // 0 → 99 across the second
      hundredths = (int)(delta / 10);
    }

    if (hundredths < 0) hundredths = 0;
    if (hundredths > 99) hundredths = 99;

    return hundredths;
  }
 
  // Drawing helpers -----------------------------------------------------------
  // Draws HH:MM:SS into the mask. Separator behavior is controlled by SeperatorOn/Off.
  void drawClock() {
    clearMask();
    setDigitInt(0, hour(localTime) / 10);
    setDigitInt(1, hour(localTime) % 10);
    setDigitInt(2, minute(localTime) / 10);
    setDigitInt(3, minute(localTime) % 10);
    setDigitInt(4, second(localTime) / 10);
    setDigitInt(5, second(localTime) % 10);
    // Separator rules:
    // - both true: blink
    // - only SeperatorOn: always on
    // - only SeperatorOff: always off
    // - neither: blink
    if (SeperatorOn && SeperatorOff) {
      if (second(localTime) % 2) {
        setSeparator(1, sepsOn);
        setSeparator(2, sepsOn);
      }
    }
    else if (SeperatorOn) {
      setSeparator(1, sepsOn);
      setSeparator(2, sepsOn);
    }
    else if (SeperatorOff) {
      // explicitly off → do nothing
    }
    else {
      if (second(localTime) % 2) {
        setSeparator(1, sepsOn);
        setSeparator(2, sepsOn);
      }
    }
  }

  void revertMode(){
    Segment& selseg = strip.getSegment(0);
    selseg.setMode(prevMode, false);
    selseg.setColor(0, prevColor);
    selseg.speed=prevSpeed;
    selseg.intensity=prevIntensity;
    strip.setBrightness(prevBrightness);
  }

  void SaveMode(){
    Segment& selseg = strip.getSegment(0);
    prevMode=selseg.mode;
    prevColor=selseg.colors[0];
    prevSpeed=selseg.speed;
    prevIntensity=selseg.intensity;
    prevBrightness=bri;
  }

  void setMode(uint8_t mode, uint32_t color, uint8_t speed, uint8_t intensity, uint8_t brightness){
    Segment& selseg = strip.getSegment(0);
    selseg.setMode(mode, false);
    selseg.setColor(0, color);
    selseg.speed=speed;
    selseg.intensity=intensity;
    strip.setBrightness(brightness);
  }

  // Draws the countdown or count-up (if target is in the past) into the mask.
  void drawCountdown() {
    clearMask();
    int64_t diff = (int64_t)targetUnix - (int64_t)localTime; // >0 remaining, <0 passed

    // If the target is in the past, count up from the moment it was reached
    bool countingUp = (diff < 0);
    int64_t absDiff = abs(diff); // absolute difference for display

    if(countingUp && absDiff > 3600) {
      if(ModeChanged){
        revertMode();
        ModeChanged = false;
      }
      drawClock();
      return;
    }

    if(countingUp){
      Segment& selseg = strip.getSegment(0);
      if(!ModeChanged){
        SaveMode();
        setMode(FX_MODE_STATIC, 0xFF0000, 128, 128, 255);
        ModeChanged = true;
        IgnoreBlinking = false;
      }
      if(selseg.mode != FX_MODE_STATIC && ModeChanged && !IgnoreBlinking){
        IgnoreBlinking=true;
        revertMode();
      }

      if(millis() - lastBlink >= countdownBlinkInterval){
      BlinkToggle = !BlinkToggle;
      lastBlink = millis();
      }

      if(BlinkToggle && !IgnoreBlinking){
        strip.setBrightness(255);
      } else if (!BlinkToggle && !IgnoreBlinking){
        strip.setBrightness(0);
      }

    if (absDiff < 60) {
      setDigitChar(0, 'x');
      setDigitChar(1, 'x');
      setDigitInt(2, absDiff / 10);
      setDigitInt(3, absDiff % 10);
      setDigitChar(4, 'x');
      setDigitChar(5, 'x');
    }
    if (absDiff >= 60) {
      setDigitChar(0, 'x');
      setDigitChar(1, 'x');
      setDigitInt(2, absDiff / 600);
      setDigitInt(3, (absDiff / 60) % 10);
      setSeparator(2, sepsOn);
      setDigitInt(4, (absDiff % 60) / 10);
      setDigitInt(5, (absDiff % 60) % 10);
    }
  }
    

    if(!countingUp){
      // Cleanup from counting up mode
      if(ModeChanged){
        revertMode();
        ModeChanged = false;
      }

      // Split absolute difference into parts and totals
      remDays    = (uint32_t)(absDiff / 86400);
      remHours   = (uint8_t)((absDiff % 86400) / 3600);
      remMinutes = (uint8_t)((absDiff % 3600) / 60);
      remSeconds = (uint8_t)(absDiff % 60);

      fullHours   = (uint32_t)(absDiff / 3600);
      fullMinutes = (uint32_t)(absDiff / 60);
      fullSeconds = (uint32_t)absDiff;

      // > 99 days → show ddd d
      if (remDays > 99) {
        setDigitChar(0, ' ');
        setDigitInt(1, (remDays / 100) % 10);
        setDigitInt(2, (remDays / 10) % 10);
        setDigitInt(3, remDays % 10);
        setDigitChar(4, 'd');
        setDigitChar(5, ' ');
        return;
      }

      // ≤ 99 days → show dd:hh:mm
      if (remDays <=99 && fullHours > 99) {
        setDigitInt(0, remDays / 10);
        setDigitInt(1, remDays % 10);
        setSeparator(1, sepsOn);
        setDigitInt(2, remHours / 10);
        setDigitInt(3, remHours % 10);
        setSeparator(2, sepsOn);
        setDigitInt(4, remMinutes / 10);
        setDigitInt(5, remMinutes % 10);
        return;
      }

      // ≤ 99 hours → show hh:mm:ss
      if (fullHours <=99 && fullMinutes > 99) {
        setDigitInt(0, fullHours / 10);
        setDigitInt(1, fullHours % 10);
        setSeparator(1, sepsOn);
        setDigitInt(2, remMinutes / 10);
        setDigitInt(3, remMinutes % 10);
        setSeparator(2, sepsOn);
        setDigitInt(4, remSeconds / 10);
        setDigitInt(5, remSeconds % 10);
        return;
      }

      // ≤ 99 minutes → MM'SS:hh (hundredths)
      int hs = getHundredths(remSeconds, /*countDown*/ !countingUp);
    
      setDigitInt(0, fullMinutes / 10);
      setDigitInt(1, fullMinutes % 10);
      setSeparatorHalf(1, true, sepsOn);  // place an upper dot between minutes and seconds
      setDigitInt(2, remSeconds / 10);
      setDigitInt(3, remSeconds % 10);
      setSeparator(2, sepsOn);
      setDigitInt(4, hs / 10);
      setDigitInt(5, hs % 10);
    }
  }

  // Lights segments for a single digit based on a numeric value (0..9).
  void setDigitInt(uint8_t digitIndex, int8_t value) {
    if (digitIndex > 5) return;
    if (value < 0) return;

    uint16_t base = digitBase(digitIndex);
    uint8_t bits  = DIGIT_MASKS[(uint8_t)value];

    for (uint8_t physSeg = 0; physSeg < SEGS_PER_DIGIT; physSeg++) {
      uint8_t logSeg = PHYS_TO_LOG[physSeg];
      bool segOn = (bits >> logSeg) & 0x01;
      if (segOn) {
        uint16_t segStart = base + (uint16_t)physSeg * LEDS_PER_SEG;
        setRangeOn(segStart, LEDS_PER_SEG);
      }
    }
  }
    // Lights segments for a single digit using a letter/symbol mask.
    void setDigitChar(uint8_t digitIndex, char c) {
    if (digitIndex > 5) return;

    uint16_t base = digitBase(digitIndex);
    uint8_t bits  = LETTER_MASK(c);

    for (uint8_t physSeg = 0; physSeg < SEGS_PER_DIGIT; physSeg++) {
      uint8_t logSeg = PHYS_TO_LOG[physSeg];
      bool segOn = (bits >> logSeg) & 0x01;
      if (segOn) {
        uint16_t segStart = base + (uint16_t)physSeg * LEDS_PER_SEG;
        setRangeOn(segStart, LEDS_PER_SEG);
      }
    }
  }

  // Lights both dots of a separator when requested.
  void setSeparator(uint8_t which, bool on) {
    uint16_t base = (which == 1) ? sep1Base() : sep2Base();
    if (on) setRangeOn(base, SEP_LEDS);
  }

  // Lights a single half (upper/lower) of the separator; each half is SEP_LEDS/2.
  void setSeparatorHalf(uint8_t which, bool upper, bool on) {
    uint16_t base = (which == 1) ? sep1Base() : sep2Base();
    uint16_t halfLen = SEP_LEDS / 2;
    uint16_t start = base + (upper ? 0 : halfLen);
    if (on) setRangeOn(start, halfLen);
  }

  // Applies the mask to the WLED strip: 1 keeps pixel, 0 clears it to black.
  void applyMaskToStrip() {
    uint16_t stripLen = strip.getLengthTotal();
    uint16_t limit = (stripLen < (uint16_t)mask.size()) ? stripLen : (uint16_t)mask.size();
    for (uint16_t i = 0; i < limit; i++) {
      if (!mask[i]) strip.setPixelColor(i, 0);
    }
  }

  template <typename T>
  static T clampVal(T v, T lo, T hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
  }

  // Validates target fields, computes targetUnix, and optionally logs changes.
  void validateTarget(bool changed = false) {
    targetYear   = clampVal(targetYear, 1970, 2099);
    targetMonth  = clampVal<uint8_t>(targetMonth, 1, 12);
    targetDay    = clampVal<uint8_t>(targetDay, 1, 31);
    targetHour   = clampVal<uint8_t>(targetHour, 0, 23);
    targetMinute = clampVal<uint8_t>(targetMinute, 0, 59);

    tmElements_t tm;
    tm.Second = 0;
    tm.Minute = targetMinute;
    tm.Hour   = targetHour;
    tm.Day    = targetDay;
    tm.Month  = targetMonth;
    tm.Year   = CalendarYrToTm(targetYear);
    targetUnix = makeTime(tm);

    if (changed) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%04d-%02u-%02u-%02u-%02u",
               targetYear, targetMonth, targetDay, targetHour, targetMinute);
      Serial.printf("[7seg] Target changed: %s | unix=%lu\r\n", buf, (unsigned long)targetUnix);
    }
  }

public:
  // Prepare the mask buffer on boot.
  void setup() override {
    ensureMaskSize();
  }
  // No periodic work; rendering is driven by handleOverlayDraw().
  void loop() override {}

 // Main entry point from WLED to draw the overlay.
 void handleOverlayDraw() {
  if (!enabled) return;

  // If both views are enabled, alternate every "alternatingTime" seconds.
  if (showClock && showCountdown) {
    uint32_t period = (alternatingTime > 0) ? (uint32_t)alternatingTime : 10U;

    // Block index based on current time
    uint32_t block = (uint32_t)localTime / period;

    // Even blocks: clock; odd blocks: countdown
    if ((block & 1U) == 0U) drawClock();
    else                    drawCountdown();
  }
  else if (showClock) {
    drawClock();
  }
  else { // countdown only (or default)
    drawCountdown();
  }

  applyMaskToStrip();
}


  // Adds a compact UI block to the info screen (u-group).
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    // Top-level array for this usermod
    JsonArray infoArr = user.createNestedArray(F("7 Segment Counter"));

    // Enable/disable button
    String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({'7seg':{");
    uiDomString += F("'enabled':");
    uiDomString += enabled ? F("false}});\">") : F("true}});\">");
    uiDomString += F("<i class=\"icons");
    uiDomString += enabled ? F(" on\">") : F(" off\">");
    uiDomString += F("&#xe08f;</i></button>");
    infoArr.add(uiDomString);

    // State
    infoArr = user.createNestedArray(F("Status"));
    infoArr.add(enabled ? F("active") : F("disabled"));

    infoArr = user.createNestedArray(F("Clock Seperators"));
    if (SeperatorOn && SeperatorOff) infoArr.add(F("Blinking"));
    else if (SeperatorOn)          infoArr.add(F("Always On"));
    else if (SeperatorOff)         infoArr.add(F("Always Off"));
    else                           infoArr.add(F("Blinking"));

    // Modes
    infoArr = user.createNestedArray(F("Mode"));
    if (showClock && showCountdown) infoArr.add(F("Clock & Countdown"));
    else if (showClock)             infoArr.add(F("Clock Only"));
    else                            infoArr.add(F("Countdown Only"));

    // Target
    infoArr = user.createNestedArray(F("Target Date"));
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02u:%02u",
             targetYear, targetMonth, targetDay, targetHour, targetMinute);
    infoArr.add(buf);
    
    // Panel LEDs
    infoArr = user.createNestedArray(F("Total Panel LEDs"));
    infoArr.add(TOTAL_PANEL_LEDS);
    infoArr.add(F(" px"));
  }

  // JSON state/config: persist and apply overlay parameters via state/config.
  void addToJsonState(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) s = root.createNestedObject(F("7seg"));
    s[F("enabled")] = enabled;

    s[F("SeperatorOn")]  = SeperatorOn;
    s[F("SeperatorOff")] = SeperatorOff;

    s[F("targetYear")]   = targetYear;
    s[F("targetMonth")]  = targetMonth;
    s[F("targetDay")]    = targetDay;
    s[F("targetHour")]   = targetHour;
    s[F("targetMinute")] = targetMinute;

    s[F("showClock")]     = showClock;
    s[F("showCountdown")] = showCountdown;
    s[F("alternatingTime")] = alternatingTime;
  }

  void readFromJsonState(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) return;

    if (s.containsKey(F("enabled"))) enabled = s[F("enabled")].as<bool>();

    bool changed = false;
    if (s.containsKey(F("targetYear")))   { targetYear   = s[F("targetYear")].as<int>();      changed = true; }
    if (s.containsKey(F("targetMonth")))  { targetMonth  = s[F("targetMonth")].as<uint8_t>(); changed = true; }
    if (s.containsKey(F("targetDay")))    { targetDay    = s[F("targetDay")].as<uint8_t>();   changed = true; }
    if (s.containsKey(F("targetHour")))   { targetHour   = s[F("targetHour")].as<uint8_t>();  changed = true; }
    if (s.containsKey(F("targetMinute"))) { targetMinute = s[F("targetMinute")].as<uint8_t>();changed = true; }

    if (s.containsKey(F("showClock")))     showClock     = s[F("showClock")].as<bool>();
    if (s.containsKey(F("showCountdown"))) showCountdown = s[F("showCountdown")].as<bool>();
    if (s.containsKey(F("alternatingTime"))) alternatingTime = s[F("alternatingTime")].as<uint16_t>();
    if (s.containsKey(F("SeperatorOn")))  SeperatorOn  = s[F("SeperatorOn")].as<bool>();
    if (s.containsKey(F("SeperatorOff"))) SeperatorOff = s[F("SeperatorOff")].as<bool>();

    if (changed) validateTarget(true);
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(F("7seg"));
    top[F("enabled")] = enabled;

    top[F("targetYear")]   = targetYear;
    top[F("targetMonth")]  = targetMonth;
    top[F("targetDay")]    = targetDay;
    top[F("targetHour")]   = targetHour;
    top[F("targetMinute")] = targetMinute;

    top[F("showClock")]     = showClock;
    top[F("showCountdown")] = showCountdown;
    top[F("alternatingTime")] = alternatingTime; // seconds to alternate when both modes enabled
    top[F("SeperatorOn")]  = SeperatorOn;
    top[F("SeperatorOff")] = SeperatorOff;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[F("7seg")];
    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[F("enabled")], enabled, true);

    configComplete &= getJsonValue(top[F("targetYear")], targetYear, year(localTime));
    configComplete &= getJsonValue(top[F("targetMonth")], targetMonth, (uint8_t)1);
    configComplete &= getJsonValue(top[F("targetDay")], targetDay, (uint8_t)1);
    configComplete &= getJsonValue(top[F("targetHour")], targetHour, (uint8_t)0);
    configComplete &= getJsonValue(top[F("targetMinute")], targetMinute, (uint8_t)0);

    configComplete &= getJsonValue(top[F("showClock")], showClock, true);
    configComplete &= getJsonValue(top[F("showCountdown")], showCountdown, false);
    configComplete &= getJsonValue(top[F("alternatingTime")], alternatingTime, (uint16_t)10);
    configComplete &= getJsonValue(top[F("SeperatorOn")], SeperatorOn, true);
    configComplete &= getJsonValue(top[F("SeperatorOff")], SeperatorOff, true);

    validateTarget(true);
    return configComplete;
  }

  void onMqttConnect(bool sessionPresent) override {
    String topic = mqttDeviceTopic;
    topic += MQTT_Topic;
    mqtt->subscribe(topic.c_str(), 0);
  }

  bool onMqttMessage(char* topic, char* payload) {
    String topicStr = String(topic);

    if (!topicStr.startsWith(F("/7seg/"))) return false;

    String subTopic = topicStr.substring(strlen("/7seg/"));
    if (subTopic.indexOf(F("enabled")) >= 0) {
      String payloadStr = String(payload);
      enabled = (payloadStr == F("true") || payloadStr == F("1"));
      return true;
    }
    if (subTopic.indexOf(F("targetYear")) >= 0) {
      String payloadStr = String(payload);
      targetYear = payloadStr.toInt();
      return true;
    }
    if (subTopic.indexOf(F("targetMonth")) >= 0) {
      String payloadStr = String(payload);
      targetMonth = (uint8_t)payloadStr.toInt();
      return true;
    }
    if (subTopic.indexOf(F("targetDay")) >= 0) {
      String payloadStr = String(payload);
      targetDay = (uint8_t)payloadStr.toInt();
      return true;
    }
    if (subTopic.indexOf(F("targetHour")) >= 0) {
      String payloadStr = String(payload);
      targetHour = (uint8_t)payloadStr.toInt();
      return true;
    }
    if (subTopic.indexOf(F("targetMinute")) >= 0) {
      String payloadStr = String(payload);
      targetMinute = (uint8_t)payloadStr.toInt();
      return true;
    }
    if (subTopic.indexOf(F("showClock")) >= 0) {
      String payloadStr = String(payload);
      showClock = (payloadStr == F("true") || payloadStr == F("1"));
      return true;
    }
    if (subTopic.indexOf(F("showCountdown")) >= 0) {
      String payloadStr = String(payload);
      showCountdown = (payloadStr == F("true") || payloadStr == F("1"));
      return true;
    }
    if (subTopic.indexOf(F("alternatingTime")) >= 0) {
      String payloadStr = String(payload);
      alternatingTime = (uint16_t)payloadStr.toInt();
      return true;
    }
    if (subTopic.indexOf(F("SeperatorOn")) >= 0) {
      String payloadStr = String(payload);
      SeperatorOn = (payloadStr == F("true") || payloadStr == F("1"));
      return true;
    }
    if (subTopic.indexOf(F("SeperatorOff")) >= 0) {
      String payloadStr = String(payload);
      SeperatorOff = (payloadStr == F("true") || payloadStr == F("1"));
      return true;
    }

    return false;
  }

  // Unique usermod id (arbitrary, but stable).
  uint16_t getId() override { return 0x22B8; }
};

static SevenSegClockCountdown usermod;
REGISTER_USERMOD(usermod);
