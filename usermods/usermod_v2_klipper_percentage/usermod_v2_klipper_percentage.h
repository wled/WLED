#pragma once

#include "wled.h"

class klipper_percentage : public Usermod
{
private:
  unsigned long lastTime = 0;
  String ip = "192.168.25.207";
  WiFiClient wifiClient;
  char errorMessage[100] = "";
  int printPercent = 0;
  int direction = 0; // 0 for along the strip, 1 for reversed direction

  static const char _name[];
  static const char _enabled[];
  bool enabled = false;

  void httpGet(WiFiClient &client, char *errorMessage)
  {
    // https://arduinojson.org/v6/example/http-client/
    // is this the most compact way to do http get and put it in arduinojson object???
    // would like async response ... ???
    client.setTimeout(10000);
    if (!client.connect(ip.c_str(), 80))
    {
      strcat(errorMessage, PSTR("Connection failed"));
    }
    else
    {
      // Send HTTP request
      client.println(F("GET /printer/objects/query?virtual_sdcard=progress HTTP/1.0"));
      client.println("Host: " + ip);
      client.println(F("Connection: close"));
      if (client.println() == 0)
      {
        strcat(errorMessage, PSTR("Failed to send request"));
      }
      else
      {
        // Check HTTP status
        char status[32] = {0};
        client.readBytesUntil('\r', status, sizeof(status));
        if (strcmp(status, "HTTP/1.1 200 OK") != 0)
        {
          strcat(errorMessage, PSTR("Unexpected response: "));
          strcat(errorMessage, status);
        }
        else
        {
          // Skip HTTP headers
          char endOfHeaders[] = "\r\n\r\n";
          if (!client.find(endOfHeaders))
          {
            strcat(errorMessage, PSTR("Invalid response"));
          }
        }
      }
    }
  }

public:
  void setup()
  {
  }

  void connected()
  {
  }

  void loop()
  {
    if (enabled)
    {
      if (WLED_CONNECTED)
      {
        if (millis() - lastTime > 10000)
        {
          httpGet(wifiClient, errorMessage);
          if (strcmp(errorMessage, "") == 0)
          {
            PSRAMDynamicJsonDocument klipperDoc(4096); // in practice about 2673
            DeserializationError error = deserializeJson(klipperDoc, wifiClient);
            if (error)
            {
              strcat(errorMessage, PSTR("deserializeJson() failed: "));
              strcat(errorMessage, error.c_str());
            }
            printPercent = (int)(klipperDoc["result"]["status"]["virtual_sdcard"]["progress"].as<float>() * 100);

            DEBUG_PRINT("Percent: ");
            DEBUG_PRINTLN((int)(klipperDoc["result"]["status"]["virtual_sdcard"]["progress"].as<float>() * 100));
            DEBUG_PRINT("LEDs: ");
            DEBUG_PRINTLN(direction == 2 ? (strip.getLengthTotal() / 2) * printPercent / 100 : strip.getLengthTotal() * printPercent / 100);
          }
          else
          {
            DEBUG_PRINTLN(errorMessage);
            DEBUG_PRINTLN(ip);
          }
          lastTime = millis();
        }
      }
    }
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject("Klipper Printing Percentage");
    top["Enabled"] = enabled;
    top["Klipper IP"] = ip;
    top["Direction"] = direction;
  }

  bool readFromConfig(JsonObject &root)
  {
    // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
    // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

    JsonObject top = root["Klipper Printing Percentage"];

    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top["Klipper IP"], ip);
    configComplete &= getJsonValue(top["Enabled"], enabled);
    configComplete &= getJsonValue(top["Direction"], direction);
    return configComplete;
  }

  /*
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
   * Below it is shown how this could be used for e.g. a light sensor
   */
  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    JsonArray infoArr = user.createNestedArray(FPSTR(_name));
    String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
    uiDomString += FPSTR(_name);
    uiDomString += F(":{");
    uiDomString += FPSTR(_enabled);
    uiDomString += enabled ? F(":false}});\">") : F(":true}});\">");
    uiDomString += F("<i class=\"icons");
    uiDomString += enabled ? F(" on") : F(" off");
    uiDomString += F("\">&#xe08f;</i>");
    uiDomString += F("</button>");
    infoArr.add(uiDomString);
  }

  void addToJsonState(JsonObject &root)
  {
    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull())
    {
      usermod = root.createNestedObject(FPSTR(_name));
    }
    usermod["on"] = enabled;
  }
  void readFromJsonState(JsonObject &root)
  {
    JsonObject usermod = root[FPSTR(_name)];
    if (!usermod.isNull())
    {
      if (usermod[FPSTR(_enabled)].is<bool>())
      {
        enabled = usermod[FPSTR(_enabled)].as<bool>();
      }
    }
  }

  /*
   * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
   * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
   * Commonly used for custom clocks (Cronixie, 7 segment)
   */
  void handleOverlayDraw()
  {
    if (enabled)
    {
      if (direction == 0) // normal
      {
        for (int i = 0; i < strip.getLengthTotal() * printPercent / 100; i++)
        {
          strip.setPixelColor(i, strip.getSegment(0).colors[1]);
        }
      }
      else if (direction == 1) // reversed
      {
        for (int i = 0; i < strip.getLengthTotal() * printPercent / 100; i++)
        {
          strip.setPixelColor(strip.getLengthTotal() - i, strip.getSegment(0).colors[1]);
        }
      }
      else if (direction == 2) // center
      {
        for (int i = 0; i < (strip.getLengthTotal() / 2) * printPercent / 100; i++)
        {
          strip.setPixelColor((strip.getLengthTotal() / 2) + i, strip.getSegment(0).colors[1]);
          strip.setPixelColor((strip.getLengthTotal() / 2) - i, strip.getSegment(0).colors[1]);
        }
      }
      else
      {
        direction = 0;
      }
    }
  }

  /*
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   * This could be used in the future for the system to determine whether your usermod is installed.
   */
  uint16_t getId()
  {
    return USERMOD_ID_KLIPPER;
  }
};
const char klipper_percentage::_name[] PROGMEM = "Klipper_Percentage";
const char klipper_percentage::_enabled[] PROGMEM = "enabled";