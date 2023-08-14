#include "wled.h"

#ifdef WLED_ENABLE_DMX_INPUT
#include <esp_dmx.h>
/*
 * Support for DMX/RDM input via serial (e.g. max485) on ESP32
 * ESP32 Library from:
 * https://github.com/sparkfun/SparkFunDMX
 */

static dmx_port_t dmxInputPort = 2; //TODO make this configurable
static bool dmxInputInitialized = false; //true once initDmx finished successfully
static bool dmxIsConnected = false;
static unsigned long dmxLastUpdate = 0;

void initDMXInput() {

    /**
     * TODOS:
     * - add personalities for all supported dmx input modes
     * - select the personality that is stored in flash on startup
     * - attach callback for personality change and store in flash if changed 
     * - attach callback for address change and store in flash
     * - load dmx address from flash and set in config on startup
     * - attach callback to rdm identify and flash leds when on
     * - Turn this into a class
     * - Make all important config variables available via rdm
    */
  if(dmxInputReceivePin > 0 && dmxInputEnablePin > 0 && dmxInputTransmitPin > 0) 
  {

    const managed_pin_type pins[] = {
      {(int8_t)dmxInputTransmitPin, false}, //these are not used as gpio pins, this isOutput is always false.
      {(int8_t)dmxInputReceivePin, false},
      {(int8_t)dmxInputEnablePin, false}      
    };
    const bool pinsAllocated = pinManager.allocateMultiplePins(pins, 3, PinOwner::DMX_INPUT);
    if(!pinsAllocated)
    {
      USER_PRINTF("Error: Failed to allocate pins for DMX_INPUT. Pins already in use:\n");
      USER_PRINTF("rx in use by: %s\n", pinManager.getPinOwnerText(dmxInputReceivePin).c_str());
      USER_PRINTF("tx in use by: %s\n", pinManager.getPinOwnerText(dmxInputTransmitPin).c_str());
      USER_PRINTF("en in use by: %s\n", pinManager.getPinOwnerText(dmxInputEnablePin).c_str());
      return;
    }

    dmx_config_t config{                                             
        255,                          /*alloc_size*/             
        0,                            /*model_id*/               
        RDM_PRODUCT_CATEGORY_FIXTURE, /*product_category*/      
        VERSION,                      /*software_version_id*/    
        "undefined",                  /*software_version_label*/ 
        1,                            /*current_personality*/    
        {{15, "WLED Effect Mode"}},   /*personalities*/          
        1,                            /*personality_count*/      
        1,                            /*dmx_start_address*/      
    };
    const std::string versionString = "WLED_V" + std::to_string(VERSION);
    strncpy(config.software_version_label, versionString.c_str(), 32);
    config.software_version_label[32] = '\0';//zero termination in case versionString string was longer than 32 chars

    if(!dmx_driver_install(dmxInputPort, &config, DMX_INTR_FLAGS_DEFAULT))
    {
      USER_PRINTF("Error: Failed to install dmx driver\n");
      return;
    }
    
    USER_PRINTF("Listening for DMX on pin %u\n", dmxInputReceivePin);
    USER_PRINTF("Sending DMX on pin %u\n", dmxInputTransmitPin);
    USER_PRINTF("DMX enable pin is: %u\n", dmxInputEnablePin);
    dmx_set_pin(dmxInputPort, dmxInputTransmitPin, dmxInputReceivePin, dmxInputEnablePin);

    dmxInputInitialized = true;
  }
  else 
  {
    USER_PRINTLN("DMX input disabled due to dmxInputReceivePin, dmxInputEnablePin or dmxInputTransmitPin not set");
    return;
  }
}

void handleDMXInput() {
  if(!dmxInputInitialized) {
    return;
  }
  byte dmxdata[DMX_PACKET_SIZE];
  dmx_packet_t packet;
  unsigned long now = millis();
  if (dmx_receive(dmxInputPort, &packet, 0)) {
    if (!packet.err) {
      if (!dmxIsConnected) {
        USER_PRINTLN("DMX is connected!");
        dmxIsConnected = true;
      }

      dmx_read(dmxInputPort, dmxdata, packet.size);
      handleDMXData(1, 512, dmxdata, REALTIME_MODE_DMX, 0);
      dmxLastUpdate = now;

    } else {
      /*This can happen when you first connect or disconnect your DMX devices.
        If you are consistently getting DMX errors, then something may have gone wrong. */
     DEBUG_PRINT("A DMX error occurred - ");
     DEBUG_PRINTLN(packet.err); //TODO translate err code to string for output
    }
  }
  else if (dmxIsConnected && (now - dmxLastUpdate > 5000)) {
    dmxIsConnected = false;
    USER_PRINTLN("DMX was disconnected.");
  }
}


#else
void initDMXInput(){}
void handleDMXInput(){}

#endif