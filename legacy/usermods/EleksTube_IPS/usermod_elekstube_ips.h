#pragma once
#include "TFTs.h"
#include "wled.h"

//Large parts of the code are from https://github.com/SmittyHalibut/EleksTubeHAX

class ElekstubeIPSUsermod : public Usermod {
  private:
    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _tubeSeg[];
    static const char _digitOffset[];

    char cronixieDisplay[7] = "HHMMSS";

    TFTs tfts;
    void updateClockDisplay(TFTs::show_t show=TFTs::yes) {
      bool set[6] = {false}; 
      for (uint8_t i = 0; i<6; i++) {
        char c = cronixieDisplay[i];
        if (c >= '0' && c <= '9') {
          tfts.setDigit(5-i, c - '0', show); set[i] = true;
        } else if (c >= 'A' && c <= 'G') {
          tfts.setDigit(5-i, c - 'A' + 10, show); set[i] = true; //10.bmp to 16.bmp static display
        } else if (c == '-' || c == '_' || c == ' ') {
          tfts.setDigit(5-i, 255, show); set[i] = true; //blank
        } else {
          set[i] = false; //display HHMMSS time
        }
      }

      
      uint8_t hr = hour(localTime);
      uint8_t hrTens = hr/10;
      uint8_t mi = minute(localTime);
      uint8_t mittens = mi/10;
      uint8_t s = second(localTime);
      uint8_t sTens = s/10;
      if (!set[0]) tfts.setDigit(HOURS_TENS, hrTens, show);
      if (!set[1]) tfts.setDigit(HOURS_ONES, hr - hrTens*10, show);
      if (!set[2]) tfts.setDigit(MINUTES_TENS, mittens, show);
      if (!set[3]) tfts.setDigit(MINUTES_ONES, mi - mittens*10, show);
      if (!set[4]) tfts.setDigit(SECONDS_TENS, sTens, show);
      if (!set[5]) tfts.setDigit(SECONDS_ONES, s - sTens*10, show);
    }
    unsigned long lastTime = 0;
  public:

    uint8_t lastBri;
    uint32_t lastCols[6];
    TFTs::show_t fshow=TFTs::yes;

    void setup() {
      tfts.begin();
      tfts.fillScreen(TFT_BLACK);

      for (int8_t i = 5; i >= 0; i--) {
        tfts.setDigit(i, 255, TFTs::force); //turn all off
      }
    }

    void loop() {
      if (!toki.isTick()) return;
      updateLocalTime();

      Segment& seg1 = strip.getSegment(tfts.tubeSegment);
      if (seg1.isActive()) {
        bool update = false;
        if (seg1.opacity != lastBri) update = true;
        lastBri = seg1.opacity;
        for (uint8_t i = 0; i < 6; i++) {
          uint32_t c = strip.getPixelColor(seg1.start + i);
          if (c != lastCols[i]) update = true;
          lastCols[i] = c;
        }
        if (update) fshow=TFTs::force;
      } else if (lastCols[0] != 0) { // Segment 1 deleted
        fshow=TFTs::force;
        lastCols[0] = 0;
      }
      
      updateClockDisplay(fshow);
      fshow=TFTs::yes;
    }

    /**
     * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
     */
    void addToConfig(JsonObject &root) {
      // we add JSON object: {"EleksTubeIPS": {"tubeSegment": 1, "digitOffset": 0}}
      JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
      top[FPSTR(_tubeSeg)] = tfts.tubeSegment;
      top[FPSTR(_digitOffset)] = tfts.digitOffset;
      DEBUG_PRINTLN(F("EleksTube config saved."));
    }

    /**
     * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
     *
     * The function should return true if configuration was successfully loaded or false if there was no configuration.
     */
    bool readFromConfig(JsonObject &root) {
      // we look for JSON object: {"EleksTubeIPS": {"tubeSegment": 1, "digitOffset": 0}}
      DEBUG_PRINT(FPSTR(_name));

      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) {
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      tfts.tubeSegment = top[FPSTR(_tubeSeg)] | tfts.tubeSegment;
      uint8_t digitOffsetPrev = tfts.digitOffset;
      tfts.digitOffset = top[FPSTR(_digitOffset)] | tfts.digitOffset;
      if (tfts.digitOffset > 240) tfts.digitOffset = 240;
      if (tfts.digitOffset != digitOffsetPrev) fshow=TFTs::force;

      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      return !top[FPSTR(_digitOffset)].isNull();
    }

    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root)
    {
      root["nx"] = cronixieDisplay;
      root[FPSTR(_digitOffset)] = tfts.digitOffset;
    }


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root)
    {
      if (root["nx"].is<const char*>()) {
        strncpy(cronixieDisplay, root["nx"], 6);
      }

      uint8_t digitOffsetPrev = tfts.digitOffset;
      tfts.digitOffset = root[FPSTR(_digitOffset)] | tfts.digitOffset;
      if (tfts.digitOffset > 240) tfts.digitOffset = 240;
      if (tfts.digitOffset != digitOffsetPrev) fshow=TFTs::force;
    }

    uint16_t getId()
    {
      return USERMOD_ID_ELEKSTUBE_IPS;
    }
};

// strings to reduce flash memory usage (used more than twice)
const char ElekstubeIPSUsermod::_name[]         PROGMEM = "EleksTubeIPS";
const char ElekstubeIPSUsermod::_tubeSeg[]      PROGMEM = "tubeSegment";
const char ElekstubeIPSUsermod::_digitOffset[]  PROGMEM = "digitOffset";
