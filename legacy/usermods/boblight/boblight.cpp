#include "wled.h"

/*
 * Usermod that implements BobLight "ambilight" protocol
 * 
 * See the accompanying README.md file for more info.
 */

#ifndef BOB_PORT
  #define BOB_PORT 19333       // Default boblightd port
#endif

class BobLightUsermod : public Usermod {
  typedef struct _LIGHT {
    char lightname[5];
    float hscan[2];
    float vscan[2];
  } light_t;

  private:
    unsigned long lastTime = 0;
    bool enabled  = false;
    bool initDone = false;

    light_t *lights = nullptr;
    uint16_t numLights = 0;  // 16 + 9 + 16 + 9
    uint16_t top, bottom, left, right;  // will be filled in readFromConfig()
    uint16_t pct;

    WiFiClient bobClient;
    WiFiServer *bob;
    uint16_t   bobPort = BOB_PORT;

    static const char _name[];
    static const char _enabled[];

    /*
    # boblight
    # Copyright (C) Bob  2009 
    #
    # makeboblight.sh created by Adam Boeglin <adamrb@gmail.com>
    #
    # boblight is free software: you can redistribute it and/or modify it
    # under the terms of the GNU General Public License as published by the
    # Free Software Foundation, either version 3 of the License, or
    # (at your option) any later version.
    # 
    # boblight is distributed in the hope that it will be useful, but
    # WITHOUT ANY WARRANTY; without even the implied warranty of
    # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    # See the GNU General Public License for more details.
    # 
    # You should have received a copy of the GNU General Public License along
    # with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

    // fills the lights[] array with position & depth of scan for each LED
    void fillBobLights(int bottom, int left, int top, int right, float pct_scan) {

      int lightcount = 0;
      int total = top+left+right+bottom;
      int bcount;

      if (total > strip.getLengthTotal()) {
        DEBUG_PRINTLN(F("BobLight: Too many lights."));
        return;
      }

      // start left part of bottom strip (clockwise direction, 1st half)
      if (bottom > 0) {
        bcount = 1;
        float brange = 100.0/bottom;
        float bcurrent = 50.0;
        if (bottom < top) {
          int diff = top - bottom;
          brange = 100.0/top;
          bcurrent -= (diff/2)*brange;
        }
        while (bcount <= bottom/2) {
          float btop = bcurrent - brange;
          String name = "b"+String(bcount);
          strncpy(lights[lightcount].lightname, name.c_str(), 4);
          lights[lightcount].hscan[0] = btop;
          lights[lightcount].hscan[1] = bcurrent;
          lights[lightcount].vscan[0] = 100 - pct_scan;
          lights[lightcount].vscan[1] = 100;
          lightcount+=1;
          bcurrent = btop;
          bcount+=1;
        }
      }

      // left side
      if (left > 0) {
        int lcount = 1;
        float lrange = 100.0/left;
        float lcurrent = 100.0;
        while (lcount <= left) {
          float ltop = lcurrent - lrange;
          String name = "l"+String(lcount);
          strncpy(lights[lightcount].lightname, name.c_str(), 4);
          lights[lightcount].hscan[0] = 0;
          lights[lightcount].hscan[1] = pct_scan;
          lights[lightcount].vscan[0] = ltop;
          lights[lightcount].vscan[1] = lcurrent;
          lightcount+=1;
          lcurrent = ltop;
          lcount+=1;
        }
      }

      // top side
      if (top > 0) {
        int tcount = 1;
        float trange = 100.0/top;
        float tcurrent = 0;
        while (tcount <= top) {
          float ttop = tcurrent + trange;
          String name = "t"+String(tcount);
          strncpy(lights[lightcount].lightname, name.c_str(), 4);
          lights[lightcount].hscan[0] = tcurrent;
          lights[lightcount].hscan[1] = ttop;
          lights[lightcount].vscan[0] = 0;
          lights[lightcount].vscan[1] = pct_scan;
          lightcount+=1;
          tcurrent = ttop;
          tcount+=1;
        }
      }

      // right side
      if (right > 0) {
        int rcount = 1;
        float rrange = 100.0/right;
        float rcurrent = 0;
        while (rcount <= right) {
          float rtop = rcurrent + rrange;
          String name = "r"+String(rcount);
          strncpy(lights[lightcount].lightname, name.c_str(), 4);
          lights[lightcount].hscan[0] = 100-pct_scan;
          lights[lightcount].hscan[1] = 100;
          lights[lightcount].vscan[0] = rcurrent;
          lights[lightcount].vscan[1] = rtop;
          lightcount+=1;
          rcurrent = rtop;
          rcount+=1;
        }
      }
      
      // right side of bottom strip (2nd half)
      if (bottom > 0) {
        float brange = 100.0/bottom;
        float bcurrent = 100;
        if (bottom < top) {
          brange = 100.0/top;
        }
        while (bcount <= bottom) {
          float btop = bcurrent - brange;
          String name = "b"+String(bcount);
          strncpy(lights[lightcount].lightname, name.c_str(), 4);
          lights[lightcount].hscan[0] = btop;
          lights[lightcount].hscan[1] = bcurrent;
          lights[lightcount].vscan[0] = 100 - pct_scan;
          lights[lightcount].vscan[1] = 100;
          lightcount+=1;
          bcurrent = btop;
          bcount+=1;
        }
      }

      numLights = lightcount;

      #if WLED_DEBUG
      DEBUG_PRINTLN(F("Fill light data: "));
      DEBUG_PRINTF_P(PSTR(" lights %d\n"), numLights);
      for (int i=0; i<numLights; i++) {
        DEBUG_PRINTF_P(PSTR(" light %s scan %2.1f %2.1f %2.1f %2.1f\n"), lights[i].lightname, lights[i].vscan[0], lights[i].vscan[1], lights[i].hscan[0], lights[i].hscan[1]);
      }
      #endif
    }

    void BobSync()  { yield(); } // allow other tasks, should also be used to force pixel redraw (not with WLED)
    void BobClear() { for (size_t i=0; i<numLights; i++) setRealtimePixel(i, 0, 0, 0, 0); }
    void pollBob();

  public:

    void setup() override {
      uint16_t totalLights = bottom + left + top + right;
      if ( totalLights > strip.getLengthTotal() ) {
        DEBUG_PRINTLN(F("BobLight: Too many lights."));
        DEBUG_PRINTF_P(PSTR("%d+%d+%d+%d>%d\n"), bottom, left, top, right, strip.getLengthTotal());
        totalLights = strip.getLengthTotal();
        top = bottom = (uint16_t) roundf((float)totalLights * 16.0f / 50.0f);
        left = right = (uint16_t) roundf((float)totalLights *  9.0f / 50.0f);
      }
      lights = new light_t[totalLights];
      if (lights) fillBobLights(bottom, left, top, right, float(pct)); // will fill numLights
      else        enable(false);
      initDone = true;
    }

    void connected() override {
      // we can only start server when WiFi is connected
      if (!bob) bob = new WiFiServer(bobPort, 1);
      bob->begin();
      bob->setNoDelay(true);
    }

    void loop() override {
      if (!enabled || strip.isUpdating()) return;
      if (millis() - lastTime > 10) {
        lastTime = millis();
        pollBob();
      }
    }

    void enable(bool en) { enabled = en; }
    
#ifndef WLED_DISABLE_MQTT
    /**
     * handling of MQTT message
     * topic only contains stripped topic (part after /wled/MAC)
     * topic should look like: /swipe with amessage of [up|down]
     */
    bool onMqttMessage(char* topic, char* payload) override {
      //if (strlen(topic) == 6 && strncmp_P(topic, PSTR("/subtopic"), 6) == 0) {
      //  String action = payload;
      //  if (action == "on") {
      //    enable(true);
      //    return true;
      //  } else if (action == "off") {
      //    enable(false);
      //    return true;
      //  }
      //}
      return false;
    }

    /**
     * subscribe to MQTT topic for controlling usermod
     */
    void onMqttConnect(bool sessionPresent) override {
      //char subuf[64];
      //if (mqttDeviceTopic[0] != 0) {
      //  strcpy(subuf, mqttDeviceTopic);
      //  strcat_P(subuf, PSTR("/subtopic"));
      //  mqtt->subscribe(subuf, 0);
      //}
    }
#endif

    void addToJsonInfo(JsonObject& root) override
    {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray infoArr = user.createNestedArray(FPSTR(_name));
      String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
      uiDomString += FPSTR(_name);
      uiDomString += F(":{");
      uiDomString += FPSTR(_enabled);
      uiDomString += enabled ? F(":false}});\">") : F(":true}});\">");
      uiDomString += F("<i class=\"icons ");
      uiDomString += enabled ? "on" : "off";
      uiDomString += F("\">&#xe08f;</i></button>");
      infoArr.add(uiDomString);
    }

    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root) override
    {
    }

    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root) override {
      if (!initDone) return;  // prevent crash on boot applyPreset()
      bool en = enabled;
      JsonObject um = root[FPSTR(_name)];
      if (!um.isNull()) {
        if (um[FPSTR(_enabled)].is<bool>()) {
          en = um[FPSTR(_enabled)].as<bool>();
        } else {
          String str = um[FPSTR(_enabled)]; // checkbox -> off or on
          en = (bool)(str!="off"); // off is guaranteed to be present
        }
        if (en != enabled && lights) {
          enable(en);
          if (!enabled && bob && bob->hasClient()) {
            if (bobClient) bobClient.stop();
            bobClient = bob->available();
            BobClear();
            exitRealtime();
          }
        }
      }
    }

    void appendConfigData() override {
      //oappend(F("dd=addDropdown('usermod','selectfield');"));
      //oappend(F("addOption(dd,'1st value',0);"));
      //oappend(F("addOption(dd,'2nd value',1);"));
      oappend(F("addInfo('BobLight:top',1,'LEDs');"));                // 0 is field type, 1 is actual field
      oappend(F("addInfo('BobLight:bottom',1,'LEDs');"));             // 0 is field type, 1 is actual field
      oappend(F("addInfo('BobLight:left',1,'LEDs');"));               // 0 is field type, 1 is actual field
      oappend(F("addInfo('BobLight:right',1,'LEDs');"));              // 0 is field type, 1 is actual field
      oappend(F("addInfo('BobLight:pct',1,'Depth of scan [%]');"));   // 0 is field type, 1 is actual field
    }

    void addToConfig(JsonObject& root) override {
      JsonObject umData = root.createNestedObject(FPSTR(_name));
      umData[FPSTR(_enabled)] = enabled;
      umData[  "port" ]       = bobPort;
      umData[F("top")]        = top;
      umData[F("bottom")]     = bottom;
      umData[F("left")]       = left;
      umData[F("right")]      = right;
      umData[F("pct")]        = pct;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject umData = root[FPSTR(_name)];
      bool configComplete = !umData.isNull();

      bool en = enabled;
      configComplete &= getJsonValue(umData[FPSTR(_enabled)], en);
      enable(en);

      configComplete &= getJsonValue(umData[  "port" ],   bobPort);
      configComplete &= getJsonValue(umData[F("bottom")], bottom,    16);
      configComplete &= getJsonValue(umData[F("top")],    top,       16);
      configComplete &= getJsonValue(umData[F("left")],   left,       9);
      configComplete &= getJsonValue(umData[F("right")],  right,      9);
      configComplete &= getJsonValue(umData[F("pct")],    pct,        5); // Depth of scan [%]
      pct = MIN(50,MAX(1,pct));

      uint16_t totalLights = bottom + left + top + right;
      if (initDone && numLights != totalLights) {
        if (lights) delete[] lights;
        setup();
      }
      return configComplete;
    }

    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw() override {
      //strip.setPixelColor(0, RGBW32(0,0,0,0)) // set the first pixel to black
    }

    uint16_t getId() override { return USERMOD_ID_BOBLIGHT; }

};

// strings to reduce flash memory usage (used more than twice)
const char BobLightUsermod::_name[]    PROGMEM = "BobLight";
const char BobLightUsermod::_enabled[] PROGMEM = "enabled";

// main boblight handling (definition here prevents inlining)
void BobLightUsermod::pollBob() {
  
  //check if there are any new clients
  if (bob && bob->hasClient()) {
    //find free/disconnected spot
    if (!bobClient || !bobClient.connected()) {
      if (bobClient) bobClient.stop();
      bobClient = bob->available();
      DEBUG_PRINTLN(F("Boblight: Client connected."));
    }
    //no free/disconnected spot so reject
    WiFiClient bobClientTmp = bob->available();
    bobClientTmp.stop();
    BobClear();
    exitRealtime();
  }
  
  //check clients for data
  if (bobClient && bobClient.connected()) {
    realtimeLock(realtimeTimeoutMs); // lock strip as we have a client connected

    //get data from the client
    while (bobClient.available()) {
      String input = bobClient.readStringUntil('\n');
      // DEBUG_PRINT(F("Client: ")); DEBUG_PRINTLN(input); // may be to stressful on Serial
      if (input.startsWith(F("hello"))) {
        DEBUG_PRINTLN(F("hello"));
        bobClient.print(F("hello\n"));
      } else if (input.startsWith(F("ping"))) {
        DEBUG_PRINTLN(F("ping 1"));
        bobClient.print(F("ping 1\n"));
      } else if (input.startsWith(F("get version"))) {
        DEBUG_PRINTLN(F("version 5"));
        bobClient.print(F("version 5\n"));
      } else if (input.startsWith(F("get lights"))) {
        char tmp[64];
        String answer = "";
        sprintf_P(tmp, PSTR("lights %d\n"), numLights);
        DEBUG_PRINT(tmp);
        answer.concat(tmp);
        for (int i=0; i<numLights; i++) {
          sprintf_P(tmp, PSTR("light %s scan %2.1f %2.1f %2.1f %2.1f\n"), lights[i].lightname, lights[i].vscan[0], lights[i].vscan[1], lights[i].hscan[0], lights[i].hscan[1]);
          DEBUG_PRINT(tmp);
          answer.concat(tmp);
        }
        bobClient.print(answer);
      } else if (input.startsWith(F("set priority"))) {
        DEBUG_PRINTLN(F("set priority not implemented"));
        // not implemented
      } else if (input.startsWith(F("set light "))) { // <id> <cmd in rgb, speed, interpolation> <value> ...
        input.remove(0,10);
        String tmp = input.substring(0,input.indexOf(' '));
        
        int light_id = -1;
        for (uint16_t i=0; i<numLights; i++) {
          if (strncmp(lights[i].lightname, tmp.c_str(), 4) == 0) {
            light_id = i;
            break;
          }
        }
        if (light_id == -1) return;

        input.remove(0,input.indexOf(' ')+1);
        if (input.startsWith(F("rgb "))) {
          input.remove(0,4);
          tmp = input.substring(0,input.indexOf(' '));
          uint8_t red = (uint8_t)(255.0f*tmp.toFloat());
          input.remove(0,input.indexOf(' ')+1);        // remove first float value
          tmp = input.substring(0,input.indexOf(' '));
          uint8_t green = (uint8_t)(255.0f*tmp.toFloat());
          input.remove(0,input.indexOf(' ')+1);        // remove second float value
          tmp = input.substring(0,input.indexOf(' '));
          uint8_t blue = (uint8_t)(255.0f*tmp.toFloat());

          //strip.setPixelColor(light_id, RGBW32(red, green, blue, 0));
          setRealtimePixel(light_id, red, green, blue, 0);
        } // currently no support for interpolation or speed, we just ignore this
      } else if (input.startsWith("sync")) {
        BobSync();
      } else {
        // Client sent gibberish
        DEBUG_PRINTLN(F("Client sent gibberish."));
        bobClient.stop();
        bobClient = bob->available();
        BobClear();
      }
    }
  }
}


static BobLightUsermod boblight;
REGISTER_USERMOD(boblight);