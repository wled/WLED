#include "wled.h"
#include "SN_Photoresistor.h"

//Pin defaults for QuinLed Dig-Uno (A0)
#ifndef PHOTORESISTOR_PIN
#define PHOTORESISTOR_PIN A0
#endif

static bool checkBoundSensor(float newValue, float prevValue, float maxDiff)
{
  return isnan(prevValue) || newValue <= prevValue - maxDiff || newValue >= prevValue + maxDiff;
}

uint16_t Usermod_SN_Photoresistor::getLuminance()
{
  // http://forum.arduino.cc/index.php?topic=37555.0
  // https://forum.arduino.cc/index.php?topic=185158.0
  float volts = analogRead(PHOTORESISTOR_PIN) * (referenceVoltage / adcPrecision);
  float amps = volts / resistorValue;
  float lux = amps * 1000000 * 2.0;

  lastMeasurement = millis();
  getLuminanceComplete = true;
  return uint16_t(lux);
}

void Usermod_SN_Photoresistor::setup()
{
  // set pinmode
  pinMode(PHOTORESISTOR_PIN, INPUT);
}

void Usermod_SN_Photoresistor::loop()
{
  if (disabled || strip.isUpdating())
    return;

  unsigned long now = millis();

  // check to see if we are due for taking a measurement
  // lastMeasurement will not be updated until the conversion
  // is complete the the reading is finished
  if (now - lastMeasurement < readingInterval)
  {
    return;
  }

  uint16_t currentLDRValue = getLuminance();
  if (checkBoundSensor(currentLDRValue, lastLDRValue, offset))
  {
    lastLDRValue = currentLDRValue;

#ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED)
    {
      char subuf[45];
      strcpy(subuf, mqttDeviceTopic);
      strcat_P(subuf, PSTR("/luminance"));
      mqtt->publish(subuf, 0, true, String(lastLDRValue).c_str());
    }
    else
    {
      DEBUG_PRINTLN(F("Missing MQTT connection. Not publishing data"));
    }
  }
#endif
}


void Usermod_SN_Photoresistor::addToJsonInfo(JsonObject &root)
{
  JsonObject user = root[F("u")];
  if (user.isNull())
    user = root.createNestedObject(F("u"));

  JsonArray lux = user.createNestedArray(F("Luminance"));

  if (!getLuminanceComplete)
  {
    // if we haven't read the sensor yet, let the user know
    // that we are still waiting for the first measurement
    lux.add((USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT - millis()) / 1000);
    lux.add(F(" sec until read"));
    return;
  }

  lux.add(lastLDRValue);
  lux.add(F(" lux"));
}


/**
   * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
   */
void Usermod_SN_Photoresistor::addToConfig(JsonObject &root)
{
  // we add JSON object.
  JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
  top[FPSTR(_enabled)] = !disabled;
  top[FPSTR(_readInterval)] = readingInterval / 1000;
  top[FPSTR(_referenceVoltage)] = referenceVoltage;
  top[FPSTR(_resistorValue)] = resistorValue;
  top[FPSTR(_adcPrecision)] = adcPrecision;
  top[FPSTR(_offset)] = offset;

  DEBUG_PRINTLN(F("Photoresistor config saved."));
}

/**
* readFromConfig() is called before setup() to populate properties from values stored in cfg.json
*/
bool Usermod_SN_Photoresistor::readFromConfig(JsonObject &root)
{
  // we look for JSON object.
  JsonObject top = root[FPSTR(_name)];
  if (top.isNull()) {
    DEBUG_PRINT(FPSTR(_name));
    DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
    return false;
  }

  disabled         = !(top[FPSTR(_enabled)] | !disabled);
  readingInterval  = (top[FPSTR(_readInterval)] | readingInterval/1000) * 1000; // convert to ms
  referenceVoltage = top[FPSTR(_referenceVoltage)] | referenceVoltage;
  resistorValue    = top[FPSTR(_resistorValue)] | resistorValue;
  adcPrecision     = top[FPSTR(_adcPrecision)] | adcPrecision;
  offset           = top[FPSTR(_offset)] | offset;
  DEBUG_PRINT(FPSTR(_name));
  DEBUG_PRINTLN(F(" config (re)loaded."));

  // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
  return true;
}


// strings to reduce flash memory usage (used more than twice)
const char Usermod_SN_Photoresistor::_name[] PROGMEM = "Photoresistor";
const char Usermod_SN_Photoresistor::_enabled[] PROGMEM = "enabled";
const char Usermod_SN_Photoresistor::_readInterval[] PROGMEM = "read-interval-s";
const char Usermod_SN_Photoresistor::_referenceVoltage[] PROGMEM = "supplied-voltage";
const char Usermod_SN_Photoresistor::_resistorValue[] PROGMEM = "resistor-value";
const char Usermod_SN_Photoresistor::_adcPrecision[] PROGMEM = "adc-precision";
const char Usermod_SN_Photoresistor::_offset[] PROGMEM = "offset";

static Usermod_SN_Photoresistor sn_photoresistor;
REGISTER_USERMOD(sn_photoresistor);