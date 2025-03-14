#include "wled.h"

/*
 * Sync to Philips hue lights
 */

#ifndef WLED_DISABLE_HUESYNC

void handleHue()
{
  if (hueReceived)
  {
    colorUpdated(CALL_MODE_HUE); hueReceived = false;
    if (hueStoreAllowed && hueNewKey)
    {
      serializeConfigSec(); //save api key
      hueStoreAllowed = false;
      hueNewKey = false;
    }
  }

  if (!WLED_CONNECTED || hueClient == nullptr || millis() - hueLastRequestSent < huePollIntervalMs) return;

  hueLastRequestSent = millis();
  if (huePollingEnabled)
  {
    reconnectHue();
  } else {
    hueClient->close();
    if (hueError == HUE_ERROR_ACTIVE) hueError = HUE_ERROR_INACTIVE;
  }
}

void reconnectHue()
{
  if (!WLED_CONNECTED || !huePollingEnabled) return;
  DEBUG_PRINTLN(F("Hue reconnect"));
  if (hueClient == nullptr) {
    hueClient = new AsyncClient();
    hueClient->onConnect(&onHueConnect, hueClient);
    hueClient->onData(&onHueData, hueClient);
    hueClient->onError(&onHueError, hueClient);
    hueAuthRequired = (strlen(hueApiKey)<20);
  }
  hueClient->connect(hueIP, 80);
}

void onHueError(void* arg, AsyncClient* client, int8_t error)
{
  DEBUG_PRINTLN(F("Hue err"));
  hueError = HUE_ERROR_TIMEOUT;
}

void onHueConnect(void* arg, AsyncClient* client)
{
  DEBUG_PRINTLN(F("Hue connect"));
  sendHuePoll();
}

void sendHuePoll()
{
  if (hueClient == nullptr || !hueClient->connected()) return;
  String req = "";
  if (hueAuthRequired)
  {
    req += F("POST /api HTTP/1.1\r\nHost: ");
    req += hueIP.toString();
    req += F("\r\nContent-Length: 25\r\n\r\n{\"devicetype\":\"wled#esp\"}");
  } else
  {
    req += F("GET /api/");
    req += hueApiKey;
    req += F("/lights/");
    req += String(huePollLightId);
    req += F(" HTTP/1.1\r\nHost: ");
    req += hueIP.toString();
    req += "\r\n\r\n";
  }
  hueClient->add(req.c_str(), req.length());
  hueClient->send();
  hueLastRequestSent = millis();
}

void onHueData(void* arg, AsyncClient* client, void *data, size_t len)
{
  if (!len) return;
  char* str = (char*)data;
  DEBUG_PRINTLN(hueApiKey);
  DEBUG_PRINTLN(str);
  //only get response body
  str = strstr(str,"\r\n\r\n");
  if (str == nullptr) return;
  str += 4;

  StaticJsonDocument<1024> root;
  if (str[0] == '[') //is JSON array
  {
    auto error = deserializeJson(root, str);
    if (error)
    {
      hueError = HUE_ERROR_JSON_PARSING; return;
    }

    int hueErrorCode = root[0][F("error")]["type"];
    if (hueErrorCode)//hue bridge returned error
    {
      hueError = hueErrorCode;
      switch (hueErrorCode)
      {
        case 1:   hueAuthRequired = true;    break; //Unauthorized user
        case 3:   huePollingEnabled = false; break; //Invalid light ID
        case 101: hueAuthRequired = true;    break; //link button not presset
      }
      return;
    }

    if (hueAuthRequired)
    {
      const char* apikey = root[0][F("success")][F("username")];
      if (apikey != nullptr && strlen(apikey) < sizeof(hueApiKey))
      {
        strlcpy(hueApiKey, apikey, sizeof(hueApiKey));
        hueAuthRequired = false;
        hueNewKey = true;
      }
    }
    return;
  }

  //else, assume it is JSON object, look for state and only parse that
  str = strstr(str,"state");
  if (str == nullptr) return;
  str = strstr(str,"{");

  auto error = deserializeJson(root, str);
  if (error)
  {
    hueError = HUE_ERROR_JSON_PARSING; return;
  }

  float hueX=0, hueY=0;
  uint16_t hueHue=0, hueCt=0;
  byte hueBri=0, hueSat=0, hueColormode=0;

  if (root["on"]) {
    if (root.containsKey("bri")) //Dimmable device
    {
      hueBri = root["bri"];
      hueBri++;
      const char* cm =root[F("colormode")];
      if (cm != nullptr) //Color device
      {
        if (strstr(cm,("ct")) != nullptr) //ct mode
        {
          hueCt = root["ct"];
          hueColormode = 3;
        } else if (strstr(cm,"xy") != nullptr) //xy mode
        {
          hueX = root["xy"][0]; // 0.5051
          hueY = root["xy"][1]; // 0.4151
          hueColormode = 1;
        } else //hs mode
        {
          hueHue = root["hue"];
          hueSat = root[F("sat")];
          hueColormode = 2;
        }
      }
    } else //On/Off device
    {
      hueBri = briLast;
    }
  } else
  {
    hueBri = 0;
  }

  hueError = HUE_ERROR_ACTIVE;

  //apply vals
  if (hueBri != hueBriLast)
  {
    if (hueApplyOnOff)
    {
      if (hueBri==0) {bri = 0;}
      else if (bri==0 && hueBri>0) bri = briLast;
    }
    if (hueApplyBri)
    {
      if (hueBri>0) bri = hueBri;
    }
    hueBriLast = hueBri;
  }
  if (hueApplyColor)
  {
    switch(hueColormode)
    {
      case 1: if (hueX != hueXLast || hueY != hueYLast) colorXYtoRGB(hueX,hueY,col); hueXLast = hueX; hueYLast = hueY; break;
      case 2: if (hueHue != hueHueLast || hueSat != hueSatLast) colorHStoRGB(hueHue,hueSat,col); hueHueLast = hueHue; hueSatLast = hueSat; break;
      case 3: if (hueCt != hueCtLast) colorCTtoRGB(hueCt,col); hueCtLast = hueCt; break;
    }
  }
  hueReceived = true;
}
#else
void handleHue(){}
void reconnectHue(){}
#endif
