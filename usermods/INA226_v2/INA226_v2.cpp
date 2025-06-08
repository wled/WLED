#include "wled.h"
#include <INA226_WE.h>

#define INA226_ADDRESS 0x40 // Default I2C address for INA226

#define DEFAULT_CHECKINTERVAL 60000
#define DEFAULT_INASAMPLES 128
#define DEFAULT_INASAMPLESENUM AVERAGE_128
#define DEFAULT_INACONVERSIONTIME 1100
#define DEFAULT_INACONVERSIONTIMEENUM CONV_TIME_1100

// A packed version of all INA settings enums and their human friendly counterparts packed into a 32 bit structure
// Some values are shifted and need to be preprocessed before usage
struct InaSettingLookup
{
    uint16_t avgSamples : 11;          // Max 1024, which could be in 10 bits if we shifted by 1; if we somehow handle the edge case with "1"
    uint8_t avgEnum : 4;               // Shift by 8 to get the INA226_AVERAGES value, accepts 0x00 to 0x0F, we need 0x00 to 0x0E
    uint16_t convTimeUs : 14;          // We could save 2 bits by shifting this, but we won't save anything at present.
    INA226_CONV_TIME convTimeEnum : 3; // Only the lowest 3 bits are defined in the conversion time enumerations
};

const InaSettingLookup _inaSettingsLookup[] = {
    {1024, AVERAGE_1024 >> 8, 8244, CONV_TIME_8244},
    {512, AVERAGE_512 >> 8, 4156, CONV_TIME_4156},
    {256, AVERAGE_256 >> 8, 2116, CONV_TIME_2116},
    {128, AVERAGE_128 >> 8, 1100, CONV_TIME_1100},
    {64, AVERAGE_64 >> 8, 588, CONV_TIME_588},
    {16, AVERAGE_16 >> 8, 332, CONV_TIME_332},
    {4, AVERAGE_4 >> 8, 204, CONV_TIME_204},
    {1, AVERAGE_1 >> 8, 140, CONV_TIME_140}};

// Note: Will update the provided arg to be the correct value
INA226_AVERAGES getAverageEnum(uint16_t &samples)
{
    for (const auto &setting : _inaSettingsLookup)
    {
        // If a user supplies 2000 samples, we serve up the highest possible value
        if (samples >= setting.avgSamples)
        {
            samples = setting.avgSamples;
            return static_cast<INA226_AVERAGES>(setting.avgEnum << 8);
        }
    }
    // Default value if not found
    samples = DEFAULT_INASAMPLES;
    return DEFAULT_INASAMPLESENUM;
}

INA226_CONV_TIME getConversionTimeEnum(uint16_t &timeUs)
{
    for (const auto &setting : _inaSettingsLookup)
    {
        // If a user supplies 9000 μs, we serve up the highest possible value
        if (timeUs >= setting.convTimeUs)
        {
            timeUs = setting.convTimeUs;
            return setting.convTimeEnum;
        }
    }
    // Default value if not found
    timeUs = DEFAULT_INACONVERSIONTIME;
    return DEFAULT_INACONVERSIONTIMEENUM;
}

class UsermodINA226 : public Usermod
{
private:
    static const char _name[];

    unsigned long _lastLoopCheck = 0;
    unsigned long _lastTriggerTime = 0;

    bool _settingEnabled : 1;                  // Enable the usermod
    bool _mqttPublish : 1;                     // Publish MQTT values
    bool _mqttPublishAlways : 1;               // Publish always, regardless if there is a change
    bool _mqttHomeAssistant : 1;               // Enable Home Assistant docs
    bool _initDone : 1;                        // Initialization is done
    bool _isTriggeredOperationMode : 1;        // false = continuous, true = triggered
    bool _measurementTriggered : 1;            // if triggered mode, then true indicates we're waiting for measurements
    uint16_t _settingInaConversionTimeUs : 12; // Conversion time, shift by 2
    uint16_t _settingInaSamples : 11;          // Number of samples for averaging, max 1024

    uint8_t _i2cAddress;
    uint16_t _checkInterval; // milliseconds, user settings is in seconds
    float _decimalFactor;    // a power of 10 factor. 1 would be no change, 10 is one decimal, 100 is two etc. User sees a power of 10 (0, 1, 2, ..)
    uint16_t _shuntResistor; // Shunt resistor value in milliohms
    uint16_t _currentRange;  // Expected maximum current in milliamps

    uint8_t _lastStatus = 0;
    float _lastCurrent = 0;
    float _lastVoltage = 0;
    float _lastPower = 0;
    float _lastShuntVoltage = 0;
    bool _lastOverflow = false;

#ifndef WLED_MQTT_DISABLE
    float _lastCurrentSent = 0;
    float _lastVoltageSent = 0;
    float _lastPowerSent = 0;
    float _lastShuntVoltageSent = 0;
    bool _lastOverflowSent = false;
#endif

    INA226_WE *_ina226 = nullptr;

    float truncateDecimals(float val)
    {
        return roundf(val * _decimalFactor) / _decimalFactor;
    }

    void initializeINA226()
    {
        if (_ina226 != nullptr)
        {
            delete _ina226;
        }

        _ina226 = new INA226_WE(_i2cAddress);
        if (!_ina226->init())
        {
            DEBUG_PRINTLN(F("INA226 initialization failed!"));
            return;
        }
        _ina226->setCorrectionFactor(1.0);

        uint16_t tmpShort = _settingInaSamples;
        _ina226->setAverage(getAverageEnum(tmpShort));

        tmpShort = _settingInaConversionTimeUs << 2;
        _ina226->setConversionTime(getConversionTimeEnum(tmpShort));

        if (_checkInterval >= 20000)
        {
            _isTriggeredOperationMode = true;
            _ina226->setMeasureMode(TRIGGERED);
        }
        else
        {
            _isTriggeredOperationMode = false;
            _ina226->setMeasureMode(CONTINUOUS);
        }

        _ina226->setResistorRange(static_cast<float>(_shuntResistor) / 1000.0, static_cast<float>(_currentRange) / 1000.0);
    }

    void fetchAndPushValues()
    {
        _lastStatus = _ina226->getI2cErrorCode();

        if (_lastStatus != 0)
            return;

        float current = truncateDecimals(_ina226->getCurrent_mA() / 1000.0);
        float voltage = truncateDecimals(_ina226->getBusVoltage_V());
        float power = truncateDecimals(_ina226->getBusPower() / 1000.0);
        float shuntVoltage = truncateDecimals(_ina226->getShuntVoltage_V());
        bool overflow = _ina226->overflow;

#ifndef WLED_DISABLE_MQTT
        mqttPublishIfChanged(F("current"), _lastCurrentSent, current, 0.01f);
        mqttPublishIfChanged(F("voltage"), _lastVoltageSent, voltage, 0.01f);
        mqttPublishIfChanged(F("power"), _lastPowerSent, power, 0.1f);
        mqttPublishIfChanged(F("shunt_voltage"), _lastShuntVoltageSent, shuntVoltage, 0.01f);
        mqttPublishIfChanged(F("overflow"), _lastOverflowSent, overflow);
#endif

        _lastCurrent = current;
        _lastVoltage = voltage;
        _lastPower = power;
        _lastShuntVoltage = shuntVoltage;
        _lastOverflow = overflow;
    }

    void handleTriggeredMode(unsigned long currentTime)
    {
        if (_measurementTriggered)
        {
            // Test if we have a measurement every 400ms
            if (currentTime - _lastTriggerTime >= 400)
            {
                _lastTriggerTime = currentTime;
                if (_ina226->isBusy())
                    return;

                fetchAndPushValues();
                _measurementTriggered = false;
            }
        }
        else
        {
            if (currentTime - _lastLoopCheck >= _checkInterval)
            {
                // Start a measurement and use isBusy() later to determine when it is done
                _ina226->startSingleMeasurementNoWait();
                _lastLoopCheck = currentTime;
                _lastTriggerTime = currentTime;
                _measurementTriggered = true;
            }
        }
    }

    void handleContinuousMode(unsigned long currentTime)
    {
        if (currentTime - _lastLoopCheck >= _checkInterval)
        {
            _lastLoopCheck = currentTime;
            fetchAndPushValues();
        }
    }

#ifndef WLED_DISABLE_MQTT
    void mqttInitialize()
    {
        if (!WLED_MQTT_CONNECTED || !_mqttPublish || !_mqttHomeAssistant)
            return;

        char topic[128];
        snprintf_P(topic, 127, "%s/current", mqttDeviceTopic);
        mqttCreateHassSensor(F("Current"), topic, F("current"), F("A"));

        snprintf_P(topic, 127, "%s/voltage", mqttDeviceTopic);
        mqttCreateHassSensor(F("Voltage"), topic, F("voltage"), F("V"));

        snprintf_P(topic, 127, "%s/power", mqttDeviceTopic);
        mqttCreateHassSensor(F("Power"), topic, F("power"), F("W"));

        snprintf_P(topic, 127, "%s/shunt_voltage", mqttDeviceTopic);
        mqttCreateHassSensor(F("Shunt Voltage"), topic, F("voltage"), F("V"));

        snprintf_P(topic, 127, "%s/overflow", mqttDeviceTopic);
        mqttCreateHassBinarySensor(F("Overflow"), topic);
    }

    void mqttPublishIfChanged(const __FlashStringHelper *topic, float &lastState, float state, float minChange)
    {
        if (WLED_MQTT_CONNECTED && _mqttPublish && (_mqttPublishAlways || fabsf(lastState - state) > minChange))
        {
            char subuf[128];
            snprintf_P(subuf, 127, PSTR("%s/%s"), mqttDeviceTopic, (const char *)topic);
            mqtt->publish(subuf, 0, false, String(state).c_str());

            lastState = state;
        }
    }

    void mqttPublishIfChanged(const __FlashStringHelper *topic, bool &lastState, bool state)
    {
        if (WLED_MQTT_CONNECTED && _mqttPublish && (_mqttPublishAlways || lastState != state))
        {
            char subuf[128];
            snprintf_P(subuf, 127, PSTR("%s/%s"), mqttDeviceTopic, (const char *)topic);
            mqtt->publish(subuf, 0, false, state ? "true" : "false");

            lastState = state;
        }
    }

    void mqttCreateHassSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement)
    {
        String t = String(F("homeassistant/sensor/")) + mqttClientID + "/" + name + F("/config");

        StaticJsonDocument<600> doc;

        doc[F("name")] = name;
        doc[F("state_topic")] = topic;
        doc[F("unique_id")] = String(mqttClientID) + name;
        if (unitOfMeasurement != "")
            doc[F("unit_of_measurement")] = unitOfMeasurement;
        if (deviceClass != "")
            doc[F("device_class")] = deviceClass;
        doc[F("expire_after")] = 1800;

        JsonObject device = doc.createNestedObject(F("device"));
        device[F("name")] = serverDescription;
        device[F("identifiers")] = "wled-sensor-" + String(mqttClientID);
        device[F("manufacturer")] = F(WLED_BRAND);
        device[F("model")] = F(WLED_PRODUCT_NAME);
        device[F("sw_version")] = versionString;

        String temp;
        serializeJson(doc, temp);
        DEBUG_PRINTLN(t);
        DEBUG_PRINTLN(temp);

        mqtt->publish(t.c_str(), 0, true, temp.c_str());
    }

    void mqttCreateHassBinarySensor(const String &name, const String &topic)
    {
        String t = String(F("homeassistant/binary_sensor/")) + mqttClientID + "/" + name + F("/config");

        StaticJsonDocument<600> doc;

        doc[F("name")] = name;
        doc[F("state_topic")] = topic;
        doc[F("unique_id")] = String(mqttClientID) + name;

        JsonObject device = doc.createNestedObject(F("device"));
        device[F("name")] = serverDescription;
        device[F("identifiers")] = "wled-sensor-" + String(mqttClientID);
        device[F("manufacturer")] = F(WLED_BRAND);
        device[F("model")] = F(WLED_PRODUCT_NAME);
        device[F("sw_version")] = versionString;

        String temp;
        serializeJson(doc, temp);
        DEBUG_PRINTLN(t);
        DEBUG_PRINTLN(temp);

        mqtt->publish(t.c_str(), 0, true, temp.c_str());
    }
#endif

public:
    UsermodINA226()
    {
        // Default values
        _settingInaSamples = DEFAULT_INASAMPLES;
        _settingInaConversionTimeUs = DEFAULT_INACONVERSIONTIME;

        _i2cAddress = INA226_ADDRESS;
        _checkInterval = DEFAULT_CHECKINTERVAL;
        _decimalFactor = 100;
        _shuntResistor = 1000;
        _currentRange = 1000;
    }

    void setup()
    {
        initializeINA226();
    }

    void loop()
    {
        if (!_settingEnabled || strip.isUpdating())
            return;

        unsigned long currentTime = millis();

        if (_isTriggeredOperationMode)
        {
            handleTriggeredMode(currentTime);
        }
        else
        {
            handleContinuousMode(currentTime);
        }
    }

#ifndef WLED_DISABLE_MQTT
    void onMqttConnect(bool sessionPresent)
    {
        mqttInitialize();
    }
#endif

    uint16_t getId()
    {
        return USERMOD_ID_INA226;
    }

    void addToJsonInfo(JsonObject &root) override
    {
        JsonObject user = root["u"];
        if (user.isNull())
            user = root.createNestedObject("u");

#ifdef USERMOD_INA226_DEBUG
        JsonArray temp = user.createNestedArray(F("INA226 last loop"));
        temp.add(_lastLoopCheck);

        temp = user.createNestedArray(F("INA226 last status"));
        temp.add(_lastStatus);

        temp = user.createNestedArray(F("INA226 average samples"));
        temp.add(_settingInaSamples);
        temp.add(F("samples"));

        temp = user.createNestedArray(F("INA226 conversion time"));
        temp.add(_settingInaConversionTimeUs << 2);
        temp.add(F("μs"));

        // INA226 uses (2 * conversion time * samples) time to take a reading.
        temp = user.createNestedArray(F("INA226 expected sample time"));
        uint32_t sampleTimeNeededUs = (static_cast<uint32_t>(_settingInaConversionTimeUs) << 2) * _settingInaSamples * 2;
        temp.add(truncateDecimals(sampleTimeNeededUs / 1000.0));
        temp.add(F("ms"));

        temp = user.createNestedArray(F("INA226 mode"));
        temp.add(_isTriggeredOperationMode ? F("triggered") : F("continuous"));

        if (_isTriggeredOperationMode)
        {
            temp = user.createNestedArray(F("INA226 triggered"));
            temp.add(_measurementTriggered ? F("waiting for measurement") : F(""));
        }
#endif

        JsonArray jsonCurrent = user.createNestedArray(F("Current"));
        JsonArray jsonVoltage = user.createNestedArray(F("Voltage"));
        JsonArray jsonPower = user.createNestedArray(F("Power"));
        JsonArray jsonShuntVoltage = user.createNestedArray(F("Shunt Voltage"));
        JsonArray jsonOverflow = user.createNestedArray(F("Overflow"));

        if (_lastLoopCheck == 0)
        {
            jsonCurrent.add(F("Not read yet"));
            jsonVoltage.add(F("Not read yet"));
            jsonPower.add(F("Not read yet"));
            jsonShuntVoltage.add(F("Not read yet"));
            jsonOverflow.add(F("Not read yet"));
            return;
        }

        if (_lastStatus != 0)
        {
            jsonCurrent.add(F("An error occurred"));
            jsonVoltage.add(F("An error occurred"));
            jsonPower.add(F("An error occurred"));
            jsonShuntVoltage.add(F("An error occurred"));
            jsonOverflow.add(F("An error occurred"));
            return;
        }

        jsonCurrent.add(_lastCurrent);
        jsonCurrent.add(F("A"));

        jsonVoltage.add(_lastVoltage);
        jsonVoltage.add(F("V"));

        jsonPower.add(_lastPower);
        jsonPower.add(F("W"));

        jsonShuntVoltage.add(_lastShuntVoltage);
        jsonShuntVoltage.add(F("V"));

        jsonOverflow.add(_lastOverflow ? F("true") : F("false"));
    }

    void addToConfig(JsonObject &root)
    {
        JsonObject top = root.createNestedObject(FPSTR(_name));
        top[F("Enabled")] = _settingEnabled;
        top[F("I2CAddress")] = static_cast<uint8_t>(_i2cAddress);
        top[F("CheckInterval")] = _checkInterval / 1000;
        top[F("INASamples")] = _settingInaSamples;
        top[F("INAConversionTime")] = _settingInaConversionTimeUs << 2;
        top[F("Decimals")] = log10f(_decimalFactor);
        top[F("ShuntResistor")] = _shuntResistor;
        top[F("CurrentRange")] = _currentRange;
#ifndef WLED_DISABLE_MQTT
        top[F("MqttPublish")] = _mqttPublish;
        top[F("MqttPublishAlways")] = _mqttPublishAlways;
        top[F("MqttHomeAssistantDiscovery")] = _mqttHomeAssistant;
#endif

        DEBUG_PRINTLN(F("INA226 config saved."));
    }

    bool readFromConfig(JsonObject &root) override
    {
        JsonObject top = root[FPSTR(_name)];

        bool configComplete = !top.isNull();
        if (!configComplete)
            return false;

        bool tmpBool;
        if (getJsonValue(top[F("Enabled")], tmpBool))
            _settingEnabled = tmpBool;
        else
            configComplete = false;

        configComplete &= getJsonValue(top[F("I2CAddress")], _i2cAddress);
        if (getJsonValue(top[F("CheckInterval")], _checkInterval))
        {
            if (1 <= _checkInterval && _checkInterval <= 600)
                _checkInterval *= 1000;
            else
                _checkInterval = DEFAULT_CHECKINTERVAL;
        }
        else
            configComplete = false;

        uint16_t tmpShort;
        if (getJsonValue(top[F("INASamples")], tmpShort))
        {
            // The method below will fix the provided value to a valid one
            getAverageEnum(tmpShort);
            _settingInaSamples = tmpShort;
        }
        else
            configComplete = false;

        if (getJsonValue(top[F("INAConversionTime")], tmpShort))
        {
            // The method below will fix the provided value to a valid one
            getConversionTimeEnum(tmpShort);
            _settingInaConversionTimeUs = tmpShort >> 2;
        }
        else
            configComplete = false;

        if (getJsonValue(top[F("Decimals")], _decimalFactor))
        {
            if (0 <= _decimalFactor && _decimalFactor <= 5)
                _decimalFactor = pow10f(_decimalFactor);
            else
                _decimalFactor = 100;
        }
        else
            configComplete = false;

        configComplete &= getJsonValue(top[F("ShuntResistor")], _shuntResistor);
        configComplete &= getJsonValue(top[F("CurrentRange")], _currentRange);

#ifndef WLED_DISABLE_MQTT
        if (getJsonValue(top[F("MqttPublish")], tmpBool))
            _mqttPublish = tmpBool;
        else
            configComplete = false;

        if (getJsonValue(top[F("MqttPublishAlways")], tmpBool))
            _mqttPublishAlways = tmpBool;
        else
            configComplete = false;

        if (getJsonValue(top[F("MqttHomeAssistantDiscovery")], tmpBool))
            _mqttHomeAssistant = tmpBool;
        else
            configComplete = false;
#endif

        if (_initDone)
        {
            initializeINA226();

#ifndef WLED_DISABLE_MQTT
            mqttInitialize();
#endif
        }

        _initDone = true;
        return configComplete;
    }

    ~UsermodINA226()
    {
        delete _ina226;
        _ina226 = nullptr;
    }

};

const char UsermodINA226::_name[] PROGMEM = "INA226";


static UsermodINA226 ina226_v2;
REGISTER_USERMOD(ina226_v2);