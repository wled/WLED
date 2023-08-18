#include "wled.h"

#ifdef WLED_ENABLE_DMX_INPUT

#ifdef ESP8266
#error DMX input is only supported on ESP32
#endif

#include "dmx_input.h"
#include <rdm/responder.h>

void DMXInput::init(uint8_t rxPin, uint8_t txPin, uint8_t enPin, uint8_t inputPortNum)
{

  if (inputPortNum < 3 && inputPortNum > 0)
  {
    this->inputPortNum = inputPortNum;
  }
  else
  {
    USER_PRINTF("DMXInput: Error: invalid inputPortNum: %d\n", inputPortNum);
    return;
  }

  /**
   * TODOS:
   * - add personalities for all supported dmx input modes
   * - select the personality that is stored in flash on startup
   * - attach callback for personality change and store in flash if changed
   * - attach callback for address change and store in flash
   * - load dmx address from flash and set in config on startup
   * - attach callback to rdm identify and flash leds when on
   * - Make all important config variables available via rdm
   */
  if (rxPin > 0 && enPin > 0 && txPin > 0)
  {

    const managed_pin_type pins[] = {
        {(int8_t)txPin, false}, // these are not used as gpio pins, this isOutput is always false.
        {(int8_t)rxPin, false},
        {(int8_t)enPin, false}};
    const bool pinsAllocated = pinManager.allocateMultiplePins(pins, 3, PinOwner::DMX_INPUT);
    if (!pinsAllocated)
    {
      USER_PRINTF("DMXInput: Error: Failed to allocate pins for DMX_INPUT. Pins already in use:\n");
      USER_PRINTF("rx in use by: %s\n", pinManager.getPinOwnerText(rxPin).c_str());
      USER_PRINTF("tx in use by: %s\n", pinManager.getPinOwnerText(txPin).c_str());
      USER_PRINTF("en in use by: %s\n", pinManager.getPinOwnerText(enPin).c_str());
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
    config.software_version_label[32] = '\0'; // zero termination in case versionString string was longer than 32 chars

    if (!dmx_driver_install(inputPortNum, &config, DMX_INTR_FLAGS_DEFAULT))
    {
      USER_PRINTF("Error: Failed to install dmx driver\n");
      return;
    }

    USER_PRINTF("Listening for DMX on pin %u\n", rxPin);
    USER_PRINTF("Sending DMX on pin %u\n", txPin);
    USER_PRINTF("DMX enable pin is: %u\n", enPin);
    dmx_set_pin(inputPortNum, txPin, rxPin, enPin);

    initialized = true;
  }
  else
  {
    USER_PRINTLN("DMX input disabled due to rxPin, enPin or txPin not set");
    return;
  }
}

void DMXInput::update()
{
  if (!initialized)
  {
    return;
  }
  byte dmxdata[DMX_PACKET_SIZE];
  dmx_packet_t packet;
  unsigned long now = millis();
  if (dmx_receive(inputPortNum, &packet, 0))
  {
    if (!packet.err)
    {
      if (!connected)
      {
        USER_PRINTLN("DMX is connected!");
        connected = true;
      }

      if (isIdentifyOn())
      {
        DEBUG_PRINTLN("RDM Identify active");
        turnOnAllLeds();
      }
      else
      {
        if (!packet.is_rdm)
        {
          dmx_read(inputPortNum, dmxdata, packet.size);
          handleDMXData(1, 512, dmxdata, REALTIME_MODE_DMX, 0);
        }
      }
      lastUpdate = now;
    }
    else
    {
      /*This can happen when you first connect or disconnect your DMX devices.
        If you are consistently getting DMX errors, then something may have gone wrong. */
      DEBUG_PRINT("A DMX error occurred - ");
      DEBUG_PRINTLN(packet.err); // TODO translate err code to string for output
    }
  }
  else if (connected && (now - lastUpdate > 5000))
  {
    connected = false;
    USER_PRINTLN("DMX was disconnected.");
  }
}

void DMXInput::turnOnAllLeds()
{
  // TODO not sure if this is the correct way?
  const uint16_t numPixels = strip.getLengthTotal();
  for (uint16_t i = 0; i < numPixels; ++i)
  {
    strip.setPixelColor(i, 255, 255, 255, 255);
  }
  strip.setBrightness(255, true);
  strip.show();
}

void DMXInput::disable()
{
  if (initialized)
  {
    dmx_driver_disable(inputPortNum);
  }
}
void DMXInput::enable()
{
  if(initialized)
  {
    dmx_driver_enable(inputPortNum);
  }
}

bool DMXInput::isIdentifyOn() const
{

  uint8_t identify = 0;
  const bool gotIdentify = rdm_get_identify_device(inputPortNum, &identify);
  // gotIdentify should never be false because it is a default parameter in rdm
  // but just in case we check for it anyway
  return bool(identify) && gotIdentify;
}
#endif