// Configurable settings for the INA2xx Usermod

// logging macro:
#define _logUsermodInaSensor(fmt, ...) \
	DEBUG_PRINTF("[INA2xx_Sensor] " fmt "\n", ##__VA_ARGS__)

// Enabled setting
#ifndef INA219_ENABLED
	#define INA219_ENABLED false // Default disabled value
#endif

#ifndef INA219_I2C_ADDRESS
	#define INA219_I2C_ADDRESS 0x40 // Default I2C address
#endif

#ifndef INA219_CHECK_INTERVAL
	#define INA219_CHECK_INTERVAL 5 // Default 5 seconds (will be converted to ms)
#endif

#ifndef INA219_CONVERSION_TIME
	#define INA219_CONVERSION_TIME BIT_MODE_12 // Conversion Time, Default 12-bit resolution
#endif

#ifndef INA219_DECIMAL_FACTOR
	#define INA219_DECIMAL_FACTOR 3 // Decimal factor for current/power readings. Default 3 decimal places
#endif

#ifndef INA219_SHUNT_RESISTOR
	#define INA219_SHUNT_RESISTOR 0.1 // Shunt Resistor value. Default 0.1 ohms
#endif

#ifndef INA219_CORRECTION_FACTOR
	#define INA219_CORRECTION_FACTOR 1.0 // Default correction factor. Default 1.0
#endif

#ifndef INA219_P_GAIN
	#define INA219_P_GAIN PG_320 // PG_40, PG_80, PG_160, PG_320
#endif

#ifndef INA219_BUSRANGE
	#define INA219_BUSRANGE BRNG_32 // BRNG_16, BRNG_32
#endif

#ifndef INA219_SHUNT_VOLT_OFFSET
	#define INA219_SHUNT_VOLT_OFFSET 0.0 // mV offset at zero current
#endif

#ifndef INA219_MQTT_PUBLISH
	#define INA219_MQTT_PUBLISH false // Default: do not publish to MQTT
#endif

#ifndef INA219_MQTT_PUBLISH_ALWAYS
	#define INA219_MQTT_PUBLISH_ALWAYS false // Default: only publish on change
#endif

#ifndef INA219_HA_DISCOVERY
	#define INA219_HA_DISCOVERY false // Default: Home Assistant discovery disabled
#endif

#include "wled.h"
#include <INA219_WE.h>

#define UPDATE_CONFIG(obj, key, var, fmt)                     \
  do {                                                         \
    auto _tmp = var;                                           \
    if ( getJsonValue((obj)[(key)], _tmp) ) {                  \
      if (_tmp != var) {                                       \
        _logUsermodInaSensor("%s updated to: " fmt, key, _tmp);\
        var = _tmp;                                            \
      }                                                        \
    } else {                                                   \
      configComplete = false;                                  \
    }                                                          \
  } while(0)

class UsermodINA2xx : public Usermod {
private:
	static const char _name[];  // Name of the usermod

	bool initDone = false;  // Flag for successful initialization
	unsigned long lastCheck = 0; // Timestamp for the last check
	bool alreadyLoggedDisabled = false;

	// Define the variables using the pre-defined or default values
	bool enabled = INA219_ENABLED;
	uint8_t _i2cAddress = INA219_I2C_ADDRESS;
	uint16_t _checkInterval = INA219_CHECK_INTERVAL; // seconds
	uint32_t checkInterval = static_cast<uint32_t>(_checkInterval) * 1000UL; // ms
	INA219_ADC_MODE conversionTime = static_cast<INA219_ADC_MODE>(INA219_CONVERSION_TIME);
	uint8_t _decimalFactor = INA219_DECIMAL_FACTOR;
	float shuntResistor = INA219_SHUNT_RESISTOR;
	float correctionFactor = INA219_CORRECTION_FACTOR;
	INA219_PGAIN pGain            = static_cast<INA219_PGAIN>(INA219_P_GAIN);
	INA219_BUS_RANGE busRange     = static_cast<INA219_BUS_RANGE>(INA219_BUSRANGE);
	float shuntVoltOffset_mV      = INA219_SHUNT_VOLT_OFFSET;
	bool mqttPublish = INA219_MQTT_PUBLISH;
	bool mqttPublishSent = !INA219_MQTT_PUBLISH;
	bool mqttPublishAlways = INA219_MQTT_PUBLISH_ALWAYS;
	bool haDiscovery = INA219_HA_DISCOVERY;
	bool haDiscoverySent = !INA219_HA_DISCOVERY;

	// Variables to store sensor readings
	float busVoltage = 0;
	float current = 0;
	float current_mA = 0;
	float power = 0;
	float power_mW = 0;
	float shuntVoltage = 0;
	float loadVoltage = 0;
	bool overflow = false;

	// Last sent variables
	float last_sent_shuntVoltage = 0;
	float last_sent_busVoltage = 0;
	float last_sent_loadVoltage = 0;
	float last_sent_current = 0;
	float last_sent_current_mA = 0;
	float last_sent_power = 0;
	float last_sent_power_mW = 0;
	bool last_sent_overflow = false;

	float dailyEnergy_kWh = 0.0; // Daily energy in kWh
	float monthlyEnergy_kWh = 0.0; // Monthly energy in kWh
	float totalEnergy_kWh = 0.0; // Total energy in kWh
	unsigned long lastPublishTime = 0; // Track the last publish time

	// Variables to store last reset timestamps
	unsigned long dailyResetTime = 0;
	unsigned long monthlyResetTime = 0;

	bool mqttStateRestored = false;

	INA219_WE *_ina2xx = nullptr; // INA2xx sensor object

	// Function to truncate decimals based on the configured decimal factor
	float roundDecimals(float val) {
		_logUsermodInaSensor("Truncating value %.6f with factor %d", val, _decimalFactor);
		if (_decimalFactor == 0) {
			return roundf(val);
		}

		static const float factorLUT[4] = {1.f, 10.f, 100.f, 1000.f};
		float factor = (_decimalFactor <= 3) ? factorLUT[_decimalFactor]
										: powf(10.0f, _decimalFactor);

		return roundf(val * factor) / factor;
	}

	bool hasSignificantChange(float oldValue, float newValue, float threshold = 0.01f) {
		bool changed = fabsf(oldValue - newValue) > threshold;
		if (changed) {
			_logUsermodInaSensor("Significant change detected: old=%.6f, new=%.6f, diff=%.6f", oldValue, newValue, fabsf(oldValue - newValue));
		}
		return changed;
	}

	bool hasValueChanged() {
		bool changed = hasSignificantChange(last_sent_shuntVoltage, shuntVoltage) ||
			hasSignificantChange(last_sent_busVoltage, busVoltage) ||
			hasSignificantChange(last_sent_loadVoltage, loadVoltage) ||
			hasSignificantChange(last_sent_current, current) ||
			hasSignificantChange(last_sent_current_mA, current_mA) ||
			hasSignificantChange(last_sent_power, power) ||
			hasSignificantChange(last_sent_power_mW, power_mW) ||
			(last_sent_overflow != overflow);

			if (changed) {
				_logUsermodInaSensor("Values changed, need to publish");
			}
			return changed;
	}

	// Update INA2xx settings and reinitialize sensor if necessary
	bool updateINA2xxSettings() {
		_logUsermodInaSensor("Updating INA2xx sensor settings");

		// Validate I2C pins; if invalid, disable usermod and log message
		if (i2c_scl < 0 || i2c_sda < 0) {
			enabled = false;
			_logUsermodInaSensor("INA2xx disabled: Invalid I2C pins. Check global I2C settings.");
			return false;
		}
		_logUsermodInaSensor("Using I2C SDA: %d", i2c_sda);
		_logUsermodInaSensor("Using I2C SCL: %d", i2c_scl);

		// Reinitialize the INA2xx instance with updated settings
		if (_ina2xx != nullptr) {
			_logUsermodInaSensor("Freeing existing INA2xx instance");
			delete _ina2xx;
			_ina2xx = nullptr;
		}

		if (!enabled) return true;

		_logUsermodInaSensor("Creating new INA2xx instance with address 0x%02X", _i2cAddress);
		_ina2xx = new INA219_WE(_i2cAddress);

		if (!_ina2xx) {
			_logUsermodInaSensor("Failed to allocate memory for INA2xx sensor!");
			enabled = false;
			return false;
		}

		_logUsermodInaSensor("Initializing INA2xx sensor");
		if (!_ina2xx->init()) {
			_logUsermodInaSensor("INA2xx initialization failed!");
			enabled = false;
			delete _ina2xx;
			_ina2xx = nullptr;
			return false;
		}

		_logUsermodInaSensor("Setting shunt resistor to %.4f Ohms", shuntResistor);
		_ina2xx->setShuntSizeInOhms(shuntResistor);

		_logUsermodInaSensor("Setting ADC mode to %d", conversionTime);
		_ina2xx->setADCMode(conversionTime);

		_logUsermodInaSensor("Setting correction factor to %.4f", correctionFactor);
		_ina2xx->setCorrectionFactor(correctionFactor);

		_logUsermodInaSensor("Setting PGA gain to %d", pGain);
		_ina2xx->setPGain(pGain);

		_logUsermodInaSensor("Setting bus range to %d", busRange);
		_ina2xx->setBusRange(busRange);

		_logUsermodInaSensor("Setting shunt voltage offset to %.3f mV", shuntVoltOffset_mV);
		_ina2xx->setShuntVoltOffset_mV(shuntVoltOffset_mV);

		_logUsermodInaSensor("INA2xx sensor configured successfully");

		return true;
	}

	// Sanitize the mqttClientID by replacing invalid characters.
	String sanitizeMqttClientID(const String &clientID) {
		String sanitizedID;
		_logUsermodInaSensor("Sanitizing MQTT client ID: %s", clientID.c_str());

		for (unsigned int i = 0; i < clientID.length(); i++) {
			char c = clientID[i];

			// Handle common accented characters using simple mapping
			if (c == '\xC3' && i + 1 < clientID.length()) {
				char next = clientID[i + 1];
				if (next == '\xBC' || next == '\x9C') { // ü or Ü
					sanitizedID += (next == '\xBC' ? "u" : "U");
					i++;
					continue;
				} else if (next == '\xA4' || next == '\xC4') { // ä or Ä
					sanitizedID += (next == '\xA4' ? "a" : "A");
					i++;
					continue;
				} else if (next == '\xB6' || next == '\xD6') { // ö or Ö
					sanitizedID += (next == '\xB6' ? "o" : "O");
					i++;
					continue;
				} else if (next == '\x9F') { // ß
					sanitizedID += "s";
					i++;
					continue;
				}
			}
			// Allow valid characters [a-zA-Z0-9_-]
			if ((c >= 'a' && c <= 'z') ||
					(c >= 'A' && c <= 'Z') ||
					(c >= '0' && c <= '9') ||
					c == '-' || c == '_') {
				sanitizedID += c;
			} else { // Replace any other invalid character with an underscore
				sanitizedID += '_';
			}
		}
		_logUsermodInaSensor("Sanitized MQTT client ID: %s", sanitizedID.c_str());
		return sanitizedID;
	}

	/**
	** Function to update energy calculations based on power and duration
	**/
	void updateEnergy(float power, unsigned long durationMs) {
		float durationHours = durationMs / 3600000.0; // Milliseconds to hours
		_logUsermodInaSensor("Updating energy - Power: %.3f W, Duration: %lu ms (%.6f hours)", power, durationMs, durationHours);

		// Skip negligible power values to avoid accumulating rounding errors
		if (power < 0.01) {
			_logUsermodInaSensor("SKIPPED: Power too low (%.3f W) — skipping to avoid rounding errors.", power);
			return;
		}

		float energy_kWh = (power / 1000.0) * durationHours; // Watts to kilowatt-hours (kWh)
		_logUsermodInaSensor("Calculated energy: %.6f kWh", energy_kWh);

		// Skip negative values or unrealistic spikes
		if (energy_kWh < 0 || energy_kWh > 10.0) { // 10 kWh in a few seconds is unrealistic
			_logUsermodInaSensor("SKIPPED: Energy value out of range (%.6f kWh)", energy_kWh);
			return;
		}

		totalEnergy_kWh += energy_kWh; // Update total energy consumed
		_logUsermodInaSensor("Total energy updated to: %.6f kWh", totalEnergy_kWh);

		// Calculate day identifier (days since epoch)
		long currentDay = localTime / 86400;
		_logUsermodInaSensor("Current day: %ld, Last reset day: %lu", currentDay, dailyResetTime);

		// Reset daily energy at midnight or if day changed
		if ((hour(localTime) == 0 && minute(localTime) == 0 && dailyResetTime != currentDay) ||
			(currentDay > dailyResetTime && dailyResetTime > 0)) {
			_logUsermodInaSensor("Resetting daily energy counter (day change detected)");
			dailyEnergy_kWh = 0;
			dailyResetTime = currentDay;
		}
		dailyEnergy_kWh += energy_kWh;
		_logUsermodInaSensor("Daily energy updated to: %.6f kWh", dailyEnergy_kWh);

		// Calculate month identifier (year*12 + month)
		long currentMonth = year(localTime) * 12 + month(localTime) - 1; // month() is 1-12
		_logUsermodInaSensor("Current month: %ld, Last reset month: %lu", currentMonth, monthlyResetTime);
		
		// Reset monthly energy on first day of month or if month changed
		if ((day(localTime) == 1 && hour(localTime) == 0 && minute(localTime) == 0 &&
			 monthlyResetTime != currentMonth) || (currentMonth > monthlyResetTime && monthlyResetTime > 0)) {
			_logUsermodInaSensor("Resetting monthly energy counter (month change detected)");
			monthlyEnergy_kWh = 0;
			monthlyResetTime = currentMonth;
		}
		monthlyEnergy_kWh += energy_kWh;
		_logUsermodInaSensor("Monthly energy updated to: %.6f kWh", monthlyEnergy_kWh);
	}

#ifndef WLED_DISABLE_MQTT
	/**
	** Function to publish INA2xx sensor data to MQTT
	**/
	void publishMqtt(float shuntVoltage, float busVoltage, float loadVoltage, 
					float current, float current_mA, float power, 
					float power_mW, bool overflow) {
		if (!WLED_MQTT_CONNECTED) {
			_logUsermodInaSensor("MQTT not connected, skipping publish");
			return;
		}

		_logUsermodInaSensor("Publishing sensor data to MQTT");

		// Create a JSON document to hold sensor data
		StaticJsonDocument<1024> jsonDoc;

		// Populate the JSON document with sensor readings
		jsonDoc["shunt_voltage_mV"] = shuntVoltage;
		jsonDoc["bus_voltage_V"] = busVoltage;
		jsonDoc["load_voltage_V"] = loadVoltage;
		jsonDoc["current_A"] = current;
		jsonDoc["current_mA"] = current_mA;
		jsonDoc["power_W"] = power;
		jsonDoc["power_mW"] = power_mW;
		jsonDoc["overflow"] = overflow;
		jsonDoc["shunt_resistor_Ohms"] = shuntResistor;

		if (mqttStateRestored) { // only add energy_kWh fields after retained state arrives from mqtt
			// Energy calculations
			jsonDoc["daily_energy_kWh"] = dailyEnergy_kWh;
			jsonDoc["monthly_energy_kWh"] = monthlyEnergy_kWh;
			jsonDoc["total_energy_kWh"] = totalEnergy_kWh;
		} else {
			_logUsermodInaSensor("Skipping energy fields until MQTT state restored");
		}

		// Reset timestamps
		jsonDoc["dailyResetTime"] = dailyResetTime;
		jsonDoc["monthlyResetTime"] = monthlyResetTime;

		// Serialize the JSON document into a character buffer
		char buffer[1024];
		size_t payload_size = serializeJson(jsonDoc, buffer, sizeof(buffer));

		// Construct the MQTT topic using the device topic
		char topic[128];
		snprintf_P(topic, sizeof(topic), "%s/sensor/ina2xx", mqttDeviceTopic);
		_logUsermodInaSensor("MQTT topic: %s", topic);

		// Publish the serialized JSON data to the specified MQTT topic
		mqtt->publish(topic, 0, true, buffer, payload_size);
		_logUsermodInaSensor("MQTT publish complete, payload size: %d bytes", payload_size);
	}

	/**
	** Function to create Home Assistant sensor configuration
	**/
	void mqttCreateHassSensor(const String &name, const String &topic, 
							const String &deviceClass, const String &unitOfMeasurement, 
							const String &jsonKey, const String &SensorType) {
		String sanitizedName = name;
		sanitizedName.replace(' ', '-');
		_logUsermodInaSensor("Creating HA sensor: %s", sanitizedName.c_str());

		String sanitizedMqttClientID = sanitizeMqttClientID(mqttClientID);
		sanitizedMqttClientID += "-" + String(escapedMac.c_str());

		// Create a JSON document for the sensor configuration
		StaticJsonDocument<1024> doc;

		// Populate the JSON document with sensor configuration details
		doc[F("name")] = name;
		doc[F("stat_t")] = topic;
    
		String uid = escapedMac.c_str();
		uid += "_" + sanitizedName;
		doc[F("uniq_id")] = uid;
		_logUsermodInaSensor("Sensor unique ID: %s", uid.c_str());

		// Template to extract specific value from JSON
		doc[F("val_tpl")] = String("{{ value_json.") + jsonKey + String(" }}");
		if (unitOfMeasurement != "")
			doc[F("unit_of_meas")] = unitOfMeasurement;
		if (deviceClass != "")
			doc[F("dev_cla")] = deviceClass;
		if (SensorType != "binary_sensor")
			doc[F("exp_aft")] = 1800;

		// Device details nested object
		JsonObject device = doc.createNestedObject(F("device"));
		device[F("name")] = serverDescription;
		device[F("ids")] = serverDescription;
		device[F("mf")] = F(WLED_BRAND);
		device[F("mdl")] = F(WLED_PRODUCT_NAME);
		device[F("sw")] = versionString;
		#ifdef ESP32
			device[F("hw")] = F("esp32");
		#else
			device[F("hw")] = F("esp8266");
		#endif
		JsonArray connections = device[F("cns")].createNestedArray();
		connections.add(F("mac"));
		connections.add(WiFi.macAddress());

		// Serialize the JSON document into a temporary string
		char buffer[1024];
		size_t payload_size = serializeJson(doc, buffer, sizeof(buffer));

		char topic_S[128];
		int length = snprintf_P(topic_S, sizeof(topic_S), "homeassistant/%s/%s/%s/config", SensorType.c_str(), sanitizedMqttClientID.c_str(), sanitizedName.c_str());
		if (length >= sizeof(topic_S)) {
			_logUsermodInaSensor("HA topic truncated - potential buffer overflow");
		}

		// Debug output for the Home Assistant topic and configuration
		_logUsermodInaSensor("Topic: %s", topic_S);
		_logUsermodInaSensor("Buffer: %s", buffer);

		// Publish the sensor configuration to Home Assistant
		mqtt->publish(topic_S, 0, true, buffer, payload_size);
		_logUsermodInaSensor("Home Assistant sensor %s created", sanitizedName.c_str());
	}

	void mqttRemoveHassSensor(const String &name, const String &SensorType) {
		String sanitizedName = name;
		sanitizedName.replace(' ', '-');
		_logUsermodInaSensor("Removing HA sensor: %s", sanitizedName.c_str());

		String sanitizedMqttClientID = sanitizeMqttClientID(mqttClientID);
		sanitizedMqttClientID += "-" + String(escapedMac.c_str());

		char sensorTopic[128];
		int length = snprintf_P(sensorTopic, sizeof(sensorTopic), "homeassistant/%s/%s/%s/config", SensorType.c_str(), sanitizedMqttClientID.c_str(), sanitizedName.c_str());
		if (length >= sizeof(sensorTopic)) {
			_logUsermodInaSensor("HA sensor topic truncated - potential buffer overflow");
		}

		// Publish an empty message with retain to delete the sensor from Home Assistant
		mqtt->publish(sensorTopic, 0, true, "");
		_logUsermodInaSensor("Published empty message to remove sensor: %s", sensorTopic);
	}
#endif

public:
	// Destructor to clean up INA2xx object
	~UsermodINA2xx() {
		if (_ina2xx) {
			_logUsermodInaSensor("Cleaning up INA2xx sensor object");
			delete _ina2xx;
			_ina2xx = nullptr;
		}
	}

	// Setup function called once on boot or restart
	void setup() override {
		_logUsermodInaSensor("Setting up INA2xx sensor usermod");
		initDone = updateINA2xxSettings();  // Configure INA2xx settings
		if (initDone) {
			_logUsermodInaSensor("INA2xx setup complete and successful");
		} else {
			_logUsermodInaSensor("INA2xx setup failed");
		}
	}

	// Loop function called continuously
	void loop() override {
		// Check if the usermod is enabled and the check interval has elapsed
		if (!enabled || !initDone || !_ina2xx || millis() - lastCheck < checkInterval) {
			return;
		}

		lastCheck = millis();
		_logUsermodInaSensor("Reading sensor data at %lu ms", lastCheck);

		// Fetch sensor data
		shuntVoltage = roundDecimals(_ina2xx->getShuntVoltage_mV());
		busVoltage = roundDecimals(_ina2xx->getBusVoltage_V());
		
		float rawCurrent_mA = _ina2xx->getCurrent_mA();
		current_mA = roundDecimals(rawCurrent_mA);
		current = roundDecimals(rawCurrent_mA / 1000.0); // Convert from mA to A

		float rawPower_mW = _ina2xx->getBusPower();
		power_mW = roundDecimals(rawPower_mW);
		power = roundDecimals(rawPower_mW / 1000.0); // Convert from mW to W

		loadVoltage = roundDecimals(busVoltage + (shuntVoltage / 1000));
		overflow = _ina2xx->getOverflow() != 0;

		_logUsermodInaSensor("Sensor readings - Shunt: %.3f mV, Bus: %.3f V, Load: %.3f V", shuntVoltage, busVoltage, loadVoltage);
		_logUsermodInaSensor("Sensor readings - Current: %.3f A (%.3f mA), Power: %.3f W (%.3f mW)", current, current_mA, power, power_mW);
		_logUsermodInaSensor("Overflow status: %s", overflow ? "TRUE" : "FALSE");

		// Update energy consumption
		if (lastPublishTime != 0) {
			if (power >= 0) {
				updateEnergy(power, lastCheck - lastPublishTime);
			} else {
				_logUsermodInaSensor("Skipping energy update due to negative power: %.3f W", power);
			}
		} else {
			_logUsermodInaSensor("First reading - establishing baseline for energy calculation");
		}
		lastPublishTime = lastCheck;

	#ifndef WLED_DISABLE_MQTT
		// Publish sensor data via MQTT if connected and enabled
		if (WLED_MQTT_CONNECTED) {
			if (mqttPublish) {
				if (mqttPublishAlways || hasValueChanged()) {
					_logUsermodInaSensor("Publishing MQTT data (always=%d, changed=%d)", mqttPublishAlways, hasValueChanged());
					publishMqtt(shuntVoltage, busVoltage, loadVoltage, current, current_mA, power, power_mW, overflow);

					last_sent_shuntVoltage = shuntVoltage;
					last_sent_busVoltage = busVoltage;
					last_sent_loadVoltage = loadVoltage;
					last_sent_current = current;
					last_sent_current_mA = current_mA;
					last_sent_power = power;
					last_sent_power_mW = power_mW;
					last_sent_overflow = overflow;

					mqttPublishSent = true;
				} else {
					_logUsermodInaSensor("No significant change in values, skipping MQTT publish");
				}
			} else if (!mqttPublish && mqttPublishSent) {
				_logUsermodInaSensor("MQTT publishing disabled, removing previous retained message");
				char sensorTopic[128];
				snprintf_P(sensorTopic, 127, "%s/sensor/ina2xx", mqttDeviceTopic);

				// Publishing an empty retained message to delete the sensor from Home Assistant
				mqtt->publish(sensorTopic, 0, true, "");
				mqttPublishSent = false;
			}
		}

		// Publish Home Assistant discovery data if enabled
		if (haDiscovery && !haDiscoverySent) {
			if (WLED_MQTT_CONNECTED) {
				_logUsermodInaSensor("Setting up Home Assistant discovery");
				char topic[128];
				snprintf_P(topic, 127, "%s/sensor/ina2xx", mqttDeviceTopic); // Common topic for all INA2xx data

				mqttCreateHassSensor(F("Current"), topic, F("current"), F("A"), F("current_A"), F("sensor"));
				mqttCreateHassSensor(F("Voltage"), topic, F("voltage"), F("V"), F("bus_voltage_V"), F("sensor"));
				mqttCreateHassSensor(F("Power"), topic, F("power"), F("W"), F("power_W"), F("sensor"));
				mqttCreateHassSensor(F("Shunt Voltage"), topic, F("voltage"), F("mV"), F("shunt_voltage_mV"), F("sensor"));
				mqttCreateHassSensor(F("Shunt Resistor"), topic, F(""), F("Ω"), F("shunt_resistor_Ohms"), F("sensor"));
				mqttCreateHassSensor(F("Overflow"), topic, F(""), F(""), F("overflow"), F("sensor"));
				mqttCreateHassSensor(F("Daily Energy"), topic, F("energy"), F("kWh"), F("daily_energy_kWh"), F("sensor"));
				mqttCreateHassSensor(F("Monthly Energy"), topic, F("energy"), F("kWh"), F("monthly_energy_kWh"), F("sensor"));
				mqttCreateHassSensor(F("Total Energy"), topic, F("energy"), F("kWh"), F("total_energy_kWh"), F("sensor"));

				haDiscoverySent = true; // Mark as sent to avoid repeating
				_logUsermodInaSensor("Home Assistant discovery complete");
			}
		} else if (!haDiscovery && haDiscoverySent) {
			if (WLED_MQTT_CONNECTED) {
				_logUsermodInaSensor("Removing Home Assistant discovery sensors");
				// Remove previously created sensors
				mqttRemoveHassSensor(F("Current"), F("sensor"));
				mqttRemoveHassSensor(F("Voltage"), F("sensor"));
				mqttRemoveHassSensor(F("Power"), F("sensor"));
				mqttRemoveHassSensor(F("Shunt Voltage"), F("sensor"));
				mqttRemoveHassSensor(F("Daily Energy"), F("sensor"));
				mqttRemoveHassSensor(F("Monthly Energy"), F("sensor"));
				mqttRemoveHassSensor(F("Total Energy"), F("sensor"));
				mqttRemoveHassSensor(F("Shunt Resistor"), F("sensor"));
				mqttRemoveHassSensor(F("Overflow"), F("sensor"));

				haDiscoverySent = false; // Mark as sent to avoid repeating
				_logUsermodInaSensor("Home Assistant discovery removal complete");
			}
		}
	#endif
	}

#ifndef WLED_DISABLE_MQTT
	/**
	** Function to publish sensor data to MQTT
	**/
	bool onMqttMessage(char* topic, char* payload) override {
		if (!WLED_MQTT_CONNECTED || !enabled) return false;

		// Check if the message is for the correct topic
		if (strstr(topic, "/sensor/ina2xx") != nullptr) {
			_logUsermodInaSensor("MQTT message received on INA2xx topic");
			StaticJsonDocument<512> jsonDoc;

			// Parse the JSON payload
			DeserializationError error = deserializeJson(jsonDoc, payload);
			if (error) {
				_logUsermodInaSensor("JSON Parse Error: %s", error.c_str());
				return false;
			}

			// Update the energy values
			if (!mqttStateRestored) {
				// Only merge in retained MQTT values once!
				if (jsonDoc.containsKey("daily_energy_kWh")) {
					float restored = jsonDoc["daily_energy_kWh"];
					if (!isnan(restored)) {
						dailyEnergy_kWh += restored;
						_logUsermodInaSensor("Merged daily energy from MQTT: +%.6f kWh => %.6f kWh", restored, dailyEnergy_kWh);
					}
				}
				if (jsonDoc.containsKey("monthly_energy_kWh")) {
					float restored = jsonDoc["monthly_energy_kWh"];
					if (!isnan(restored)) {
						monthlyEnergy_kWh += restored;
						_logUsermodInaSensor("Merged monthly energy from MQTT: +%.6f kWh => %.6f kWh", restored, monthlyEnergy_kWh);
					}
				}
				if (jsonDoc.containsKey("total_energy_kWh")) {
					float restored = jsonDoc["total_energy_kWh"];
					if (!isnan(restored)) {
						totalEnergy_kWh += restored;
						_logUsermodInaSensor("Merged total energy from MQTT: +%.6f kWh => %.6f kWh", restored, totalEnergy_kWh);
					}
				}
				mqttStateRestored = true;  // Only do this once!
			}
			return true;
		}
		return false;
	}

	/**
	** Subscribe to MQTT topic for controlling the usermod
	**/
	void onMqttConnect(bool sessionPresent) override {
		if (!enabled) return;
		if (WLED_MQTT_CONNECTED) {
			char subuf[64];
			if (mqttDeviceTopic[0] != 0) {
				strcpy(subuf, mqttDeviceTopic);
				strcat_P(subuf, PSTR("/sensor/ina2xx"));
				mqtt->subscribe(subuf, 0);
				_logUsermodInaSensor("Subscribed to MQTT topic: %s", subuf);
			}
		}
	}
#endif

	/**
	** Add energy consumption data to a JSON object for reporting
	**/
	void addToJsonInfo(JsonObject &root) {
		JsonObject user = root[F("u")];
		if (user.isNull()) {
			user = root.createNestedObject(F("u"));
		}

		JsonArray energy_json = user.createNestedArray(F("INA2xx:"));

		if (!enabled || !initDone) {
			energy_json.add(F("disabled"));
			if (!alreadyLoggedDisabled) {
				_logUsermodInaSensor("Adding disabled status to JSON info");
				alreadyLoggedDisabled = true;
			}
			return;
		}
		alreadyLoggedDisabled = false;

		// File needs to be UTF-8 to show an arrow that points down and right instead of an question mark
		// Create a nested array for daily energy
		JsonArray dailyEnergy_json = user.createNestedArray(F("⤷ Daily Energy"));
		dailyEnergy_json.add(dailyEnergy_kWh);
		dailyEnergy_json.add(F(" kWh"));

		// Create a nested array for monthly energy
		JsonArray monthlyEnergy_json = user.createNestedArray(F("⤷ Monthly Energy"));
		monthlyEnergy_json.add(monthlyEnergy_kWh);
		monthlyEnergy_json.add(F(" kWh"));

		// Create a nested array for total energy
		JsonArray totalEnergy_json = user.createNestedArray(F("⤷ Total Energy"));
		totalEnergy_json.add(totalEnergy_kWh);
		totalEnergy_json.add(F(" kWh"));

		_logUsermodInaSensor("Added energy data to JSON info: daily=%.6f, monthly=%.6f, total=%.6f kWh", dailyEnergy_kWh, monthlyEnergy_kWh, totalEnergy_kWh);
	}

	/**
	** Add the current state of energy consumption to a JSON object
	**/
	void addToJsonState(JsonObject& root) override {
		if (!enabled) return;
		if (!initDone) {
			_logUsermodInaSensor("Not adding to JSON state - initialization not complete");
			return;
		}

		JsonObject usermod = root[FPSTR(_name)];
		if (usermod.isNull()) {
			usermod = root.createNestedObject(FPSTR(_name));
		}

		usermod["enabled"] = enabled;
		usermod["shuntVoltage_mV"] = shuntVoltage;
		usermod["busVoltage_V"] = busVoltage;
		usermod["loadVoltage_V"] = loadVoltage;
		usermod["current_A"] = current;
		usermod["current_mA"] = current_mA;
		usermod["power_W"] = power;
		usermod["power_mW"] = power_mW;
		usermod["overflow"] = overflow;
		usermod["totalEnergy_kWh"] = totalEnergy_kWh;
		usermod["dailyEnergy_kWh"] = dailyEnergy_kWh;
		usermod["monthlyEnergy_kWh"] = monthlyEnergy_kWh;
		usermod["dailyResetTime"] = dailyResetTime;
		usermod["monthlyResetTime"] = monthlyResetTime;

		_logUsermodInaSensor("Added sensor readings to JSON state: V=%.3fV, I=%.3fA, P=%.3fW", loadVoltage, current, power);
	}
	
	/**
	** Read energy consumption data from a JSON object
	**/
	void readFromJsonState(JsonObject& root) override {
		if (!enabled) return;
		if (!initDone) { // Prevent crashes on boot if initialization is not done
			_logUsermodInaSensor("Not reading from JSON state - initialization not complete");
			return;
		}

		JsonObject usermod = root[FPSTR(_name)];
		if (!usermod.isNull()) {
			// Read values from JSON or retain existing values if not present
			if (usermod.containsKey("enabled")) {
				bool prevEnabled = enabled;
				enabled = usermod["enabled"] | enabled;	
				if (prevEnabled != enabled) {
					_logUsermodInaSensor("Enabled state changed: %s", enabled ? "enabled" : "disabled");
				}
			}

			if (usermod.containsKey("totalEnergy_kWh")) {
				float prevTotal = totalEnergy_kWh;
				totalEnergy_kWh = usermod["totalEnergy_kWh"] | totalEnergy_kWh;
				if (totalEnergy_kWh != prevTotal) {
					_logUsermodInaSensor("Total energy updated from JSON: %.6f kWh", totalEnergy_kWh);
				}
			}

			if (usermod.containsKey("dailyEnergy_kWh")) {
				float prevDaily = dailyEnergy_kWh;
				dailyEnergy_kWh = usermod["dailyEnergy_kWh"] | dailyEnergy_kWh;
				if (dailyEnergy_kWh != prevDaily) {
					_logUsermodInaSensor("Daily energy updated from JSON: %.6f kWh", dailyEnergy_kWh);
				}
			}

			if (usermod.containsKey("monthlyEnergy_kWh")) {
				float prevMonthly = monthlyEnergy_kWh;
				monthlyEnergy_kWh = usermod["monthlyEnergy_kWh"] | monthlyEnergy_kWh;
				if (monthlyEnergy_kWh != prevMonthly) {
					_logUsermodInaSensor("Monthly energy updated from JSON: %.6f kWh", monthlyEnergy_kWh);
				}
			}

			if (usermod.containsKey("dailyResetTime")) {
				unsigned long prevDailyReset = dailyResetTime;
				dailyResetTime = usermod["dailyResetTime"] | dailyResetTime;
				if (dailyResetTime != prevDailyReset) {
					_logUsermodInaSensor("Daily reset time updated from JSON: %lu", dailyResetTime);
				}
			}

			if (usermod.containsKey("monthlyResetTime")) {
				unsigned long prevMonthlyReset = monthlyResetTime;
				monthlyResetTime = usermod["monthlyResetTime"] | monthlyResetTime;
				if (monthlyResetTime != prevMonthlyReset) {
					_logUsermodInaSensor("Monthly reset time updated from JSON: %lu", monthlyResetTime);
				}
			}
		} else {
			_logUsermodInaSensor("No usermod data found in JSON state");
		}
	}

	/**
	** Append configuration options to the Usermod menu.
	**/
	void addToConfig(JsonObject& root) override {
		JsonObject top = root.createNestedObject(F("INA2xx"));
		top["Enabled"] = enabled;
		top["i2c_address"] = static_cast<uint8_t>(_i2cAddress);
		top["check_interval"] = checkInterval / 1000;
		top["conversion_time"] = conversionTime;
		top["decimals"] = _decimalFactor;
		top["shunt_resistor"] = shuntResistor;
		top["correction_factor"] = correctionFactor;
		top["pga_gain"]        = pGain;
		top["bus_range"]       = busRange;
		top["shunt_offset"]    = shuntVoltOffset_mV;

		#ifndef WLED_DISABLE_MQTT
			// Store MQTT settings if MQTT is not disabled
			top["mqtt_publish"] = mqttPublish;
			top["mqtt_publish_always"] = mqttPublishAlways;
			top["ha_discovery"] = haDiscovery;
		#endif
	}
	
	/**
	** Append configuration UI data for the usermod menu.
	**/
	void appendConfigData() override {
		// Append the dropdown for I2C address selection
		oappend("dd=addDropdown('INA2xx','i2c_address');");
		oappend("addOption(dd,'0x40 - Default',0x40, true);");
		oappend("addOption(dd,'0x41 - A0 soldered',0x41);");
		oappend("addOption(dd,'0x44 - A1 soldered',0x44);");
		oappend("addOption(dd,'0x45 - A0 and A1 soldered',0x45);");

		// Append the dropdown for ADC mode (resolution + samples)
		oappend("ct=addDropdown('INA2xx','conversion_time');");
		oappend("addOption(ct,'9-Bit (84 µs)',0);");
		oappend("addOption(ct,'10-Bit (148 µs)',1);");
		oappend("addOption(ct,'11-Bit (276 µs)',2);");
		oappend("addOption(ct,'12-Bit (532 µs)',3, true);");
		oappend("addOption(ct,'2 samples (1.06 ms)',9);");
		oappend("addOption(ct,'4 samples (2.13 ms)',10);");
		oappend("addOption(ct,'8 samples (4.26 ms)',11);");
		oappend("addOption(ct,'16 samples (8.51 ms)',12);");
		oappend("addOption(ct,'32 samples (17.02 ms)',13);");
		oappend("addOption(ct,'64 samples (34.05 ms)',14);");
		oappend("addOption(ct,'128 samples (68.10 ms)',15);");

		// Append the dropdown for decimal precision (0 to 3)
		oappend("df=addDropdown('INA2xx','decimals');");
		for (int i = 0; i <= 3; i++) {
			oappend(String("addOption(df,'" + String(i) + "'," + String(i) + (i == 2 ? ", true);" : ");")).c_str());
		}

		oappend("pg=addDropdown('INA2xx','pga_gain');");
		oappend("addOption(pg,'40mV',0);");
		oappend("addOption(pg,'80mV',2048);");
		oappend("addOption(pg,'160mV',4096);");
		oappend("addOption(pg,'320mV',6144, true);");

		oappend("br=addDropdown('INA2xx','bus_range');");
		oappend("addOption(br,'16V',0);");
		oappend("addOption(br,'32V',8192, true);");
	}
	
	/**
	** Read settings from the Usermod menu configuration
	**/
	bool readFromConfig(JsonObject& root) override {
		JsonObject top = root[FPSTR(_name)];

		bool configComplete = !top.isNull();

		_logUsermodInaSensor("Checking if configuration has changed:");
		UPDATE_CONFIG(top, "Enabled", enabled, "%u");
		UPDATE_CONFIG(top, "i2c_address",       _i2cAddress,       "0x%02X");
		UPDATE_CONFIG(top, "conversion_time",    conversionTime,    "%u");
		UPDATE_CONFIG(top, "decimals",           _decimalFactor,    "%u");
		UPDATE_CONFIG(top, "shunt_resistor",     shuntResistor,     "%.6f Ohms");
		UPDATE_CONFIG(top, "correction_factor",  correctionFactor,  "%.3f");
		UPDATE_CONFIG(top, "pga_gain",           pGain,             "%d");
		UPDATE_CONFIG(top, "bus_range",          busRange,          "%d");
		UPDATE_CONFIG(top, "shunt_offset",       shuntVoltOffset_mV,"%.3f mV");

		#ifndef WLED_DISABLE_MQTT
			UPDATE_CONFIG(top, "mqtt_publish",         mqttPublish,       "%u");
			UPDATE_CONFIG(top, "mqtt_publish_always",  mqttPublishAlways, "%u");

			bool tempHaDiscovery = haDiscovery;
			UPDATE_CONFIG(top, "ha_discovery", haDiscovery, "%u");
			if (haDiscovery != tempHaDiscovery) haDiscoverySent = !haDiscovery;
		#endif

		uint16_t tempInterval = 0;
		if (getJsonValue(top[F("check_interval")], tempInterval)) {
			if (1 <= tempInterval && tempInterval <= 600) {
				uint32_t newInterval = static_cast<uint32_t>(tempInterval) * 1000UL;
				if (newInterval != checkInterval) {
					_logUsermodInaSensor("Check interval updated to: %u ms", newInterval);
					checkInterval = newInterval;
				}
			} else {
				_logUsermodInaSensor("Invalid check_interval value %u; using default %u seconds", tempInterval, INA219_CHECK_INTERVAL);
				checkInterval = static_cast<uint32_t>(_checkInterval) * 1000UL;
			}
		} else {
			configComplete = false;
		}

		bool prevInitDone = initDone;
		initDone = updateINA2xxSettings();  // Configure INA2xx settings

		if (prevInitDone != initDone) {
			_logUsermodInaSensor("Sensor initialization %s", initDone ? "succeeded" : "failed");
		}

		return configComplete;
	}
	
	/**
	** Get the unique identifier for this usermod.
	**/
	uint16_t getId() override {
		return USERMOD_ID_INA2XX;
	}
};

const char UsermodINA2xx::_name[] PROGMEM = "INA2xx";

static UsermodINA2xx ina2xx_v2;
REGISTER_USERMOD(ina2xx_v2);