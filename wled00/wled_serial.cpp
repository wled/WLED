#include "wled.h"

/*
 * Adalight and TPM2 handler
 */

enum class AdaState {
  Header_A,
  Header_d,
  Header_a,
  Header_CountHi,
  Header_CountLo,
  Header_CountCheck,
  Data_Red,
  Data_Green,
  Data_Blue,
  TPM2_Header_Type,
  TPM2_Header_CountHi,
  TPM2_Header_CountLo,
};

void handleSerial()
{
  if (pinManager.isPinAllocated(3)) return;
  
  #ifdef WLED_ENABLE_ADALIGHT
  static auto state = AdaState::Header_A;
  static uint16_t count = 0;
  static uint16_t pixel = 0;
  static byte check = 0x00;
  static byte red   = 0x00;
  static byte green = 0x00;
  
  while (Serial.available() > 0)
  {
    yield();
    byte next = Serial.peek();
    switch (state) {
      case AdaState::Header_A:
        if (next == 'A') state = AdaState::Header_d;
        else if (next == 0xC9) { //TPM2 start byte
          state = AdaState::TPM2_Header_Type;
        }
        else if (next == 'I') {
          handleImprovPacket();
          return;
        } else if (next == 'v') {
          Serial.print("WLED"); Serial.write(' '); Serial.println(VERSION);
        } else if (next == '{') { //JSON API
          bool verboseResponse = false;
          #ifdef WLED_USE_DYNAMIC_JSON
          DynamicJsonDocument doc(JSON_BUFFER_SIZE);
          #else
          if (!requestJSONBufferLock(16)) return;
          #endif
          Serial.setTimeout(100);
          DeserializationError error = deserializeJson(doc, Serial);
          if (error) {
            releaseJSONBufferLock();
            return;
          }
          verboseResponse = deserializeState(doc.as<JsonObject>());
          //only send response if TX pin is unused for other purposes
          if (verboseResponse && !pinManager.isPinAllocated(1)) {
            doc.clear();
            JsonObject state = doc.createNestedObject("state");
            serializeState(state);
            JsonObject info  = doc.createNestedObject("info");
            serializeInfo(info);

            serializeJson(doc, Serial);
            Serial.println();
          }
          releaseJSONBufferLock();
        }
        break;
      case AdaState::Header_d:
        if (next == 'd') state = AdaState::Header_a;
        else             state = AdaState::Header_A;
        break;
      case AdaState::Header_a:
        if (next == 'a') state = AdaState::Header_CountHi;
        else             state = AdaState::Header_A;
        break;
      case AdaState::Header_CountHi:
        pixel = 0;
        count = next * 0x100;
        check = next;
        state = AdaState::Header_CountLo;
        break;
      case AdaState::Header_CountLo:
        count += next + 1;
        check = check ^ next ^ 0x55;
        state = AdaState::Header_CountCheck;
        break;
      case AdaState::Header_CountCheck:
        if (check == next) state = AdaState::Data_Red;
        else               state = AdaState::Header_A;
        break;
      case AdaState::TPM2_Header_Type:
        state = AdaState::Header_A; //(unsupported) TPM2 command or invalid type
        if (next == 0xDA) state = AdaState::TPM2_Header_CountHi; //TPM2 data
        else if (next == 0xAA) Serial.write(0xAC); //TPM2 ping
        break;
      case AdaState::TPM2_Header_CountHi:
        pixel = 0;
        count = (next * 0x100) /3;
        state = AdaState::TPM2_Header_CountLo;
        break;
      case AdaState::TPM2_Header_CountLo:
        count += next /3;
        state = AdaState::Data_Red;
        break;
      case AdaState::Data_Red:
        red   = next;
        state = AdaState::Data_Green;
        break;
      case AdaState::Data_Green:
        green = next;
        state = AdaState::Data_Blue;
        break;
      case AdaState::Data_Blue:
        byte blue  = next;
        if (!realtimeOverride) setRealtimePixel(pixel++, red, green, blue, 0);
        if (--count > 0) state = AdaState::Data_Red;
        else {
          if (!realtimeMode && bri == 0) strip.setBrightness(briLast);
          realtimeLock(realtimeTimeoutMs, REALTIME_MODE_ADALIGHT);

          if (!realtimeOverride) strip.show();
          state = AdaState::Header_A;
        }
        break;
    }
    Serial.read(); //discard the byte
  }
  #endif
}
