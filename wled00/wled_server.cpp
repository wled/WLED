#include "wled.h"

#ifndef WLED_DISABLE_OTA
  #include "ota_update.h"  
#endif
#include "html_ui.h"
#include "html_settings.h"
#include "html_other.h"
#ifdef WLED_ENABLE_PIXART
  #include "html_pixart.h"
#endif
#ifdef WLED_ENABLE_PXMAGIC
  #include "html_pxmagic.h"
#endif
#ifndef WLED_DISABLE_PIXELFORGE
  #include "html_pixelforge.h"
#endif
#include "html_cpal.h"
#include "html_edit.h"


// define flash strings once (saves flash memory)
static const char s_redirecting[] PROGMEM = "Redirecting...";
static const char s_content_enc[] PROGMEM = "Content-Encoding";
static const char s_unlock_ota [] PROGMEM = "Please unlock OTA in security settings!";
static const char s_unlock_cfg [] PROGMEM = "Please unlock settings using PIN code!";
static const char s_rebooting  [] PROGMEM = "Rebooting now...";
static const char s_notimplemented[] PROGMEM = "Not implemented";
static const char s_accessdenied[]   PROGMEM = "Access Denied";
static const char s_not_found[]      PROGMEM = "Not found";
static const char s_wsec[]           PROGMEM = "wsec.json";
static const char s_func[]           PROGMEM = "func";
static const char s_list[]           PROGMEM = "list";
static const char s_path[]           PROGMEM = "path";
static const char s_cache_control[]  PROGMEM = "Cache-Control";
static const char s_no_store[]       PROGMEM = "no-store";
static const char s_expires[]        PROGMEM = "Expires";
static const char _common_js[]       PROGMEM = "/common.js";


//Is this an IP?
static bool isIp(const String &str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

static bool inSubnet(const IPAddress &ip, const IPAddress &subnet, const IPAddress &mask) {
  return (((uint32_t)ip & (uint32_t)mask) == ((uint32_t)subnet & (uint32_t)mask));
}

static bool inSameSubnet(const IPAddress &client) {
  return inSubnet(client, Network.localIP(), Network.subnetMask());
}

static bool inLocalSubnet(const IPAddress &client) {
  return  inSubnet(client, IPAddress(10,0,0,0),    IPAddress(255,0,0,0))                  // 10.x.x.x
      ||  inSubnet(client, IPAddress(192,168,0,0), IPAddress(255,255,0,0))                // 192.168.x.x
      ||  inSubnet(client, IPAddress(172,16,0,0),  IPAddress(255,240,0,0))                // 172.16.x.x
      || (inSubnet(client, IPAddress(4,3,2,0),     IPAddress(255,255,255,0)) && apActive) // WLED AP
      ||  inSameSubnet(client);                                                           // same subnet as WLED device
}

/*
 * Integrated HTTP web server page declarations
 */

static void generateEtag(char *etag, uint16_t eTagSuffix) {
  sprintf_P(etag, PSTR("%u-%02x-%04x"), WEB_BUILD_TIME, cacheInvalidate, eTagSuffix);
}

static void setStaticContentCacheHeaders(AsyncWebServerResponse *response, int code, uint16_t eTagSuffix = 0) {
  // Only send ETag for 200 (OK) responses
  if (code != 200) return;

  // https://medium.com/@codebyamir/a-web-developers-guide-to-browser-caching-cc41f3b73e7c
  #ifndef WLED_DEBUG
  // this header name is misleading, "no-cache" will not disable cache,
  // it just revalidates on every load using the "If-None-Match" header with the last ETag value
  response->addHeader(FPSTR(s_cache_control), F("no-cache"));
  #else
  response->addHeader(FPSTR(s_cache_control), F("no-store,max-age=0"));  // prevent caching if debug build
  #endif
  char etag[32];
  generateEtag(etag, eTagSuffix);
  response->addHeader(F("ETag"), etag);
}

static bool handleIfNoneMatchCacheHeader(AsyncWebServerRequest *request, int code, uint16_t eTagSuffix = 0) {
  // Only send 304 (Not Modified) if response code is 200 (OK)
  if (code != 200) return false;

  AsyncWebHeader *header = request->getHeader(F("If-None-Match"));
  char etag[32];
  generateEtag(etag, eTagSuffix);
  if (header && header->value() == etag) {
    AsyncWebServerResponse *response = request->beginResponse(304);
    setStaticContentCacheHeaders(response, code, eTagSuffix);
    request->send(response);
    return true;
  }
  return false;
}

/**
 * Handles the request for a static file.
 * If the file was found in the filesystem, it will be sent to the client.
 * Otherwise it will be checked if the browser cached the file and if so, a 304 response will be sent.
 * If the file was not found in the filesystem and not in the browser cache, the request will be handled as a 200 response with the content of the page.
 *
 * @param request The request object
 * @param path If a file with this path exists in the filesystem, it will be sent to the client. Set to "" to skip this check.
 * @param code The HTTP status code
 * @param contentType The content type of the web page
 * @param content Content of the web page
 * @param len Length of the content
 * @param gzip Optional. Defaults to true. If false, the gzip header will not be added.
 * @param eTagSuffix Optional. Defaults to 0. A suffix that will be added to the ETag header. This can be used to invalidate the cache for a specific page.
 */
static void handleStaticContent(AsyncWebServerRequest *request, const String &path, int code, const String &contentType, const uint8_t *content, size_t len, bool gzip = true, uint16_t eTagSuffix = 0) {
  if (path != "" && handleFileRead(request, path)) return;
  if (handleIfNoneMatchCacheHeader(request, code, eTagSuffix)) return;
  AsyncWebServerResponse *response = request->beginResponse_P(code, contentType, content, len);
  if (gzip) response->addHeader(FPSTR(s_content_enc), F("gzip"));
  setStaticContentCacheHeaders(response, code, eTagSuffix);
  request->send(response);
}

#ifdef WLED_ENABLE_DMX
static String dmxProcessor(const String& var)
{
  String mapJS;
  if (var == F("DMXVARS")) {
    mapJS += F("\nCN=");
    mapJS += String(DMXChannels);
    mapJS += F(";\nCS=");
    mapJS += String(DMXStart);
    mapJS += F(";\nCG=");
    mapJS += String(DMXGap);
    mapJS += F(";\nLC=");
    mapJS += String(strip.getLengthTotal());
    mapJS += F(";\nvar CH=[");
    for (int i=0; i<15; i++) {
      mapJS += String(DMXFixtureMap[i]) + ',';
    }
    mapJS += F("0];");
  }
  return mapJS;
}
#endif

static String msgProcessor(const String& var)
{
  if (var == "MSG") {
    String messageBody = messageHead;
    messageBody += F("</h2>");
    messageBody += messageSub;
    uint32_t optt = optionType;

    if (optt < 60) //redirect to settings after optionType seconds
    {
      messageBody += F("<script>setTimeout(RS,");
      messageBody += String(optt*1000);
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


static void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool isFinal) {
  if (!correctPIN) {
    if (isFinal) request->send(401, FPSTR(CONTENT_TYPE_PLAIN), FPSTR(s_unlock_cfg));
    return;
  }
  if (!index) {
    String finalname = filename;
    if (finalname.charAt(0) != '/') {
      finalname = '/' + finalname; // prepend slash if missing
    }

    request->_tempFile = WLED_FS.open(finalname, "w");
    DEBUG_PRINTF_P(PSTR("Uploading %s\n"), finalname.c_str());
    if (finalname.equals(FPSTR(getPresetsFileName()))) presetsModifiedTime = toki.second();
  }
  if (len) {
    request->_tempFile.write(data,len);
  }
  if (isFinal) {
    request->_tempFile.close();
    if (filename.indexOf(F("cfg.json")) >= 0) { // check for filename with or without slash
      doReboot = true;
      request->send(200, FPSTR(CONTENT_TYPE_PLAIN), F("Config restore ok.\nRebooting..."));
    } else {
      if (filename.indexOf(F("palette")) >= 0 && filename.indexOf(F(".json")) >= 0) loadCustomPalettes();
      request->send(200, FPSTR(CONTENT_TYPE_PLAIN), F("File Uploaded!"));
    }
    cacheInvalidate++;
  }
}

static const char _edit_htm[] PROGMEM = "/edit.htm";

void createEditHandler() {
  if (editHandler != nullptr) server.removeHandler(editHandler);

  editHandler = &server.on(F("/edit"), static_cast<WebRequestMethod>(HTTP_GET), [](AsyncWebServerRequest *request) {
    // PIN check for GET/DELETE, for POST it is done in handleUpload()
    if (!correctPIN) {
      serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_cfg), 254);
      return;
    }
    const String& func = request->arg(FPSTR(s_func));
    bool legacyList = false;
    if (request->hasArg(FPSTR(s_list))) {
      legacyList = true; // support for '?list=/'
    }

    if(func.length() == 0 && !legacyList) {
      // default: serve the editor page
      handleStaticContent(request, FPSTR(_edit_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_edit, PAGE_edit_length);
      return;
    }

    if (func == FPSTR(s_list) || legacyList) {
      bool first = true;
      AsyncResponseStream* response = request->beginResponseStream(FPSTR(CONTENT_TYPE_JSON));
      response->addHeader(FPSTR(s_cache_control), FPSTR(s_no_store));
      response->addHeader(FPSTR(s_expires), F("0"));
      response->write('[');

      File rootdir = WLED_FS.open("/", "r");
      File rootfile = rootdir.openNextFile();
      while (rootfile) {
        String name = rootfile.name();
        if (name.indexOf(FPSTR(s_wsec)) >= 0) {
          rootfile = rootdir.openNextFile(); // skip wsec.json
          continue;
        }
        if (!first) response->write(',');
        first = false;
        response->printf_P(PSTR("{\"name\":\"%s\",\"type\":\"file\",\"size\":%u}"), name.c_str(), rootfile.size());
        rootfile = rootdir.openNextFile();
      }
      rootfile.close();
      rootdir.close();
      response->write(']');
      request->send(response);
      return;
    }

    String path = request->arg(FPSTR(s_path)); // remaining functions expect a path

    if (path.length() == 0) {
      request->send(400, FPSTR(CONTENT_TYPE_PLAIN), F("Missing path"));
      return;
    }

    if (path.charAt(0) != '/') {
      path = '/' + path; // prepend slash if missing
    }

    if (!WLED_FS.exists(path)) {
      request->send(404, FPSTR(CONTENT_TYPE_PLAIN), FPSTR(s_not_found));
      return;
    }

    if (path.indexOf(FPSTR(s_wsec)) >= 0) {
      request->send(403, FPSTR(CONTENT_TYPE_PLAIN), FPSTR(s_accessdenied)); // skip wsec.json
      return;
    }

    if (func == "edit") {
      request->send(WLED_FS, path);
      return;
    }

    if (func == "download") {
      request->send(WLED_FS, path, String(), true);
      return;
    }

    if (func == "delete") {
      if (!WLED_FS.remove(path))
        request->send(500, FPSTR(CONTENT_TYPE_PLAIN), F("Delete failed"));
      else
        request->send(200, FPSTR(CONTENT_TYPE_PLAIN), F("File deleted"));
      return;
    }

    // unrecognized func
    request->send(400, FPSTR(CONTENT_TYPE_PLAIN), F("Invalid function"));
  });
}

static bool captivePortal(AsyncWebServerRequest *request)
{
  if (!apActive) return false; //only serve captive in AP mode
  if (!request->hasHeader(F("Host"))) return false;

  String hostH = request->getHeader(F("Host"))->value();
  if (!isIp(hostH) && hostH.indexOf(F("wled.me")) < 0 && hostH.indexOf(cmDNS) < 0 && hostH.indexOf(':') < 0) {
    DEBUG_PRINTLN(F("Captive portal"));
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
  #ifndef WLED_DISABLE_2D 
  server.on(F("/liveview2D"), HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, "", 200, FPSTR(CONTENT_TYPE_HTML), PAGE_liveviewws2D, PAGE_liveviewws2D_length);
  });
  #endif
#endif
  server.on(F("/liveview"), HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, "", 200, FPSTR(CONTENT_TYPE_HTML), PAGE_liveview, PAGE_liveview_length);
  });

  server.on(_common_js, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_common_js), 200, FPSTR(CONTENT_TYPE_JAVASCRIPT), JS_common, JS_common_length);
  });

  //settings page
  server.on(F("/settings"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });

  // "/settings/settings.js&p=x" request also handled by serveSettings()
  static const char _style_css[] PROGMEM = "/style.css";
  server.on(_style_css, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_style_css), 200, FPSTR(CONTENT_TYPE_CSS), PAGE_settingsCss, PAGE_settingsCss_length);
  });

  static const char _favicon_ico[] PROGMEM = "/favicon.ico";
  server.on(_favicon_ico, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_favicon_ico), 200, F("image/x-icon"), favicon, favicon_length, false);
  });

  static const char _skin_css[] PROGMEM = "/skin.css";
  server.on(_skin_css, HTTP_GET, [](AsyncWebServerRequest *request) {
    if (handleFileRead(request, FPSTR(_skin_css))) return;
    AsyncWebServerResponse *response = request->beginResponse(200, FPSTR(CONTENT_TYPE_CSS));
    request->send(response);
  });

  server.on(F("/welcome"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });

  server.on(F("/reset"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 200, FPSTR(s_rebooting), F("Please wait ~10 seconds."), 131);
    doReboot = true;
  });

  server.on(F("/settings"), HTTP_POST, [](AsyncWebServerRequest *request){
    serveSettings(request, true);
  });

  const static char _json[] PROGMEM = "/json";
  server.on(FPSTR(_json), HTTP_GET, [](AsyncWebServerRequest *request){
    serveJson(request);
  });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(FPSTR(_json), [](AsyncWebServerRequest *request) {
    bool verboseResponse = false;
    bool isConfig = false;

    if (!requestJSONBufferLock(14)) {
      request->deferResponse();
      return;
    }

    DeserializationError error = deserializeJson(*pDoc, (uint8_t*)(request->_tempObject));
    JsonObject root = pDoc->as<JsonObject>();
    if (error || root.isNull()) {
      releaseJSONBufferLock();
      serveJsonError(request, 400, ERR_JSON);
      return;
    }
    if (root.containsKey("pin")) checkSettingsPIN(root["pin"].as<const char*>());

    const String& url = request->url();
    isConfig = url.indexOf(F("cfg")) > -1;
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
      if (!correctPIN && strlen(settingsPIN)>0) {
        releaseJSONBufferLock();
        serveJsonError(request, 401, ERR_DENIED);
        return;
      }
      verboseResponse = deserializeConfig(root); //use verboseResponse to determine whether cfg change should be saved immediately
    }
    releaseJSONBufferLock();

    if (verboseResponse) {
      if (!isConfig) {
        lastInterfaceUpdate = millis(); // prevent WS update until cooldown
        interfaceUpdateCallMode = CALL_MODE_WS_SEND; // override call mode & schedule WS update
        #ifndef WLED_DISABLE_MQTT
        // publish state to MQTT as requested in wled#4643 even if only WS response selected
        publishMqtt();
        #endif
        serveJson(request);
        return; //if JSON contains "v"
      } else {
        configNeedsWrite = true; //Save new settings to FS
      }
    }
    request->send(200, CONTENT_TYPE_JSON, F("{\"success\":true}"));
  }, JSON_BUFFER_SIZE);
  server.addHandler(handler);

  server.on(F("/version"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, FPSTR(CONTENT_TYPE_PLAIN), (String)VERSION);
  });

  server.on(F("/uptime"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, FPSTR(CONTENT_TYPE_PLAIN), (String)millis());
  });

  server.on(F("/freeheap"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, FPSTR(CONTENT_TYPE_PLAIN), (String)getFreeHeapSize());
  });

#ifdef WLED_ENABLE_USERMOD_PAGE
  server.on("/u", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, "", 200, FPSTR(CONTENT_TYPE_HTML), PAGE_usermod, PAGE_usermod_length);
  });
#endif

  server.on(F("/teapot"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 418, F("418. I'm a teapot."), F("(Tangible Embedded Advanced Project Of Twinkling)"), 254);
  });

  server.on(F("/upload"), HTTP_POST, [](AsyncWebServerRequest *request) {},
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                      size_t len, bool isFinal) {handleUpload(request, filename, index, data, len, isFinal);}
  );

  createEditHandler(); // initialize "/edit" handler, access is protected by "correctPIN"

  static const char _update[] PROGMEM = "/update";
#ifndef WLED_DISABLE_OTA
  //init ota page
  server.on(_update, HTTP_GET, [](AsyncWebServerRequest *request){
    if (otaLock) {
      serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254);
    } else
      serveSettings(request); // checks for "upd" in URL and handles PIN
  });

  server.on(_update, HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->_tempObject) {
      auto ota_result = getOTAResult(request);
      if (ota_result.first) {
        if (ota_result.second.length() > 0) {
          serveMessage(request, 500, F("Update failed!"), ota_result.second, 254);
        } else {
          serveMessage(request, 200, F("Update successful!"), FPSTR(s_rebooting), 131);
        }
      }
    } else {
      // No context structure - something's gone horribly wrong
      serveMessage(request, 500, F("Update failed!"), F("Internal server fault"), 254);
    }
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool isFinal){
    if (index == 0) { 
      // Allocate the context structure
      if (!initOTA(request)) {
        return; // Error will be dealt with after upload in response handler, above
      }

      // Privilege checks
      IPAddress client  = request->client()->remoteIP();
      if (((otaSameSubnet && !inSameSubnet(client)) && !strlen(settingsPIN)) || (!otaSameSubnet && !inLocalSubnet(client))) {        
        DEBUG_PRINTLN(F("Attempted OTA update from different/non-local subnet!"));
        serveMessage(request, 401, FPSTR(s_accessdenied), F("Client is not on local subnet."), 254);
        setOTAReplied(request);
        return;
      }
      if (!correctPIN) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_cfg), 254);
        setOTAReplied(request);
        return;
      };
      if (otaLock) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254);
        setOTAReplied(request);
        return;
      }      
    }

    handleOTAData(request, index, data, len, isFinal);
  });
#else
  const auto notSupported = [](AsyncWebServerRequest *request){
    serveMessage(request, 501, FPSTR(s_notimplemented), F("This build does not support OTA update."), 254);
  };
  server.on(_update, HTTP_GET, notSupported);
  server.on(_update, HTTP_POST, notSupported, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool isFinal){});
#endif

#if defined(ARDUINO_ARCH_ESP32) && !defined(WLED_DISABLE_OTA)
  // ESP32 bootloader update endpoint
  server.on(F("/updatebootloader"), HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->_tempObject) {
      auto bootloader_result = getBootloaderOTAResult(request);
      if (bootloader_result.first) {
        if (bootloader_result.second.length() > 0) {
          serveMessage(request, 500, F("Bootloader update failed!"), bootloader_result.second, 254);
        } else {
          serveMessage(request, 200, F("Bootloader updated successfully!"), FPSTR(s_rebooting), 131);
        }
      }
    } else {
      // No context structure - something's gone horribly wrong
      serveMessage(request, 500, F("Bootloader update failed!"), F("Internal server fault"), 254);
    }
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool isFinal){
    if (index == 0) {
      // Privilege checks
      IPAddress client = request->client()->remoteIP();
      if (((otaSameSubnet && !inSameSubnet(client)) && !strlen(settingsPIN)) || (!otaSameSubnet && !inLocalSubnet(client))) {
        DEBUG_PRINTLN(F("Attempted bootloader update from different/non-local subnet!"));
        serveMessage(request, 401, FPSTR(s_accessdenied), F("Client is not on local subnet."), 254);
        setBootloaderOTAReplied(request);
        return;
      }
      if (!correctPIN) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_cfg), 254);
        setBootloaderOTAReplied(request);
        return;
      }
      if (otaLock) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254);
        setBootloaderOTAReplied(request);
        return;
      }

      // Allocate the context structure
      if (!initBootloaderOTA(request)) {
        return; // Error will be dealt with after upload in response handler, above
      }
    }

    handleBootloaderOTAData(request, index, data, len, isFinal);
  });
#endif

#ifdef WLED_ENABLE_DMX
  server.on(F("/dmxmap"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, FPSTR(CONTENT_TYPE_HTML), PAGE_dmxmap, dmxProcessor);
  });
#endif

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (captivePortal(request)) return;
    if (!showWelcomePage || request->hasArg(F("sliders"))) {
      handleStaticContent(request, F("/index.htm"), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_index, PAGE_index_length);
    } else {
      serveSettings(request);
    }
  });

#ifndef WLED_DISABLE_2D
  #ifdef WLED_ENABLE_PIXART
  static const char _pixart_htm[] PROGMEM = "/pixart.htm";
  server.on(_pixart_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_pixart_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_pixart, PAGE_pixart_length);
  });
  #endif

  #ifdef WLED_ENABLE_PXMAGIC
  static const char _pxmagic_htm[] PROGMEM = "/pxmagic.htm";
  server.on(_pxmagic_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_pxmagic_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_pxmagic, PAGE_pxmagic_length);
  });
  #endif

  #ifndef WLED_DISABLE_PIXELFORGE
  static const char _pixelforge_htm[] PROGMEM = "/pixelforge.htm";
  server.on(_pixelforge_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_pixelforge_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_pixelforge, PAGE_pixelforge_length);
  });
  #endif
#endif

  static const char _cpal_htm[] PROGMEM = "/cpal.htm";
  server.on(_cpal_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_cpal_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_cpal, PAGE_cpal_length);
  });

#ifdef WLED_ENABLE_WEBSOCKETS
  server.addHandler(&ws);
#endif

  //called when the url is not defined here, ajax-in; get-settings
  server.onNotFound([](AsyncWebServerRequest *request){
    DEBUG_PRINTF_P(PSTR("Not-Found HTTP call: %s\n"), request->url().c_str());
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
    handleStaticContent(request, request->url(), 404, FPSTR(CONTENT_TYPE_HTML), PAGE_404, PAGE_404_length);
  });
}


void serveMessage(AsyncWebServerRequest* request, uint16_t code, const String& headl, const String& subl, byte optionT)
{
  messageHead = headl;
  messageSub = subl;
  optionType = optionT;

  request->send_P(code, FPSTR(CONTENT_TYPE_HTML), PAGE_msg, msgProcessor);
}


void serveJsonError(AsyncWebServerRequest* request, uint16_t code, uint16_t error)
{
    AsyncJsonResponse *response = new AsyncJsonResponse(64);
    if (error < ERR_NOT_IMPL) response->addHeader(F("Retry-After"), F("1"));
    response->setContentType(CONTENT_TYPE_JSON);
    response->setCode(code);
    JsonObject obj = response->getRoot();
    obj[F("error")] = error;
    response->setLength();
    request->send(response);
}


void serveSettingsJS(AsyncWebServerRequest* request)
{
  if (request->url().indexOf(FPSTR(_common_js)) > 0) {
    handleStaticContent(request, FPSTR(_common_js), 200, FPSTR(CONTENT_TYPE_JAVASCRIPT), JS_common, JS_common_length);
    return;
  }
  byte subPage = request->arg(F("p")).toInt();
  if (subPage > 10) {
    request->send_P(501, FPSTR(CONTENT_TYPE_JAVASCRIPT), PSTR("alert('Settings for this request are not implemented.');"));
    return;
  }
  if (subPage > 0 && !correctPIN && strlen(settingsPIN)>0) {
    request->send_P(401, FPSTR(CONTENT_TYPE_JAVASCRIPT), PSTR("alert('PIN incorrect.');"));
    return;
  }
  
  AsyncResponseStream *response = request->beginResponseStream(FPSTR(CONTENT_TYPE_JAVASCRIPT));
  response->addHeader(FPSTR(s_cache_control), FPSTR(s_no_store));
  response->addHeader(FPSTR(s_expires), F("0"));

  response->print(F("function GetV(){var d=document;"));
  getSettingsJS(subPage, *response);
  response->print(F("}"));
  request->send(response);
}


void serveSettings(AsyncWebServerRequest* request, bool post) {
  byte subPage = 0, originalSubPage = 0;
  const String& url = request->url();

  if (url.indexOf("sett") >= 0) {
    if      (url.indexOf(F(".js"))  > 0) subPage = SUBPAGE_JS;
    else if (url.indexOf(F(".css")) > 0) subPage = SUBPAGE_CSS;
    else if (url.indexOf(F("wifi")) > 0) subPage = SUBPAGE_WIFI;
    else if (url.indexOf(F("leds")) > 0) subPage = SUBPAGE_LEDS;
    else if (url.indexOf(F("ui"))   > 0) subPage = SUBPAGE_UI;
    else if (url.indexOf(  "sync")  > 0) subPage = SUBPAGE_SYNC;
    else if (url.indexOf(  "time")  > 0) subPage = SUBPAGE_TIME;
    else if (url.indexOf(F("sec"))  > 0) subPage = SUBPAGE_SEC;
#ifdef WLED_ENABLE_DMX
    else if (url.indexOf(  "dmx")   > 0) subPage = SUBPAGE_DMX;
#endif
    else if (url.indexOf(  "um")    > 0) subPage = SUBPAGE_UM;
#ifndef WLED_DISABLE_2D
    else if (url.indexOf(  "2D")    > 0) subPage = SUBPAGE_2D;
#endif
    else if (url.indexOf(F("lock")) > 0) subPage = SUBPAGE_LOCK;
  }
  else if (url.indexOf("/update") >= 0) subPage = SUBPAGE_UPDATE; // update page, for PIN check
  //else if (url.indexOf("/edit")   >= 0) subPage = 10;
  else subPage = SUBPAGE_WELCOME;

  bool pinRequired = !correctPIN && strlen(settingsPIN) > 0 && (subPage > (WLED_WIFI_CONFIGURED ? SUBPAGE_MENU : SUBPAGE_WIFI) && subPage < SUBPAGE_LOCK);
  if (pinRequired) {
    originalSubPage = subPage;
    subPage = SUBPAGE_PINREQ; // require PIN
  }

  // if OTA locked or too frequent PIN entry requests fail hard
  if ((subPage == SUBPAGE_WIFI && wifiLock && otaLock) || (post && pinRequired && millis()-lastEditTime < PIN_RETRY_COOLDOWN))
  {
    serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254); return;
  }

  if (post) { //settings/set POST request, saving
    IPAddress client = request->client()->remoteIP();
    if (!inLocalSubnet(client)) { // includes same subnet check
      serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_redirecting), 123);
      return;
    }
    if (subPage != SUBPAGE_WIFI || !(wifiLock && otaLock)) handleSettingsSet(request, subPage);

    char s[32];
    char s2[45] = "";

    switch (subPage) {
      case SUBPAGE_WIFI   : strcpy_P(s, PSTR("WiFi")); strcpy_P(s2, PSTR("Please connect to the new IP (if changed)")); break;
      case SUBPAGE_LEDS   : strcpy_P(s, PSTR("LED")); break;
      case SUBPAGE_UI     : strcpy_P(s, PSTR("UI")); break;
      case SUBPAGE_SYNC   : strcpy_P(s, PSTR("Sync")); break;
      case SUBPAGE_TIME   : strcpy_P(s, PSTR("Time")); break;
      case SUBPAGE_SEC    : strcpy_P(s, PSTR("Security")); if (doReboot) strcpy_P(s2, PSTR("Rebooting, please wait ~10 seconds...")); break;
#ifdef WLED_ENABLE_DMX
      case SUBPAGE_DMX    : strcpy_P(s, PSTR("DMX")); break;
#endif
      case SUBPAGE_UM     : strcpy_P(s, PSTR("Usermods")); break;
#ifndef WLED_DISABLE_2D
      case SUBPAGE_2D     : strcpy_P(s, PSTR("2D")); break;
#endif
      case SUBPAGE_PINREQ : strcpy_P(s, correctPIN ? PSTR("PIN accepted") : PSTR("PIN rejected")); break;
    }

    if (subPage != SUBPAGE_PINREQ) strcat_P(s, PSTR(" settings saved."));

    if (subPage == SUBPAGE_PINREQ && correctPIN) {
      subPage = originalSubPage; // on correct PIN load settings page the user intended
    } else {
      if (!s2[0]) strcpy_P(s2, s_redirecting);

      bool redirectAfter9s = (subPage == SUBPAGE_WIFI || ((subPage == SUBPAGE_SEC || subPage == SUBPAGE_UM) && doReboot));
      serveMessage(request, (!pinRequired ? 200 : 401), s, s2, redirectAfter9s ? 129 : (!pinRequired ? 1 : 3));
      return;
    }
  }

  int code = 200;
  String contentType = FPSTR(CONTENT_TYPE_HTML);
  const uint8_t* content;
  size_t len;

  switch (subPage) {
    case SUBPAGE_WIFI    :  content = PAGE_settings_wifi; len = PAGE_settings_wifi_length; break;
    case SUBPAGE_LEDS    :  content = PAGE_settings_leds; len = PAGE_settings_leds_length; break;
    case SUBPAGE_UI      :  content = PAGE_settings_ui;   len = PAGE_settings_ui_length;   break;
    case SUBPAGE_SYNC    :  content = PAGE_settings_sync; len = PAGE_settings_sync_length; break;
    case SUBPAGE_TIME    :  content = PAGE_settings_time; len = PAGE_settings_time_length; break;
    case SUBPAGE_SEC     :  content = PAGE_settings_sec;  len = PAGE_settings_sec_length;  break;
#ifdef WLED_ENABLE_DMX
    case SUBPAGE_DMX     :  content = PAGE_settings_dmx;  len = PAGE_settings_dmx_length;  break;
#endif
    case SUBPAGE_UM      :  content = PAGE_settings_um;   len = PAGE_settings_um_length;   break;
#ifndef WLED_DISABLE_OTA
    case SUBPAGE_UPDATE  :  content = PAGE_update;        len = PAGE_update_length;
      #ifdef ARDUINO_ARCH_ESP32
      if (request->hasArg(F("revert")) && inLocalSubnet(request->client()->remoteIP()) && Update.canRollBack()) {
        doReboot = Update.rollBack();
        if (doReboot) {
          serveMessage(request, 200, F("Reverted to previous version!"), FPSTR(s_rebooting), 133);
        } else {
          serveMessage(request, 500, F("Rollback failed!"), F("Please reboot and retry."), 254);
        }
        return;
      }
      #endif
      break;
#endif
#ifndef WLED_DISABLE_2D
    case SUBPAGE_2D      :  content = PAGE_settings_2D;   len = PAGE_settings_2D_length;   break;
#endif
    case SUBPAGE_LOCK    : {
      correctPIN = !strlen(settingsPIN); // lock if a pin is set
      serveMessage(request, 200, strlen(settingsPIN) > 0 ? PSTR("Settings locked") : PSTR("No PIN set"), FPSTR(s_redirecting), 1);
      return;
    }
    case SUBPAGE_PINREQ  :  content = PAGE_settings_pin;  len = PAGE_settings_pin_length; code = 401;                 break;
    case SUBPAGE_CSS     :  content = PAGE_settingsCss;   len = PAGE_settingsCss_length;  contentType = FPSTR(CONTENT_TYPE_CSS); break;
    case SUBPAGE_JS      :  serveSettingsJS(request); return;
    case SUBPAGE_WELCOME :  content = PAGE_welcome;       len = PAGE_welcome_length;       break;
    default:                content = PAGE_settings;      len = PAGE_settings_length;      break;
  }
  handleStaticContent(request, "", code, contentType, content, len);
}
