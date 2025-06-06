#include "wled.h"

#ifdef WLED_ENABLE_DMX_INPUT

#ifdef ESP8266
#error DMX input is only supported on ESP32
#endif

#include "dmx_input.h"
#include <rdm/responder.h>

void rdmPersonalityChangedCb(dmx_port_t dmxPort, const rdm_header_t *header,
                             void *context)
{
  DMXInput *dmx = static_cast<DMXInput *>(context);

  if (!dmx) {
    DEBUG_PRINTLN("DMX: Error: no context in rdmPersonalityChangedCb");
    return;
  }

  if (header->cc == RDM_CC_SET_COMMAND_RESPONSE) {
    const uint8_t personality = dmx_get_current_personality(dmx->inputPortNum);
    DMXMode = std::min(DMX_MODE_PRESET, std::max(DMX_MODE_SINGLE_RGB, int(personality)));
    configNeedsWrite = true;
    DEBUG_PRINTF("DMX personality changed to to: %d\n", DMXMode);
  }
}

void rdmAddressChangedCb(dmx_port_t dmxPort, const rdm_header_t *header,
                         void *context)
{
  DMXInput *dmx = static_cast<DMXInput *>(context);

  if (!dmx) {
    DEBUG_PRINTLN("DMX: Error: no context in rdmAddressChangedCb");
    return;
  }

  if (header->cc == RDM_CC_SET_COMMAND_RESPONSE) {
    const uint16_t addr = dmx_get_start_address(dmx->inputPortNum);
    DMXAddress = std::min(512, int(addr));
    configNeedsWrite = true;
    DEBUG_PRINTF("DMX start addr changed to: %d\n", DMXAddress);
  }
}

static dmx_config_t createConfig()
{
  dmx_config_t config;
  config.pd_size = 255;
  config.dmx_start_address = DMXAddress;
  config.model_id = 0;
  config.product_category = RDM_PRODUCT_CATEGORY_FIXTURE;
  config.software_version_id = VERSION;
  strcpy(config.device_label, "WLED_MM");

  const std::string versionString = "WLED_V" + std::to_string(VERSION);
  strncpy(config.software_version_label, versionString.c_str(), 32);
  config.software_version_label[32] = '\0'; // zero termination in case versionString string was longer than 32 chars

  config.personalities[0].description = "SINGLE_RGB";
  config.personalities[0].footprint = 3;
  config.personalities[1].description = "SINGLE_DRGB";
  config.personalities[1].footprint = 4;
  config.personalities[2].description = "EFFECT";
  config.personalities[2].footprint = 15;
  config.personalities[3].description = "MULTIPLE_RGB";
  config.personalities[3].footprint = std::min(512, int(strip.getLengthTotal()) * 3);
  config.personalities[4].description = "MULTIPLE_DRGB";
  config.personalities[4].footprint = std::min(512, int(strip.getLengthTotal()) * 3 + 1);
  config.personalities[5].description = "MULTIPLE_RGBW";
  config.personalities[5].footprint = std::min(512, int(strip.getLengthTotal()) * 4);
  config.personalities[6].description = "EFFECT_W";
  config.personalities[6].footprint = 18;
  config.personalities[7].description = "EFFECT_SEGMENT";
  config.personalities[7].footprint = std::min(512, strip.getSegmentsNum() * 15);
  config.personalities[8].description = "EFFECT_SEGMENT_W";
  config.personalities[8].footprint = std::min(512, strip.getSegmentsNum() * 18);
  config.personalities[9].description = "PRESET";
  config.personalities[9].footprint = 1;

  config.personality_count = 10;
  // rdm personalities are numbered from 1, thus we can just set the DMXMode directly.
  config.current_personality = DMXMode;

  return config;
}

void dmxReceiverTask(void *context)
{
  DMXInput *instance = static_cast<DMXInput *>(context);
  if (instance == nullptr) {
    return;
  }

  if (instance->installDriver()) {
    while (true) {
      instance->updateInternal();
    }
  }
}

bool DMXInput::installDriver()
{

  const auto config = createConfig();
  DEBUG_PRINTF("DMX port: %u\n", inputPortNum);
  if (!dmx_driver_install(inputPortNum, &config, DMX_INTR_FLAGS_DEFAULT)) {
    DEBUG_PRINTF("Error: Failed to install dmx driver\n");
    return false;
  }

  DEBUG_PRINTF("Listening for DMX on pin %u\n", rxPin);
  DEBUG_PRINTF("Sending DMX on pin %u\n", txPin);
  DEBUG_PRINTF("DMX enable pin is: %u\n", enPin);
  dmx_set_pin(inputPortNum, txPin, rxPin, enPin);

  rdm_register_dmx_start_address(inputPortNum, rdmAddressChangedCb, this);
  rdm_register_dmx_personality(inputPortNum, rdmPersonalityChangedCb, this);
  initialized = true;
  return true;
}

void DMXInput::init(uint8_t rxPin, uint8_t txPin, uint8_t enPin, uint8_t inputPortNum)
{

#ifdef WLED_ENABLE_DMX_OUTPUT
  //TODO add again once dmx output has been merged
  // if(inputPortNum == dmxOutputPort)
  // {
  //   DEBUG_PRINTF("DMXInput: Error: Input port == output port");
  //   return;
  // }
#endif

  if (inputPortNum <= (SOC_UART_NUM - 1) && inputPortNum > 0) {
    this->inputPortNum = inputPortNum;
  }
  else {
    DEBUG_PRINTF("DMXInput: Error: invalid inputPortNum: %d\n", inputPortNum);
    return;
  }

  if (rxPin > 0 && enPin > 0 && txPin > 0) {

    const managed_pin_type pins[] = {
        {(int8_t)txPin, false}, // these are not used as gpio pins, thus isOutput is always false.
        {(int8_t)rxPin, false},
        {(int8_t)enPin, false}};
    const bool pinsAllocated = PinManager::allocateMultiplePins(pins, 3, PinOwner::DMX_INPUT);
    if (!pinsAllocated) {
      DEBUG_PRINTF("DMXInput: Error: Failed to allocate pins for DMX_INPUT. Pins already in use:\n");
      DEBUG_PRINTF("rx in use by: %s\n", PinManager::getPinOwner(rxPin));
      DEBUG_PRINTF("tx in use by: %s\n", PinManager::getPinOwner(txPin));
      DEBUG_PRINTF("en in use by: %s\n", PinManager::getPinOwner(enPin));
      return;
    }

    this->rxPin = rxPin;
    this->txPin = txPin;
    this->enPin = enPin;

    // put dmx receiver into seperate task because it should not be blocked
    // pin to core 0 because wled is running on core 1
    xTaskCreatePinnedToCore(dmxReceiverTask, "DMX_RCV_TASK", 10240, this, 2, &task, 0);
    if (!task) {
      DEBUG_PRINTF("Error: Failed to create dmx rcv task");
    }
  }
  else {
    DEBUG_PRINTLN("DMX input disabled due to rxPin, enPin or txPin not set");
    return;
  }
}

void DMXInput::updateInternal()
{
  if (!initialized) {
    return;
  }

  checkAndUpdateConfig();

  dmx_packet_t packet;
  unsigned long now = millis();
  if (dmx_receive(inputPortNum, &packet, DMX_TIMEOUT_TICK)) {
    if (!packet.err) {
      if(!connected) {
        DEBUG_PRINTLN("DMX Input - connected");
      }
      connected = true;
      identify = isIdentifyOn();
      if (!packet.is_rdm) {
        const std::lock_guard<std::mutex> lock(dmxDataLock);
        dmx_read(inputPortNum, dmxdata, packet.size);
      }
    }
    else {
      connected = false;
    }
  }
  else {
    if(connected) {
      DEBUG_PRINTLN("DMX Input - disconnected");
    }
    connected = false;
  }
}


void DMXInput::update()
{
  if (identify) {
    turnOnAllLeds();
  }
  else if (connected) {
    const std::lock_guard<std::mutex> lock(dmxDataLock);
    handleDMXData(1, 512, dmxdata, REALTIME_MODE_DMX, 0);
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
  if (initialized) {
    dmx_driver_disable(inputPortNum);
  }
}
void DMXInput::enable()
{
  if (initialized) {
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

void DMXInput::checkAndUpdateConfig()
{

  /**
   * The global configuration variables are modified by the web interface.
   * If they differ from the driver configuration, we have to update the driver
   * configuration.
   */

  const uint8_t currentPersonality = dmx_get_current_personality(inputPortNum);
  if (currentPersonality != DMXMode) {
    DEBUG_PRINTF("DMX personality has changed from %d to %d\n", currentPersonality, DMXMode);
    dmx_set_current_personality(inputPortNum, DMXMode);
  }

  const uint16_t currentAddr = dmx_get_start_address(inputPortNum);
  if (currentAddr != DMXAddress) {
    DEBUG_PRINTF("DMX address has changed from %d to %d\n", currentAddr, DMXAddress);
    dmx_set_start_address(inputPortNum, DMXAddress);
  }
}

#endif