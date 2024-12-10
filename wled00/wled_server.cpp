#include "wled.h"

#include "html_ui.h"
#ifdef WLED_ENABLE_SIMPLE_UI
  #include "html_simple.h"
#endif
#include "html_settings.h"
#include "html_other.h"
#ifdef WLED_ENABLE_PIXART
  #include "html_pixart.h"
#endif
#include "html_cpal.h"

/*
 * Integrated HTTP web server page declarations
 */

bool handleIfNoneMatchCacheHeader(AsyncWebServerRequest* request);
void setStaticContentCacheHeaders(AsyncWebServerResponse *response);

// define flash strings once (saves flash memory)
static const char s_redirecting[] PROGMEM = "Redirecting...";
static const char s_content_enc[] PROGMEM = "Content-Encoding";
static const char s_unlock_ota [] PROGMEM = "Please unlock OTA in security settings!";
static const char s_unlock_cfg [] PROGMEM = "Please unlock settings using PIN code!";

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

void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!correctPIN) {
    if (final) request->send(500, "text/plain", FPSTR(s_unlock_cfg));
    return;
  }
  if (!index) {
    String finalname = filename;
    if (finalname.charAt(0) != '/') {
      finalname = '/' + finalname; // prepend slash if missing
    }

    request->_tempFile = WLED_FS.open(finalname, "w");
    DEBUG_PRINT(F("Uploading "));
    DEBUG_PRINTLN(finalname);
    if (finalname.equals("/presets.json")) presetsModifiedTime = toki.second();
  }
  if (len) {
    request->_tempFile.write(data,len);
  }
  if (final) {
    request->_tempFile.close();
    USER_PRINT(F("File uploaded: "));  // WLEDMM
    USER_PRINTLN(filename);            // WLEDMM
    invalidateFileNameCache();         // WLEDMM
    if (filename.equalsIgnoreCase("/cfg.json") || filename.equalsIgnoreCase("cfg.json")) { // WLEDMM
      request->send(200, "text/plain", F("Configuration restore successful.\nRebooting..."));
      doReboot = true;
    } else {
      if (filename.equals("/presets.json") || filename.equals("presets.json")) {  // WLEDMM
        request->send(200, "text/plain", F("Presets File Uploaded!"));
      } else
        request->send(200, "text/plain", F("File Uploaded!"));
    }
    cacheInvalidate++;
  }
}

void createEditHandler(bool enable) {
  if (editHandler != nullptr) server.removeHandler(editHandler);
  if (enable) {
    #ifdef WLED_ENABLE_FS_EDITOR
      #ifdef ARDUINO_ARCH_ESP32
      editHandler = &server.addHandler(new SPIFFSEditor(WLED_FS));//http_username,http_password));
      #else
      editHandler = &server.addHandler(new SPIFFSEditor("","",WLED_FS));//http_username,http_password));
      #endif
    #else
      editHandler = &server.on("/edit", HTTP_GET, [](AsyncWebServerRequest *request){
        serveMessage(request, 501, "Not implemented", F("The FS editor is disabled in this build."), 254);
      });
    #endif
  } else {
    editHandler = &server.on("/edit", HTTP_ANY, [](AsyncWebServerRequest *request){
      serveMessage(request, 500, "Access Denied", FPSTR(s_unlock_cfg), 254);
    });
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
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_liveviewws, PAGE_liveviewws_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
    //request->send_P(200, "text/html", PAGE_liveviewws);
  });
  #ifndef WLED_DISABLE_2D
  server.on("/liveview2D", HTTP_GET, [](AsyncWebServerRequest *request){
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_liveviewws2D, PAGE_liveviewws2D_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
    //request->send_P(200, "text/html", PAGE_liveviewws);
  });
  #endif
#else
  server.on("/liveview", HTTP_GET, [](AsyncWebServerRequest *request){
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_liveview, PAGE_liveview_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
    //request->send_P(200, "text/html", PAGE_liveview);
  });
#endif

  //settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });

  // "/settings/settings.js&p=x" request also handled by serveSettings()

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", PAGE_settingsCss, PAGE_settingsCss_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
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

    if (!requestJSONBufferLock(14)) return;

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
      /*
      #ifdef WLED_DEBUG
        DEBUG_PRINTLN(F("Serialized HTTP"));
        serializeJson(root,Serial);
        DEBUG_PRINTLN();
      #endif
      */
      verboseResponse = deserializeState(root);
    } else {
      verboseResponse = deserializeConfig(root); //use verboseResponse to determine whether cfg change should be saved immediately
    }
    releaseJSONBufferLock();

    if (verboseResponse) {
      if (!isConfig) {
        lastInterfaceUpdate = millis(); // prevent WS update until cooldown
        interfaceUpdateCallMode = CALL_MODE_WS_SEND; // schedule WS update
        serveJson(request); return; //if JSON contains "v"
      } else {
        doSerializeConfig = true; //serializeConfig(); //Save new settings to FS
      }
      //#ifdef ARDUINO_ARCH_ESP32
      //DEBUG_PRINTF("%s min free stack %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL)); //WLEDMM
      //#endif
    }
    request->send(200, "application/json", F("{\"success\":true}"));
#if !defined(ARDUINO_ARCH_ESP32)
  }, JSON_BUFFER_SIZE);
#else
  });  // WLEDMM JSON_BUFFER_SIZE not needed on ESP32
#endif
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

  //WLEDMM and Athom
  server.on("/getflash", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", (String)ESP.getFlashChipSize());
    });

  server.on("/u", HTTP_GET, [](AsyncWebServerRequest *request){
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_usermod, PAGE_usermod_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
    //request->send_P(200, "text/html", PAGE_usermod);
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

#ifdef WLED_ENABLE_SIMPLE_UI
  server.on("/simple.htm", HTTP_GET, [](AsyncWebServerRequest *request){
    if (handleFileRead(request, "/simple.htm")) return;
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_simple, PAGE_simple_L);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
  });
#endif

  server.on("/iro.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", iroJs, iroJs_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
  });

  server.on("/rangetouch.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", rangetouchJs, rangetouchJs_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
  });

  //WLEDMM
  server.on("/peek.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", PAGE_peekJs, PAGE_peekJs_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
  });

  createEditHandler(correctPIN);

#ifndef WLED_DISABLE_OTA
  //init ota page
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if (otaLock) {
      serveMessage(request, 500, "Access Denied", FPSTR(s_unlock_ota), 254);
    } else
      serveSettings(request); // checks for "upd" in URL and handles PIN
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!correctPIN) {
      serveSettings(request, true); // handle PIN page POST request
      return;
    }
    if (Update.hasError() || otaLock) {
#ifdef ARDUINO_ARCH_ESP32
      if (Update.hasError()) {
        #if defined(WLEDMM_SAVE_FLASH) 
          // not requesting the error string reduces flash size by 200 bytes
          String updErr = Update.getError() == UPDATE_ERROR_ACTIVATE ? String("Could Not Activate The Firmware. (wrong board type?)") : String("Error ") + String(Update.getError());
        #else
          String updErr = Update.getError() == UPDATE_ERROR_ACTIVATE ? String("Could Not Activate The Firmware. (wrong board type?)") : String(Update.errorString());
        #endif
        serveMessage(request, 500, F("Update failed!"), updErr + String("<br>Please check your file and retry!"), 254);
      } else
        serveMessage(request, 500, F("Update failed!"), F("Please check your file and retry! (OTA may be locked)"), 254);
#else
      serveMessage(request, 500, F("Update failed!"), F("Please check your file and retry!"), 254);
#endif
    } else {
      serveMessage(request, 200, F("Update successful!"), F("Rebooting..."), 131);
      doReboot = true;
    }
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!correctPIN || otaLock) return;
    if(!index){
      DEBUG_PRINTLN(F("OTA Update Start"));
      WLED::instance().disableWatchdog();
      OTAisRunning = true; // WLEDMM flicker fixer
      strip.fill(BLACK);
      usermods.onUpdateBegin(true); // notify usermods that update is about to begin (some may require task de-init)
      lastEditTime = millis(); // make sure PIN does not lock during update
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
        usermods.onUpdateBegin(false); // notify usermods that update has failed (some may require task init)
        WLED::instance().enableWatchdog();
      }
      OTAisRunning = false; // WLEDMM flicker fixer
    }
  });
#else
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 501, "Not implemented", F("OTA updating is disabled in this build."), 254);
  });
#endif


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
    USER_PRINT("Client request"); //WLEDMM: want to see if client connects to wled
    #ifdef ARDUINO_ARCH_ESP32
    DEBUG_PRINTF("%s min free stack %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL)); //WLEDMM
    #endif
    if (captivePortal(request)) return;
    serveIndexOrWelcome(request);
    USER_PRINTLN(" done"); //WLEDMM: want to see if client connects to wled
  });

  #ifdef WLED_ENABLE_PIXART
  server.on("/pixart.htm", HTTP_GET, [](AsyncWebServerRequest *request){
    if (handleFileRead(request, "/pixart.htm")) return;
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_pixart, PAGE_pixart_L);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
  });
  #endif

  server.on("/cpal.htm", HTTP_GET, [](AsyncWebServerRequest *request){
    if (handleFileRead(request, "/cpal.htm")) return;
    if (handleIfNoneMatchCacheHeader(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_cpal, PAGE_cpal_L);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
  });

  #ifdef WLED_ENABLE_WEBSOCKETS
  server.addHandler(&ws);
  #endif

  //called when the url is not defined here, ajax-in; get-settings
  server.onNotFound([](AsyncWebServerRequest *request){
    DEBUG_PRINT("Not-Found HTTP call: ");
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
    AsyncWebServerResponse *response = request->beginResponse_P(404, "text/html", PAGE_404, PAGE_404_length);
    response->addHeader(FPSTR(s_content_enc),"gzip");
    setStaticContentCacheHeaders(response);
    request->send(response);
    //request->send_P(404, "text/html", PAGE_404);
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
  char tmp[12];
  // https://medium.com/@codebyamir/a-web-developers-guide-to-browser-caching-cc41f3b73e7c
  #ifndef WLED_DEBUG
  //this header name is misleading, "no-cache" will not disable cache,
  //it just revalidates on every load using the "If-None-Match" header with the last ETag value
  response->addHeader(F("Cache-Control"),"no-cache");
  #else
  response->addHeader(F("Cache-Control"),"no-store,max-age=0"); // prevent caching if debug build
  #endif
  sprintf_P(tmp, PSTR("%8d-%02x"), VERSION, cacheInvalidate);
  response->addHeader(F("ETag"), tmp);
}

void serveIndex(AsyncWebServerRequest* request)
{
  if (handleFileRead(request, "/index.htm")) return;

  if (handleIfNoneMatchCacheHeader(request)) return;

  AsyncWebServerResponse *response;
#ifdef WLED_ENABLE_SIMPLE_UI
  if (simplifiedUI)
    response = request->beginResponse_P(200, "text/html", PAGE_simple, PAGE_simple_L);
  else
#endif
    response = request->beginResponse_P(200, "text/html", PAGE_index, PAGE_index_L);

  response->addHeader(FPSTR(s_content_enc),"gzip");
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


#ifdef WLED_ENABLE_DMX
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
#endif


void serveSettingsJS(AsyncWebServerRequest* request)
{
  char buf[SETTINGS_STACK_BUF_SIZE+37] = { '\0' }; // WLEDMM ensure buffer is cleared initially
  buf[0] = 0;
  byte subPage = request->arg(F("p")).toInt();
  if (subPage > 10) {
    strcpy_P(buf, PSTR("alert('Settings for this request are not implemented.');"));
    request->send(501, "application/javascript", buf);
    return;
  }
  if (subPage > 0 && !correctPIN && strlen(settingsPIN)>0) {
    strcpy_P(buf, PSTR("alert('PIN incorrect.');"));
    request->send(403, "application/javascript", buf);
    return;
  }
  strcat_P(buf,PSTR("function GetV(){var d=document;"));
  getSettingsJS(request, subPage, buf+strlen(buf));  // this may overflow by 35bytes!!! WLEDMM add request
  strcat_P(buf,PSTR("}"));

  #ifdef ARDUINO_ARCH_ESP32
    DEBUG_PRINT(F("ServeSettingsJS: "));
    DEBUG_PRINTF("%s min free stack %d", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL)); //WLEDMM
    DEBUG_PRINTF(PSTR(" bytes.\t\tString buffer usage: %4d of %d bytes\n"), strlen(buf)+1, SETTINGS_STACK_BUF_SIZE+37);
  #endif
  
  AsyncWebServerResponse *response;
  response = request->beginResponse(200, "application/javascript", buf);
  response->addHeader(F("Cache-Control"),"no-store");
  response->addHeader(F("Expires"),"0");
  request->send(response);
}


void serveSettings(AsyncWebServerRequest* request, bool post)
{
  byte subPage = 0, originalSubPage = 0;
  const String& url = request->url();

  if (url.indexOf("sett") >= 0)
  {
    if      (url.indexOf(".js")  > 0) subPage = 254;
    else if (url.indexOf(".css") > 0) subPage = 253;
    else if (url.indexOf("wifi") > 0) subPage = 1;
    else if (url.indexOf("leds") > 0) subPage = 2;
    else if (url.indexOf("ui")   > 0) subPage = 3;
    else if (url.indexOf("sync") > 0) subPage = 4;
    else if (url.indexOf("time") > 0) subPage = 5;
    else if (url.indexOf("sec")  > 0) subPage = 6;
    else if (url.indexOf("dmx")  > 0) subPage = 7;
    else if (url.indexOf("um")   > 0) subPage = 8;
    else if (url.indexOf("2D")   > 0) subPage = 10;
    else if (url.indexOf("lock") > 0) subPage = 251;
  }
  else if (url.indexOf("/update") >= 0) subPage = 9; // update page, for PIN check
  //else if (url.indexOf("/edit")   >= 0) subPage = 10;
  else subPage = 255; // welcome page

  if (!correctPIN && strlen(settingsPIN) > 0 && (subPage > 0 && subPage < 11)) {
    originalSubPage = subPage;
    subPage = 252; // require PIN
  }

  // if OTA locked or too frequent PIN entry requests fail hard
  if ((subPage == 1 && wifiLock && otaLock) || (post && !correctPIN && millis()-lastEditTime < 3000))
  {
    serveMessage(request, 500, "Access Denied", FPSTR(s_unlock_ota), 254); return;
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
      case 6: strcpy_P(s, PSTR("Security")); if (doReboot) strcpy_P(s2, PSTR("Rebooting, please wait ~10 seconds...")); break;
      case 7: strcpy_P(s, PSTR("DMX")); break;
      case 8: strcpy_P(s, PSTR("Usermods")); break;
      case 10: strcpy_P(s, PSTR("2D")); break;
      case 252: strcpy_P(s, correctPIN ? PSTR("PIN accepted") : PSTR("PIN rejected")); break;
    }

    if (subPage == 252) {
      createEditHandler(correctPIN);
    } else
      strcat_P(s, PSTR(" settings saved."));

    if (subPage == 252 && correctPIN) {
      subPage = originalSubPage; // on correct PIN load settings page the user intended
    } else {
      if (!s2[0]) strcpy_P(s2, s_redirecting);

      serveMessage(request, 200, s, s2, (subPage == 1 || (subPage == 6 && doReboot)) ? 129 : (correctPIN ? 1 : 3));
      return;
    }
  }

  AsyncWebServerResponse *response;
  switch (subPage)
  {
    case 1:   response = request->beginResponse_P(200, "text/html", PAGE_settings_wifi, PAGE_settings_wifi_length); break;
    case 2:   response = request->beginResponse_P(200, "text/html", PAGE_settings_leds, PAGE_settings_leds_length); break;
    case 3:   response = request->beginResponse_P(200, "text/html", PAGE_settings_ui,   PAGE_settings_ui_length);   break;
    case 4:   response = request->beginResponse_P(200, "text/html", PAGE_settings_sync, PAGE_settings_sync_length); break;
    case 5:   response = request->beginResponse_P(200, "text/html", PAGE_settings_time, PAGE_settings_time_length); break;
    case 6:   response = request->beginResponse_P(200, "text/html", PAGE_settings_sec,  PAGE_settings_sec_length);  break;
#ifdef WLED_ENABLE_DMX
    case 7:   response = request->beginResponse_P(200, "text/html", PAGE_settings_dmx,  PAGE_settings_dmx_length);  break;
#endif
    case 8:   response = request->beginResponse_P(200, "text/html", PAGE_settings_um,   PAGE_settings_um_length);   break;
    case 9:   response = request->beginResponse_P(200, "text/html", PAGE_update,        PAGE_update_length);        break;
#ifndef WLED_DISABLE_2D
    case 10:  response = request->beginResponse_P(200, "text/html", PAGE_settings_2D,   PAGE_settings_2D_length);   break;
#endif
    case 251: {
      correctPIN = !strlen(settingsPIN); // lock if a pin is set
      createEditHandler(correctPIN);
      serveMessage(request, 200, strlen(settingsPIN) > 0 ? PSTR("Settings locked") : PSTR("No PIN set"), FPSTR(s_redirecting), 1);
      return;
    }
    case 252: response = request->beginResponse_P(200, "text/html", PAGE_settings_pin,  PAGE_settings_pin_length);  break;
    case 253: response = request->beginResponse_P(200, "text/css",  PAGE_settingsCss,   PAGE_settingsCss_length);   break;
    case 254: serveSettingsJS(request); return;
    case 255: response = request->beginResponse_P(200, "text/html", PAGE_welcome,       PAGE_welcome_length);       break;
    default:  response = request->beginResponse_P(200, "text/html", PAGE_settings,      PAGE_settings_length);      break;
  }
  response->addHeader(FPSTR(s_content_enc),"gzip");
  setStaticContentCacheHeaders(response);
  request->send(response);
}
