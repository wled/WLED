#include "wled.h"


// WLED Usermod PRO+ v0.15.0
// + Detekcja ruchu i wzorca
// + Zdalne sterowanie przez Wi-Fi (MQTT i HTTP)


class Usermod : public Usermod {
  private:
    // Presety
    uint8_t presetUp   = 10;
    uint8_t presetDown = 11;
    // Tryby
    bool continuousMode  = false;
    bool secondInterrupt = false;
    enum LedMode { NORMAL, SPLIT, MIRROR } ledMode = NORMAL;
    bool dayNightMode    = false;
    // Wzorzec
    static const uint8_t patternLength = 4;
    uint32_t triggerTimes[patternLength];
    uint8_t triggerCount = 0;
    uint32_t patternWindow = 1000; // ms
    // Blokada
    uint32_t lockTime = 0, lastTrigger = 0;
    // Piny
    const uint8_t pinSensorUp   = 2;
    const uint8_t pinSensorDown = 16;
    const uint8_t pinPir        = 17;
    const uint8_t pinStatusLED  = LED_BUILTIN;
    // Zdalnie
    bool mqttEnabled    = true;
    String mqttTopicCmd = "proplus/cmd";
    bool httpEnabled    = true;
    // UI IDs
    uint16_t uiPresetUp, uiPresetDown;
    uint16_t uiContinuous, uiSecondInterrupt;
    uint16_t uiLedMode, uiLockTime;
    uint16_t uiTestUp, uiTestDown;
    uint16_t uiStatusLED, uiDayNight;
    uint16_t uiPatternWindow, uiEnableMotion, uiMqtt, uiHttp;
  public:
    String getName() { return "PRO+ UsrMod"; }
    uint16_t getId()   { return USERMOD_ID + 1; }
    void setup() {
      pinMode(pinSensorUp, INPUT_PULLUP);
      pinMode(pinSensorDown, INPUT_PULLUP);
      pinMode(pinPir, INPUT);
      pinMode(pinStatusLED, OUTPUT);
      memset(triggerTimes, 0, sizeof(triggerTimes));
      if (mqttEnabled && WLED_MQTT_CONNECTED) mqtt->client->subscribe(mqttTopicCmd.c_str());
    }
    void loop() {
      uint32_t now = millis();
      if (nmUI.getCheckBoxValue(uiEnableMotion) && digitalRead(pinPir)==HIGH) {
        triggerEffectUp(); delay(500);
      }
      if (!isLocked(now)) {
        if (digitalRead(pinSensorUp)==LOW)   registerTrigger(now, true);
        if (digitalRead(pinSensorDown)==LOW) registerTrigger(now, false);
      }
      if (mqttEnabled) mqtt->loop();
    }
    void onMqttMessage(const JsonObject &doc, char *topic) {
      String t = String(topic);
      if (t.endsWith(mqttTopicCmd)) {
        String act = doc["action"].as<String>();
        if (act=="up")    triggerEffectUp();
        if (act=="down")  triggerEffectDown();
        if (act=="pattern") runPattern();
      }
    }
    bool onHttpRequest(AsyncWebServerRequest *req) {
      if (!httpEnabled) return false;
      if (req->url()=="/proplus/cmd" && req->hasParam("action")) {
        String a = req->getParam("action")->value();
        if (a=="up")    triggerEffectUp();
        if (a=="down")  triggerEffectDown();
        if (a=="pattern") runPattern();
        req->send(200,"text/plain","OK");
        return true;
      }
      return false;
    }
    void addUserInterface() {
      nmUI.addSection("PRO+ Ustawienia");
      uiPresetUp       = nmUI.addSlider("Preset Góra", String(presetUp).c_str(),1,30);
      uiPresetDown     = nmUI.addSlider("Preset Dół", String(presetDown).c_str(),1,30);
      uiContinuous     = nmUI.addCheckBox("Tryb ciągły");
      uiSecondInterrupt= nmUI.addCheckBox("Drugi czujnik przerwa");
      uiLedMode        = nmUI.addSelect("Tryb LED1/LED2",{ "Normalny","Podział","Lustro" });
      uiLockTime       = nmUI.addNumberInput("Czas blokady (ms)", lockTime);
      uiEnableMotion   = nmUI.addCheckBox("Detekcja ruchu (PIR)");
      uiPatternWindow  = nmUI.addNumberInput("Okno wzorca (ms)", patternWindow);
      uiMqtt           = nmUI.addCheckBox("MQTT remote");
      uiHttp           = nmUI.addCheckBox("HTTP remote");
      uiTestUp         = nmUI.addButton("Test Góra");
      uiTestDown       = nmUI.addButton("Test Dół");
      uiStatusLED      = nmUI.addCheckBox("Status diody ESP");
      uiDayNight       = nmUI.addCheckBox("Tryb noc/dzień");
    }
    void onButtonPressed(uint16_t id) {
      if (id==uiTestUp)   triggerEffectUp();
      if (id==uiTestDown) triggerEffectDown();
    }
    void onSettingsSaved() {
      presetUp        = nmUI.getSliderValue(uiPresetUp);
      presetDown      = nmUI.getSliderValue(uiPresetDown);
      continuousMode  = nmUI.getCheckBoxValue(uiContinuous);
      secondInterrupt = nmUI.getCheckBoxValue(uiSecondInterrupt);
      ledMode         = (LedMode)nmUI.getSelectValue(uiLedMode);
      lockTime        = nmUI.getNumberInputValue(uiLockTime);
      dayNightMode    = nmUI.getCheckBoxValue(uiDayNight);
      patternWindow   = nmUI.getNumberInputValue(uiPatternWindow);
      mqttEnabled     = nmUI.getCheckBoxValue(uiMqtt);
      httpEnabled     = nmUI.getCheckBoxValue(uiHttp);
      saveConfiguration();
    }
  private:
    bool isLocked(uint32_t t) { return (t - lastTrigger) < lockTime; }
    void registerTrigger(uint32_t t, bool isUp) {
      triggerTimes[triggerCount++ % patternLength] = t;
      lastTrigger = t;
      if (triggerCount>=patternLength && (t - triggerTimes[triggerCount % patternLength]) < patternWindow) {
        runPattern();
      } else {
        if (isUp) triggerEffectUp(); else triggerEffectDown();
      }
    }
    void triggerEffectUp()   { applyPreset(presetUp); }
    void triggerEffectDown() { applyPreset(presetDown); }
    void runPattern() {
      for (uint8_t i=0;i<3;i++){
        setAllHue((state.hue + i*85) % 256);
        stateUpdated(CALL_MODE_USER);
        delay(200);
      }
      stateUpdated(CALL_MODE_USER);
    }
    void applyPreset(uint8_t p) {
      effectCurrent = p; stateUpdated(CALL_MODE_USER);
      if (ledMode==SPLIT||ledMode==MIRROR) applySplitMirror();
      if (nmUI.getCheckBoxValue(uiStatusLED)) digitalWrite(pinStatusLED, HIGH);
      sendEvents(p);
    }
    void applySplitMirror() { /* … */ }
    void sendEvents(uint8_t p) {
      if (mqttEnabled && WLED_MQTT_CONNECTED) {
        StaticJsonDocument<64> d;
        d["action"]="preset"; d["preset"]=p;
        char b[64]; size_t n=serializeJson(d,b);
        mqtt->publish(mqttTopicCmd.c_str(),b,n,true);
      }
    }
};


Usermod usermod;
