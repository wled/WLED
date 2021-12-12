#include "wled.h"

/*
 * Integrated HTTP web server page declarations
 */

//Is this an IP?
bool isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
  if (otaLock) {
    if (final) request->send(500, "text/plain", F("Please unlock OTA in security settings!"));
    return;
  }
  if(!index){
    request->_tempFile = WLED_FS.open(filename, "w");
    DEBUG_PRINT("Uploading ");
    DEBUG_PRINTLN(filename);
    if (filename == "/presets.json") presetsModifiedTime = toki.second();
  }
  if (len) {
    request->_tempFile.write(data,len);
  }
  if(final){
    request->_tempFile.close();
    request->send(200, "text/plain", F("File Uploaded!"));
  }
}

bool captivePortal(AsyncWebServerRequest *request)
{
  if (ON_STA_FILTER(request)) return false; //only serve captive in AP mode
  String hostH;
  if (!request->hasHeader("Host")) return false;
  hostH = request->getHeader("Host")->value();
  
  if (!isIp(hostH) && hostH.indexOf("wled.me") < 0 && hostH.indexOf(cmDNS) < 0) {
    DEBUG_PRINTLN("Captive portal");
    AsyncWebServerResponse *response = request->beginResponse(302);
    response->addHeader(F("Location"), F("http://4.3.2.1"));
    request->send(response);
    return true;
  }
  return false;
}

void initServer()
{
  //CORS compatiblity
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Methods"), "*");
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), "*");

 #ifdef WLED_ENABLE_WEBSOCKETS
    server.on("/liveview", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", PAGE_liveviewws);
    });
 #else
    server.on("/liveview", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", PAGE_liveview);
    });
  #endif
  
  //settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });
  
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!handleFileRead(request, "/favicon.ico"))
    {
      request->send_P(200, "image/x-icon", favicon, 156);
    }
  });
  
  server.on("/sliders", HTTP_GET, [](AsyncWebServerRequest *request){
    serveIndex(request);
  });
  
  server.on("/welcome", HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });
  
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 200,F("Rebooting now..."),F("Please wait ~10 seconds..."),129);
    doReboot = true;
  });
  
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
    serveSettings(request, true);
  });

  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request){
    serveJson(request);
  });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/json", [](AsyncWebServerRequest *request) {
    bool verboseResponse = false;
    bool isConfig = false;
    { //scope JsonDocument so it releases its buffer
      #ifdef WLED_USE_DYNAMIC_JSON
      DynamicJsonDocument doc(JSON_BUFFER_SIZE);
      #else
      if (!requestJSONBufferLock(14)) return;
      #endif

      DeserializationError error = deserializeJson(doc, (uint8_t*)(request->_tempObject));
      JsonObject root = doc.as<JsonObject>();
      if (error || root.isNull()) {
        releaseJSONBufferLock();
        request->send(400, "application/json", F("{\"error\":9}"));
        return;
      }
      const String& url = request->url();
      isConfig = url.indexOf("cfg") > -1;
      if (!isConfig) {
        #ifdef WLED_DEBUG
          DEBUG_PRINTLN(F("Serialized HTTP"));
          serializeJson(root,Serial);
          DEBUG_PRINTLN();
        #endif
        verboseResponse = deserializeState(root);
      } else {
        verboseResponse = deserializeConfig(root); //use verboseResponse to determine whether cfg change should be saved immediately
      }
      releaseJSONBufferLock();
    }
    if (verboseResponse) {
      if (!isConfig) {
        serveJson(request); return; //if JSON contains "v"
      } else {
        serializeConfig(); //Save new settings to FS
      }
    } 
    request->send(200, "application/json", F("{\"success\":true}"));
  });
  server.addHandler(handler);

  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", (String)VERSION);
    });
    
  server.on("/uptime", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", (String)millis());
    });
    
  server.on("/freeheap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", (String)ESP.getFreeHeap());
    });
  
  server.on("/u", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", PAGE_usermod);
    });
    
  //Deprecated, use of /json/state and presets recommended instead
  server.on("/url", HTTP_GET, [](AsyncWebServerRequest *request){
    URL_response(request);
    });
    
  server.on("/teapot", HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 418, F("418. I'm a teapot."), F("(Tangible Embedded Advanced Project Of Twinkling)"), 254);
    });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {},
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                      size_t len, bool final) {handleUpload(request, filename, index, data, len, final);}
  );

  //if OTA is allowed
  if (!otaLock){
    #ifdef WLED_ENABLE_FS_EDITOR
     #ifdef ARDUINO_ARCH_ESP32
      server.addHandler(new SPIFFSEditor(WLED_FS));//http_username,http_password));
     #else
      server.addHandler(new SPIFFSEditor("","",WLED_FS));//http_username,http_password));
     #endif
    #else
    server.on("/edit", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 501, "Not implemented", F("The FS editor is disabled in this build."), 254);
    });
    #endif
    //init ota page
    #ifndef WLED_DISABLE_OTA
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", PAGE_update);
    });
    
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
      if (Update.hasError())
      {
        serveMessage(request, 500, F("Update failed!"), F("Please check your file and retry!"), 254); return;
      }
      serveMessage(request, 200, F("Update successful!"), F("Rebooting..."), 131); 
      doReboot = true;
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if(!index){
        DEBUG_PRINTLN(F("OTA Update Start"));
        #ifdef ESP8266
        Update.runAsync(true);
        #endif
        Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
      }
      if(!Update.hasError()) Update.write(data, len);
      if(final){
        if(Update.end(true)){
          DEBUG_PRINTLN(F("Update Success"));
        } else {
          DEBUG_PRINTLN(F("Update Failed"));
        }
      }
    });
    
    #else
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 501, "Not implemented", F("OTA updating is disabled in this build."), 254);
    });
    #endif
  } else
  {
    server.on("/edit", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 500, "Access Denied", F("Please unlock OTA in security settings!"), 254);
    });
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 500, "Access Denied", F("Please unlock OTA in security settings!"), 254);
    });
  }


  #ifdef WLED_ENABLE_DMX
  server.on("/dmxmap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", PAGE_dmxmap     , dmxProcessor);
  });
  #else
  server.on("/dmxmap", HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 501, "Not implemented", F("DMX support is not enabled in this build."), 254);
  });
  #endif
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (captivePortal(request)) return;
    serveIndexOrWelcome(request);
  });

  #ifdef WLED_ENABLE_WEBSOCKETS
  server.addHandler(&ws);
  #endif
  
  //called when the url is not defined here, ajax-in; get-settings
  server.onNotFound([](AsyncWebServerRequest *request){
    DEBUG_PRINTLN("Not-Found HTTP call:");
    DEBUG_PRINTLN("URI: " + request->url());
    if (captivePortal(request)) return;

    //make API CORS compatible
    if (request->method() == HTTP_OPTIONS)
    {
      AsyncWebServerResponse *response = request->beginResponse(200);
      response->addHeader(F("Access-Control-Max-Age"), F("7200"));
      request->send(response);
      return;
    }
    
    if(handleSet(request, request->url())) return;
    #ifndef WLED_DISABLE_ALEXA
    if(espalexa.handleAlexaApiCall(request)) return;
    #endif
    if(handleFileRead(request, request->url())) return;
    request->send_P(404, "text/html", PAGE_404);
  });
}


void serveIndexOrWelcome(AsyncWebServerRequest *request)
{
  if (!showWelcomePage){
    serveIndex(request);
  } else {
    serveSettings(request);
  }
}

bool handleIfNoneMatchCacheHeader(AsyncWebServerRequest* request)
{
  AsyncWebHeader* header = request->getHeader("If-None-Match");
  if (header && header->value() == String(VERSION)) {
    request->send(304);
    return true;
  }
  return false;
}

void setStaticContentCacheHeaders(AsyncWebServerResponse *response)
{
  #ifndef WLED_DEBUG
  //this header name is misleading, "no-cache" will not disable cache,
  //it just revalidates on every load using the "If-None-Match" header with the last ETag value
  response->addHeader(F("Cache-Control"),"no-cache");
  #else
  response->addHeader(F("Cache-Control"),"no-store,max-age=0"); // prevent caching if debug build
  #endif
  response->addHeader(F("ETag"), String(VERSION));
}

void serveIndex(AsyncWebServerRequest* request)
{
  if (handleFileRead(request, "/index.htm")) return;

  if (handleIfNoneMatchCacheHeader(request)) return;

  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_index, PAGE_index_L);

  response->addHeader(F("Content-Encoding"),"gzip");
  setStaticContentCacheHeaders(response);
  request->send(response);
}


String msgProcessor(const String& var)
{
  if (var == "MSG") {
    String messageBody = messageHead;
    messageBody += F("</h2>");
    messageBody += messageSub;
    uint32_t optt = optionType;

    if (optt < 60) //redirect to settings after optionType seconds
    {
      messageBody += F("<script>setTimeout(RS,");
      messageBody +=String(optt*1000);
      messageBody += F(")</script>");
    } else if (optt < 120) //redirect back after optionType-60 seconds, unused
    {
      //messageBody += "<script>setTimeout(B," + String((optt-60)*1000) + ")</script>";
    } else if (optt < 180) //reload parent after optionType-120 seconds
    {
      messageBody += F("<script>setTimeout(RP,");
      messageBody += String((optt-120)*1000);
      messageBody += F(")</script>");
    } else if (optt == 253)
    {
      messageBody += F("<br><br><form action=/settings><button class=\"bt\" type=submit>Back</button></form>"); //button to settings
    } else if (optt == 254)
    {
      messageBody += F("<br><br><button type=\"button\" class=\"bt\" onclick=\"B()\">Back</button>");
    }
    return messageBody;
  }
  return String();
}


void serveMessage(AsyncWebServerRequest* request, uint16_t code, const String& headl, const String& subl, byte optionT)
{
  messageHead = headl;
  messageSub = subl;
  optionType = optionT;
  
  request->send_P(code, "text/html", PAGE_msg, msgProcessor);
}


String settingsProcessor(const String& var)
{
  if (var == "CSS") {
    char buf[SETTINGS_STACK_BUF_SIZE];
    buf[0] = 0;
    getSettingsJS(optionType, buf);
    //Serial.println(uxTaskGetStackHighWaterMark(NULL));
    return String(buf);
  }
  
  #ifdef WLED_ENABLE_DMX

  if (var == "DMXMENU") {
    return String(F("<form action=/settings/dmx><button type=submit>DMX Output</button></form>"));
  }
  
  #endif
  if (var == "SCSS") return String(FPSTR(PAGE_settingsCss));
  return String();
}

String dmxProcessor(const String& var)
{
  String mapJS;
  #ifdef WLED_ENABLE_DMX
    if (var == "DMXVARS") {
      mapJS += "\nCN=" + String(DMXChannels) + ";\n";
      mapJS += "CS=" + String(DMXStart) + ";\n";
      mapJS += "CG=" + String(DMXGap) + ";\n";
      mapJS += "LC=" + String(strip.getLengthTotal()) + ";\n";
      mapJS += "var CH=[";
      for (int i=0;i<15;i++) {
        mapJS += String(DMXFixtureMap[i]) + ",";
      }
      mapJS += "0];";
    }
  #endif
  
  return mapJS;
}


void serveSettings(AsyncWebServerRequest* request, bool post)
{
  byte subPage = 0;
  const String& url = request->url();
  if (url.indexOf("sett") >= 0) 
  {
    if      (url.indexOf("wifi") > 0) subPage = 1;
    else if (url.indexOf("leds") > 0) subPage = 2;
    else if (url.indexOf("ui")   > 0) subPage = 3;
    else if (url.indexOf("sync") > 0) subPage = 4;
    else if (url.indexOf("time") > 0) subPage = 5;
    else if (url.indexOf("sec")  > 0) subPage = 6;
    #ifdef WLED_ENABLE_DMX // include only if DMX is enabled
    else if (url.indexOf("dmx")  > 0) subPage = 7;
    #endif
    else if (url.indexOf("um")  > 0) subPage = 8;
  } else subPage = 255; //welcome page

  if (subPage == 1 && wifiLock && otaLock)
  {
    serveMessage(request, 500, "Access Denied", F("Please unlock OTA in security settings!"), 254); return;
  }

  if (post) { //settings/set POST request, saving
    if (subPage != 1 || !(wifiLock && otaLock)) handleSettingsSet(request, subPage);

    char s[32];
    char s2[45] = "";

    switch (subPage) {
      case 1: strcpy_P(s, PSTR("WiFi")); strcpy_P(s2, PSTR("Please connect to the new IP (if changed)")); forceReconnect = true; break;
      case 2: strcpy_P(s, PSTR("LED")); break;
      case 3: strcpy_P(s, PSTR("UI")); break;
      case 4: strcpy_P(s, PSTR("Sync")); break;
      case 5: strcpy_P(s, PSTR("Time")); break;
      case 6: strcpy_P(s, PSTR("Security")); strcpy_P(s2, PSTR("Rebooting, please wait ~10 seconds...")); break;
      case 7: strcpy_P(s, PSTR("DMX")); break;
      case 8: strcpy_P(s, PSTR("Usermods")); break;
    }

    strcat_P(s, PSTR(" settings saved."));
    if (!s2[0]) strcpy_P(s2, PSTR("Redirecting..."));

    if (!doReboot) serveMessage(request, 200, s, s2, (subPage == 1 || subPage == 6) ? 129 : 1);
    if (subPage == 6) doReboot = true;

    return;
  }
  
  #ifdef WLED_DISABLE_MOBILE_UI //disable welcome page if not enough storage
   if (subPage == 255) {serveIndex(request); return;}
  #endif

  optionType = subPage;
  
  switch (subPage)
  {
    case 1:   request->send_P(200, "text/html", PAGE_settings_wifi, settingsProcessor); break;
    case 2:   request->send_P(200, "text/html", PAGE_settings_leds, settingsProcessor); break;
    case 3:   request->send_P(200, "text/html", PAGE_settings_ui  , settingsProcessor); break;
    case 4:   request->send_P(200, "text/html", PAGE_settings_sync, settingsProcessor); break;
    case 5:   request->send_P(200, "text/html", PAGE_settings_time, settingsProcessor); break;
    case 6:   request->send_P(200, "text/html", PAGE_settings_sec , settingsProcessor); break;
    case 7:   request->send_P(200, "text/html", PAGE_settings_dmx , settingsProcessor); break;
    case 8:   request->send_P(200, "text/html", PAGE_settings_um  , settingsProcessor); break;
    case 255: request->send_P(200, "text/html", PAGE_welcome); break;
    default:  request->send_P(200, "text/html", PAGE_settings     , settingsProcessor); 
  }
}
