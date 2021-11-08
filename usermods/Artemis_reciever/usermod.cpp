/*
 *          RGB.NET (artemis) receiver
 *          
 * This works via the UDP, http is not supported apart from reporting LED count
 * 
 * 
 */
#include "wled.h"
#include <WiFiUdp.h>

WiFiUDP UDP;
const unsigned int RGBNET_localUdpPort = 1872; // local port to listen on
unsigned char RGBNET_packet[770];
long lastTime = 0;
int delayMs = 10;
bool isRGBNETUDPEnabled;

void RGBNET_readValues() {
  
  int RGBNET_packetSize = UDP.parsePacket();
  if (RGBNET_packetSize) {
    // receive incoming UDP packets
    int sequenceNumber = UDP.read();
    int channel = UDP.read();

    //channel data is not used we only supports one channel
    int len = UDP.read(RGBNET_packet, strip.getLengthTotal()*3);
    if(len==0){
      return;
    }
    
    for (int i = 0; i < len; i=i+3) {
      strip.setPixelColor(i/3, RGBNET_packet[i], RGBNET_packet[i+1], RGBNET_packet[i+2], 0);
    } 
    //strip.show();  
  }
}

//update LED strip
void RGBNET_show() {
  strip.show();
  lastTime = millis();
}

//This function provides a json with info on the number of LEDs connected
// it is needed by artemis to know how many LEDs to display on the surface
void handleConfig(AsyncWebServerRequest *request)
{
  String config = (String)"{\
  \"channels\": [\
    {\
      \"channel\": 1,\
      \"leds\": " + strip.getLengthTotal() + "\
    },\
    {\
      \"channel\": 2,\
      \"leds\": " + "0" + "\
    },\
    {\
      \"channel\": 3,\
      \"leds\": " + "0" + "\
    },\
    {\
      \"channel\": 4,\
      \"leds\": " + "0" + "\
    }\
  ]\
}";
  request->send(200, "application/json", config);
}


void userSetup()
{
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){ 
    handleConfig(request);
  });
}

void userConnected()
{
  // new wifi, who dis?
  UDP.begin(RGBNET_localUdpPort);
  isRGBNETUDPEnabled = true;
}

void userLoop()
{
  RGBNET_readValues();
    if (millis()-lastTime > delayMs) {
      RGBNET_show();
    }
}