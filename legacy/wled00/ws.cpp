#include "wled.h"

/*
 * WebSockets server for bidirectional communication
 */
#ifdef WLED_ENABLE_WEBSOCKETS

uint16_t wsLiveClientId = 0;
unsigned long wsLastLiveTime = 0;
//uint8_t* wsFrameBuffer = nullptr;

#define WS_LIVE_INTERVAL 40

void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if(type == WS_EVT_CONNECT){
    //client connected
    DEBUG_PRINTLN(F("WS client connected."));
    sendDataWs(client);
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    if (client->id() == wsLiveClientId) wsLiveClientId = 0;
    DEBUG_PRINTLN(F("WS client disconnected."));
  } else if(type == WS_EVT_DATA){
    // data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      // the whole message is in a single frame and we got all of its data (max. 1450 bytes)
      if(info->opcode == WS_TEXT)
      {
        if (len > 0 && len < 10 && data[0] == 'p') {
          // application layer ping/pong heartbeat.
          // client-side socket layer ping packets are unanswered (investigate)
          client->text(F("pong"));
          return;
        }

        bool verboseResponse = false;
        if (!requestJSONBufferLock(11)) {
          client->text(F("{\"error\":3}")); // ERR_NOBUF
          return;
        }

        DeserializationError error = deserializeJson(*pDoc, data, len);
        JsonObject root = pDoc->as<JsonObject>();
        if (error || root.isNull()) {
          releaseJSONBufferLock();
          return;
        }
        if (root["v"] && root.size() == 1) {
          //if the received value is just "{"v":true}", send only to this client
          verboseResponse = true;
        } else if (root.containsKey("lv")) {
          wsLiveClientId = root["lv"] ? client->id() : 0;
        } else {
          verboseResponse = deserializeState(root);
        }
        releaseJSONBufferLock();

        if (!interfaceUpdateCallMode) { // individual client response only needed if no WS broadcast soon
          if (verboseResponse) {
            sendDataWs(client);
          } else {
            // we have to send something back otherwise WS connection closes
            client->text(F("{\"success\":true}"));
          }
          // force broadcast in 500ms after updating client
          //lastInterfaceUpdate = millis() - (INTERFACE_UPDATE_COOLDOWN -500); // ESP8266 does not like this
        }
      }
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      //if(info->index == 0){
        //if (!wsFrameBuffer && len < 4096) wsFrameBuffer = new uint8_t[4096];
      //}

      //if (wsFrameBuffer && len < 4096 && info->index + info->)
      //{

      //}

      if((info->index + len) == info->len){
        if(info->final){
          if(info->message_opcode == WS_TEXT) {
            client->text(F("{\"error\":9}")); // ERR_JSON we do not handle split packets right now
          }
        }
      }
      DEBUG_PRINTLN(F("WS multipart message."));
    }
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    DEBUG_PRINTLN(F("WS error."));

  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    DEBUG_PRINTLN(F("WS pong."));

  } else {
    DEBUG_PRINTLN(F("WS unknown event."));
  }
}

void sendDataWs(AsyncWebSocketClient * client)
{
  if (!ws.count()) return;

  if (!requestJSONBufferLock(12)) {
    const char* error = PSTR("{\"error\":3}");
    if (client) {
      client->text(FPSTR(error)); // ERR_NOBUF
    } else {
      ws.textAll(FPSTR(error)); // ERR_NOBUF
    }
    return;
  }

  JsonObject state = pDoc->createNestedObject("state");
  serializeState(state);
  JsonObject info  = pDoc->createNestedObject("info");
  serializeInfo(info);

  size_t len = measureJson(*pDoc);
  DEBUG_PRINTF_P(PSTR("JSON buffer size: %u for WS request (%u).\n"), pDoc->memoryUsage(), len);

  // the following may no longer be necessary as heap management has been fixed by @willmmiles in AWS
  size_t heap1 = ESP.getFreeHeap();
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());
  #ifdef ESP8266
  if (len>heap1) {
    DEBUG_PRINTLN(F("Out of memory (WS)!"));
    return;
  }
  #endif
  AsyncWebSocketBuffer buffer(len);
  #ifdef ESP8266
  size_t heap2 = ESP.getFreeHeap();
  DEBUG_PRINTF_P(PSTR("heap %u\n"), ESP.getFreeHeap());
  #else
  size_t heap2 = 0; // ESP32 variants do not have the same issue and will work without checking heap allocation
  #endif
  if (!buffer || heap1-heap2<len) {
    releaseJSONBufferLock();
    DEBUG_PRINTLN(F("WS buffer allocation failed."));
    ws.closeAll(1013); //code 1013 = temporary overload, try again later
    ws.cleanupClients(0); //disconnect all clients to release memory
    return; //out of memory
  }
  serializeJson(*pDoc, (char *)buffer.data(), len);

  DEBUG_PRINT(F("Sending WS data "));
  if (client) {
    DEBUG_PRINTLN(F("to a single client."));
    client->text(std::move(buffer));
  } else {
    DEBUG_PRINTLN(F("to multiple clients."));
    ws.textAll(std::move(buffer));
  }

  releaseJSONBufferLock();
}

bool sendLiveLedsWs(uint32_t wsClient)
{
  AsyncWebSocketClient * wsc = ws.client(wsClient);
  if (!wsc || wsc->queueLength() > 0) return false; //only send if queue free

  size_t used = strip.getLengthTotal();
#ifdef ESP8266
  const size_t MAX_LIVE_LEDS_WS = 256U;
#else
  const size_t MAX_LIVE_LEDS_WS = 1024U;
#endif
  size_t n = ((used -1)/MAX_LIVE_LEDS_WS) +1; //only serve every n'th LED if count over MAX_LIVE_LEDS_WS
  size_t pos = 2;  // start of data
#ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    // ignore anything behid matrix (i.e. extra strip)
    used = Segment::maxWidth*Segment::maxHeight; // always the size of matrix (more or less than strip.getLengthTotal())
    n = 1;
    if (used > MAX_LIVE_LEDS_WS) n = 2;
    if (used > MAX_LIVE_LEDS_WS*4) n = 4;
    pos = 4;
  }
#endif
  size_t bufSize = pos + (used/n)*3;

  AsyncWebSocketBuffer wsBuf(bufSize);
  if (!wsBuf) return false; //out of memory
  uint8_t* buffer = reinterpret_cast<uint8_t*>(wsBuf.data());
  if (!buffer) return false; //out of memory
  buffer[0] = 'L';
  buffer[1] = 1; //version

#ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    buffer[1] = 2; //version
    buffer[2] = Segment::maxWidth/n;
    buffer[3] = Segment::maxHeight/n;
  }
#endif

  for (size_t i = 0; pos < bufSize -2; i += n)
  {
#ifndef WLED_DISABLE_2D
    if (strip.isMatrix && n>1 && (i/Segment::maxWidth)%n) i += Segment::maxWidth * (n-1);
#endif
    uint32_t c = strip.getPixelColor(i);
    uint8_t r = R(c);
    uint8_t g = G(c);
    uint8_t b = B(c);
    uint8_t w = W(c);
    buffer[pos++] = bri ? qadd8(w, r) : 0; //R, add white channel to RGB channels as a simple RGBW -> RGB map
    buffer[pos++] = bri ? qadd8(w, g) : 0; //G
    buffer[pos++] = bri ? qadd8(w, b) : 0; //B
  }

  wsc->binary(std::move(wsBuf));
  return true;
}

void handleWs()
{
  if (millis() - wsLastLiveTime > WS_LIVE_INTERVAL)
  {
    #ifdef ESP8266
    ws.cleanupClients(3);
    #else
    ws.cleanupClients();
    #endif
    bool success = true;
    if (wsLiveClientId) success = sendLiveLedsWs(wsLiveClientId);
    wsLastLiveTime = millis();
    if (!success) wsLastLiveTime -= 20; //try again in 20ms if failed due to non-empty WS queue
  }
}

#else
void handleWs() {}
void sendDataWs(AsyncWebSocketClient * client) {}
#endif