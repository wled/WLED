#include "wled.h"
#include <WiFiUdp.h>

// Maximum number of lights supported
#define MAX_WIZ_LIGHTS 15

WiFiUDP UDP;




class WizLightsUsermod : public Usermod {
  
  private:
    unsigned long lastTime = 0;
    long updateInterval;
    long sendDelay;
    
    long forceUpdateMinutes;
    bool forceUpdate;

    bool useEnhancedWhite;
    long warmWhite;
    long coldWhite;

    IPAddress lightsIP[MAX_WIZ_LIGHTS];    // Stores Light IP addresses
    bool      lightsValid[MAX_WIZ_LIGHTS]; // Stores Light IP address validity
    uint32_t  colorsSent[MAX_WIZ_LIGHTS];  // Stores last color sent for each light



  public:



    // Send JSON blob to WiZ Light over UDP
    // RGB or C/W white
    // TODO:
    //   Better utilize WLED existing white mixing logic
    void wizSendColor(IPAddress ip, uint32_t color) {
      UDP.beginPacket(ip, 38899);

      // If no LED color, turn light off. Note wiz light setting for "Off fade-out" will be applied by the light itself.
      if (color == 0) { 
        UDP.print("{\"method\":\"setPilot\",\"params\":{\"state\":false}}");

      // If color is WHITE, try and use the lights WHITE LEDs instead of mixing RGB LEDs
      } else if (color == 16777215 && useEnhancedWhite){

        // set cold white light only
        if (coldWhite > 0 && warmWhite == 0){
          UDP.print("{\"method\":\"setPilot\",\"params\":{\"c\":"); UDP.print(coldWhite) ;UDP.print("}}");}
          
        // set warm white light only
        if (warmWhite > 0 && coldWhite == 0){
          UDP.print("{\"method\":\"setPilot\",\"params\":{\"w\":"); UDP.print(warmWhite) ;UDP.print("}}");}
          
        // set combination of warm and cold white light
        if (coldWhite > 0 && warmWhite > 0){
          UDP.print("{\"method\":\"setPilot\",\"params\":{\"c\":"); UDP.print(coldWhite) ;UDP.print(",\"w\":"); UDP.print(warmWhite); UDP.print("}}");}

      // Send color as RGB  
      } else {
        UDP.print("{\"method\":\"setPilot\",\"params\":{\"r\":");
        UDP.print(R(color));
        UDP.print(",\"g\":");
        UDP.print(G(color));
        UDP.print(",\"b\":");
        UDP.print(B(color));
        UDP.print("}}");
      }
    
      UDP.endPacket();
    }

    // Override definition so it compiles
    void setup() {
      
    }


    // TODO: Check millis() rollover
    void loop() {
      
      // Make sure we are connected first
      if (!WLED_CONNECTED) return;
      
      unsigned long ellapsedTime = millis() - lastTime;
      if (ellapsedTime > updateInterval) {
        bool update = false;
        for (uint8_t i = 0; i < MAX_WIZ_LIGHTS; i++) {
          if (!lightsValid[i]) { continue; }
          uint32_t newColor = strip.getPixelColor(i);
          if (forceUpdate || (newColor != colorsSent[i]) || (ellapsedTime > forceUpdateMinutes*60000)){
            wizSendColor(lightsIP[i], newColor);
            colorsSent[i] = newColor;
            update = true;
            delay(sendDelay);
            }
          }
        if (update) lastTime = millis();
        }
    }



    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject("wizLightsUsermod");
      top["Interval (ms)"]                = updateInterval;
      top["Send Delay (ms)"]              = sendDelay;
      top["Use Enhanced White *"]         = useEnhancedWhite;
      top["* Warm White Value (0-255)"]   = warmWhite;
      top["* Cold White Value (0-255)"]   = coldWhite;
      top["Always Force Update"]          = forceUpdate;
      top["Force Update Every x Minutes"] = forceUpdateMinutes;
      
      for (uint8_t i = 0; i < MAX_WIZ_LIGHTS; i++) {
        top[getJsonLabel(i)] = lightsIP[i].toString();
      }
    }



    bool readFromConfig(JsonObject& root)
    {
      JsonObject top = root["wizLightsUsermod"];
      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top["Interval (ms)"],                updateInterval,     1000);  // How frequently to update the wiz lights
      configComplete &= getJsonValue(top["Send Delay (ms)"],              sendDelay,          0);     // Optional delay after sending each UDP message
      configComplete &= getJsonValue(top["Use Enhanced White *"],         useEnhancedWhite,   false); // When color is white use wiz white LEDs instead of mixing RGB
      configComplete &= getJsonValue(top["* Warm White Value (0-255)"],   warmWhite,          0);     // Warm White LED value for Enhanced White
      configComplete &= getJsonValue(top["* Cold White Value (0-255)"],   coldWhite,          50);   // Cold White LED value for Enhanced White
      configComplete &= getJsonValue(top["Always Force Update"],          forceUpdate,        false); // Update wiz light every loop, even if color value has not changed
      configComplete &= getJsonValue(top["Force Update Every x Minutes"], forceUpdateMinutes, 5);     // Update wiz light if color value has not changed, every x minutes
      
      // Read list of IPs
      String tempIp;
      for (uint8_t i = 0; i < MAX_WIZ_LIGHTS; i++) {
        configComplete &= getJsonValue(top[getJsonLabel(i)], tempIp, "0.0.0.0");
        lightsValid[i] = lightsIP[i].fromString(tempIp);
        
        // If the IP is not valid, force the value to be empty
        if (!lightsValid[i]){lightsIP[i].fromString("0.0.0.0");}
        }

      return configComplete;
    }


   // Create label for the usermod page (I cannot make it work with JSON arrays...)
    String getJsonLabel(uint8_t i) {return "WiZ Light IP #" + String(i+1);}
        
    uint16_t getId(){return USERMOD_ID_WIZLIGHTS;}
};


static WizLightsUsermod wizlights;
REGISTER_USERMOD(wizlights);