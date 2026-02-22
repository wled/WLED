/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#include "wled.h"
#include "PluginAPI/custom/DummySensor/DummySensor.h"

// override this via PlatformIO with your custom LED / relay GPIO
#ifndef UM_PLUGIN_DEMO_RELAY_PIN
#define UM_PLUGIN_DEMO_RELAY_PIN 8 // Pin number of a relay or LED.
#endif

extern uint16_t mode_static(void);

//--------------------------------------------------------------------------------------------------

/** Example effect for processing the readings of a TemperatureSensor and a HumiditySensor plugin.
 * It also shows how to access the custom API of a plugin, which may or may not be compiled into
 * the WLED binary.
 */
uint16_t mode_Thermometer()
{
  SEGMENT.clear();

  // try to get the specific API of the DummySensor
  DummySensor *dummySensor = GET_PLUGIN_API(DummySensor);
  if (dummySensor)
  {
    // fill the background with light yellow when we're connected to the DummySensor
    SEGMENT.fill(0x080800);
    // enable/disable its TemperatureSensor
    if (SEGMENT.check3)
    {
      if (!dummySensor->isTemperatureSensorEnabled())
        dummySensor->enableTemperatureSensor();
    }
    else
    {
      if (dummySensor->isTemperatureSensorEnabled())
        dummySensor->disableTemperatureSensor();
    }
    // enable/disable its HumiditySensor
    if (SEGMENT.check2)
    {
      if (!dummySensor->isHumiditySensorEnabled())
        dummySensor->enableHumiditySensor();
    }
    else
    {
      if (dummySensor->isHumiditySensorEnabled())
        dummySensor->disableHumiditySensor();
    }
  }

  // try to get an external Temperature- and HumiditySensor
  TemperatureSensor *tempSensor = pluginManager.getTemperatureSensor();
  HumiditySensor *humSensor = pluginManager.getHumiditySensor();
  if (!tempSensor && !humSensor)
    return mode_static();

  // got a TemperatureSensor? --> draw its reading as a bar
  if (tempSensor && tempSensor->isReady())
  {
    // read the temperature value from the plugin
    const float temp = tempSensor->temperatureC();
    // and draw a representing bar in the FX color
    int tempPos = temp * (SEGLEN - 1) / 40.0f; // 20° shall be in the middle
    while (tempPos >= 0)
      SEGMENT.setPixelColor(tempPos--, fast_color_scale(SEGCOLOR(0), 64));
  }

  // draw some dots as scale
  SEGMENT.setPixelColor(SEGLEN / 4, 0x8080F8);     // 10°C
  SEGMENT.setPixelColor(SEGLEN / 2, 0x808080);     // 20°C
  SEGMENT.setPixelColor(SEGLEN * 3 / 4, 0xF88080); // 30°C

  // got a HumiditySensor? --> draw its reading as a dot
  if (humSensor && humSensor->isReady())
  {
    // read the humidity value from the plugin
    const float hum = humSensor->humidity();
    // and draw a representing dot in magenta
    const int humPos = hum * (SEGLEN - 1) / 100.0f;
    SEGMENT.setPixelColor(humPos, 0xFF00FF);
  }

  return FRAMETIME;
}
static const char _data_FX_MODE_EX_UM_THERMOMETER[] PROGMEM = "Ex: Thermometer@,,,,,,Humidity Dummy,Temperature Dummy;!;;o2=1,o3=1";

//--------------------------------------------------------------------------------------------------

static constexpr char _name[] = "Demo-Plugin";

/** A usermod as plugin example.
 */
class UM_PluginDemo : public Usermod, public PinUser
{
  // ----- usermod functions -----

  void setup() override
  {
    registerEffects();
    registerPins();
  }

  void loop() override
  {
    processFanControl();
  }

  // ----- initialization helper functions -----

  void registerEffects()
  {
    strip.addEffect(255, &mode_Thermometer, _data_FX_MODE_EX_UM_THERMOMETER);
  }

  void registerPins()
  {
    // for single pin configuration:
    // _pinConfig.pinNr = UM_PLUGIN_DEMO_RELAY_PIN;
    _pinConfig[0].pinNr = UM_PLUGIN_DEMO_RELAY_PIN;
    if (pluginManager.registerPinUser(*this, _pinConfig, _name))
    {
      pinMode(_pinConfig[0].pinNr, OUTPUT);
    }
  }

  // ----- internal processing functions -----

  /// Just an example for custom plugin sensor data processing.
  void processFanControl()
  {
    // quit if we didn't get a GPIO
    if (!_pinConfig[0].isPinValid())
      return;

    // try to get a TemperatureSensor - and quit if that fails
    TemperatureSensor *tempSensor = pluginManager.getTemperatureSensor();
    if (!tempSensor || !tempSensor->isReady())
      return;

    // read the temperature value from the other plugin
    const float temp = tempSensor->temperatureC();
    // and control a connected fan via relay
    const bool isHot = temp > 20.0f;
    digitalWrite(_pinConfig[0].pinNr, isHot ? HIGH : LOW);
  }

  // ----- member variables -----

  PinConfigs<12> _pinConfig{{{PinType::Digital_out, "Relay"},
                             // the following are just demo examples:
                             {PinType::Digital_in},
                             {PinType::Digital_out},
                             {PinType::PWM_out},
                             {PinType::I2C_scl},
                             {PinType::I2C_sda},
                             {PinType::OneWire},
                             {PinType::OneWire, "1W-aux"}, // 2nd bus with different name
                             {PinType::SPI_miso},
                             {PinType::SPI_mosi},
                             {PinType::SPI_sclk},
                             {PinType::Analog_in}}};
  // how a single pin configuration would look like:
  // PinConfig _pinConfig{PinType::Digital_out, "Relay"};
};

//--------------------------------------------------------------------------------------------------

static UM_PluginDemo um_PluginDemo;
REGISTER_USERMOD(um_PluginDemo);
