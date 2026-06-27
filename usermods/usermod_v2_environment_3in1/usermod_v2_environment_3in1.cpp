#include "wled.h"
#include <Wire.h>

/*
 * Environment sensor usermod for the "3-in-1" I2C module
 * (HTU21D temp/humidity + BMP180 pressure/temp + BH1750 light).
 *
 * Scope (v1): TEMPERATURE only. Reads the HTU21D (primary); if it is not present it
 * falls back to the BMP180. Humidity / pressure / lux can be added later.
 *
 * It feeds the temperature to usermod_v2_word_clock_16x16 (for its WARM/COOL/HOT/COLD
 * words) WITHOUT a hard dependency, via a weak-symbol bridge: if the word clock usermod
 * is compiled in, wc16_setLiveTempC() exists and we call it; otherwise the symbol is
 * null and we skip. This keeps both usermods independent and needs no core edits.
 *
 * I2C uses WLED's configured SDA/SCL pins (Wire is initialised by WLED at boot).
 */

// Weak bridge into the word clock usermod (see usermod_v2_word_clock_16x16.cpp).
extern "C" void wc16_setLiveTempC(float) __attribute__((weak));

class EnvSensor3in1Usermod : public Usermod {
  private:
    bool enabled       = true;
    bool feedWordClock = true;
    uint16_t readSeconds = 30;
    uint8_t  htuAddr   = 0x40; // HTU21D
    uint8_t  bmpAddr   = 0x77; // BMP180

    bool  initDone   = false;
    bool  htuPresent = false;
    bool  bmpPresent = false;
    uint8_t phase    = 0;      // HTU21D read state: 0 idle, 1 awaiting conversion
    unsigned long lastRead    = 0;
    unsigned long triggerTime = 0;

    bool  haveTemp = false;
    float tempC    = 0.0f;

    // BMP180 calibration coefficients needed for temperature.
    int16_t  ac5 = 0, ac6 = 0, mc = 0, md = 0;

    static const char _name[];
    static const char _enabled[];
    static const char _feed[];
    static const char _interval[];

    bool i2cReady() const { return i2c_sda >= 0 && i2c_scl >= 0; }

    bool probe(uint8_t addr) {
      Wire.beginTransmission(addr);
      return Wire.endTransmission() == 0;
    }

    // ---- BMP180 (temperature only) -------------------------------------------
    bool bmpReadCal16(uint8_t reg, int16_t &out) {
      Wire.beginTransmission(bmpAddr);
      Wire.write(reg);
      if (Wire.endTransmission() != 0) return false;
      if (Wire.requestFrom(bmpAddr, (uint8_t)2) != 2) return false;
      out = (int16_t)((Wire.read() << 8) | Wire.read());
      return true;
    }

    bool bmpInit() {
      // chip id register 0xD0 must read 0x55
      Wire.beginTransmission(bmpAddr);
      Wire.write(0xD0);
      if (Wire.endTransmission() != 0) return false;
      if (Wire.requestFrom(bmpAddr, (uint8_t)1) != 1) return false;
      if (Wire.read() != 0x55) return false;
      return bmpReadCal16(0xB4, ac5) && bmpReadCal16(0xB6, ac6)
          && bmpReadCal16(0xBC, mc)  && bmpReadCal16(0xBE, md);
    }

    bool bmpReadTemp(float &out) {
      Wire.beginTransmission(bmpAddr);   // start temperature conversion
      Wire.write(0xF4); Wire.write(0x2E);
      if (Wire.endTransmission() != 0) return false;
      delay(5);                          // datasheet max 4.5 ms (well under WLED's 10 ms limit)
      Wire.beginTransmission(bmpAddr);
      Wire.write(0xF6);
      if (Wire.endTransmission() != 0) return false;
      if (Wire.requestFrom(bmpAddr, (uint8_t)2) != 2) return false;
      int32_t ut = (int32_t)((Wire.read() << 8) | Wire.read());
      int32_t x1 = ((ut - ac6) * (int32_t)ac5) >> 15;
      int32_t x2 = ((int32_t)mc << 11) / (x1 + md);
      int32_t b5 = x1 + x2;
      out = ((b5 + 8) >> 4) / 10.0f;
      return true;
    }

    void publish(float c) {
      tempC = c; haveTemp = true;
      if (feedWordClock && wc16_setLiveTempC) wc16_setLiveTempC(c);
    }

  public:
    void setup() override {
      if (!enabled || !i2cReady()) { initDone = true; return; }
      htuPresent = probe(htuAddr);
      if (!htuPresent) bmpPresent = bmpInit();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !i2cReady() || (!htuPresent && !bmpPresent)) return;
      const unsigned long now = millis();
      const unsigned long interval = (unsigned long)readSeconds * 1000UL;

      if (htuPresent) {
        if (phase == 0) {
          if (now - lastRead < interval) return;
          Wire.beginTransmission(htuAddr);
          Wire.write(0xF3);              // trigger temperature, no-hold master
          if (Wire.endTransmission() == 0) { phase = 1; triggerTime = now; }
          else { lastRead = now; }       // retry next interval
        } else {
          if (now - triggerTime < 60) return; // ~50 ms conversion time
          if (Wire.requestFrom(htuAddr, (uint8_t)3) == 3) {
            uint16_t raw = (Wire.read() << 8) | Wire.read();
            Wire.read();                 // checksum byte (ignored)
            raw &= ~0x0003;              // clear status bits
            publish(-46.85f + 175.72f * (raw / 65536.0f));
          }
          phase = 0; lastRead = now;
        }
      } else { // BMP180 fallback
        if (now - lastRead < interval) return;
        float c;
        if (bmpReadTemp(c)) publish(c);
        lastRead = now;
      }
    }

    void addToJsonInfo(JsonObject &root) override {
      if (!enabled) return;
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));
      JsonArray arr = user.createNestedArray(F("Environment temperature"));
      if (!htuPresent && !bmpPresent) { arr.add(F("no sensor")); return; }
      if (!haveTemp) { arr.add(F("--")); return; }
      char buf[16];
      snprintf(buf, sizeof(buf), "%.1f", tempC);
      arr.add(buf);
      arr.add(F(" °C"));
    }

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]  = enabled;
      top[FPSTR(_feed)]     = feedWordClock;
      top[FPSTR(_interval)] = readSeconds;
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)],  enabled);
      configComplete &= getJsonValue(top[FPSTR(_feed)],     feedWordClock);
      configComplete &= getJsonValue(top[FPSTR(_interval)], readSeconds);
      if (readSeconds < 2) readSeconds = 2;
      return configComplete;
    }

    void appendConfigData() {
      oappend(F("addInfo('Env3in1:feedWordClock', 1, 'push temperature to Word Clock 16x16');"));
      oappend(F("addInfo('Env3in1:readSeconds', 1, 'seconds between readings');"));
    }

    // No getId() override (USERMOD_ID_UNSPECIFIED) — keeps changes out of wled00/const.h.
};

const char EnvSensor3in1Usermod::_name[]     PROGMEM = "Env3in1";
const char EnvSensor3in1Usermod::_enabled[]  PROGMEM = "enabled";
const char EnvSensor3in1Usermod::_feed[]     PROGMEM = "feedWordClock";
const char EnvSensor3in1Usermod::_interval[] PROGMEM = "readSeconds";

static EnvSensor3in1Usermod usermod_v2_environment_3in1;
REGISTER_USERMOD(usermod_v2_environment_3in1);
