// Configurable settings for the INA219 Usermod

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

class UsermodINA219 : public Usermod {
private:
	static const char _name[];  // Name of the usermod

	bool initDone = false;  // Flag for successful initialization
	unsigned long lastCheck = 0; // Timestamp for the last check

	// Define the variables using the pre-defined or default values
	bool enabled = INA219_ENABLED;
	uint8_t _i2cAddress = INA219_I2C_ADDRESS;
	uint16_t _checkInterval = INA219_CHECK_INTERVAL; // seconds
	uint32_t checkInterval  = static_cast<uint32_t>(_checkInterval) * 1000UL; // ms
	INA219_ADC_MODE conversionTime = static_cast<INA219_ADC_MODE>(INA219_CONVERSION_TIME);
	uint8_t _decimalFactor = INA219_DECIMAL_FACTOR;
	float shuntResistor = INA219_SHUNT_RESISTOR;
	float correctionFactor = INA219_CORRECTION_FACTOR;
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

	// Variables to track last sent readings
	float _lastCurrentSent = 0;
	float _lastVoltageSent = 0;
	float _lastPowerSent = 0;
	float _lastShuntVoltageSent = 0;

	INA219_WE *_ina219 = nullptr; // INA219 sensor object

	// Function to truncate decimals based on the configured decimal factor
	float truncateDecimals(float val) {
		if (_decimalFactor == 0) {
			return roundf(val);
		}
		float factor = powf(10.0f, _decimalFactor);
		return roundf(val * factor) / factor;
	}

	// Update INA219 settings and reinitialize sensor if necessary
	void updateINA219Settings() {
		// Validate I2C pins; if invalid, disable usermod and log message
		if (i2c_scl < 0 || i2c_sda < 0) {
			enabled = false;
			DEBUG_PRINTLN(F("INA219 disabled: Invalid I2C pins. Check global I2C settings."));
			return;
		}
		DEBUG_PRINT(F("Using I2C SDA: "));
		DEBUG_PRINTLN(i2c_sda);
		DEBUG_PRINT(F("Using I2C SCL: "));
		DEBUG_PRINTLN(i2c_scl);

		// Reinitialize the INA219 instance with updated settings
		if (_ina219 != nullptr) {
			delete _ina219;
		}
		_ina219 = new INA219_WE(_i2cAddress);

		if (!_ina219->init()) {
			DEBUG_PRINTLN(F("INA219 initialization failed!"));
			enabled = false;
			return;
		}
		_ina219->setShuntSizeInOhms(shuntResistor);
		_ina219->setADCMode(conversionTime);
		_ina219->setCorrectionFactor(correctionFactor);
	}

public:
	// Destructor to clean up INA219 object
	~UsermodINA219() {
		delete _ina219;
		_ina219 = nullptr;
	}

	// Setup function called once on boot or restart
	void setup() override {
		updateINA219Settings();  // Configure INA219 settings
		initDone = true;  // Mark initialization as complete
	}

	// Loop function called continuously
	void loop() override {
		// Check if the usermod is enabled and the check interval has elapsed
		if (enabled && millis() - lastCheck > checkInterval) {
			lastCheck = millis();

			// Fetch sensor data
			shuntVoltage = truncateDecimals(_ina219->getShuntVoltage_mV());
			busVoltage = truncateDecimals(_ina219->getBusVoltage_V());
			current_mA = truncateDecimals(_ina219->getCurrent_mA());
			current = truncateDecimals(_ina219->getCurrent_mA() / 1000.0); // Convert from mA to A
			power_mW = truncateDecimals(_ina219->getBusPower());
			power = truncateDecimals(_ina219->getBusPower() / 1000.0); // Convert from mW to W
			loadVoltage = truncateDecimals(busVoltage + (shuntVoltage / 1000));
			//overflow = truncateDecimals(_ina219->getOverflow());
			overflow = _ina219->getOverflow() != 0;

			// Update energy consumption
			if (lastPublishTime != 0) {
				updateEnergy(power, lastCheck - lastPublishTime);
			}
			lastPublishTime = lastCheck;

		#ifndef WLED_DISABLE_MQTT
			// Publish sensor data via MQTT if connected and enabled
			if (WLED_MQTT_CONNECTED) {
				if (mqttPublish) {
					if (mqttPublishAlways || hasValueChanged()) {
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
					}
				} else if (!mqttPublish && mqttPublishSent) {
					char sensorTopic[128];
					snprintf_P(sensorTopic, 127, "%s/sensor/ina219", mqttDeviceTopic);

					// Publishing an empty retained message to delete the sensor from Home Assistant
					mqtt->publish(sensorTopic, 0, true, "");
					mqttPublishSent = false;
				}
			}

			// Publish Home Assistant discovery data if enabled
			if (haDiscovery && !haDiscoverySent) {
				if (WLED_MQTT_CONNECTED) {
					char topic[128];
					snprintf_P(topic, 127, "%s/sensor/ina219", mqttDeviceTopic); // Common topic for all INA219 data

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
				}
			} else if (!haDiscovery && haDiscoverySent) {
				if (WLED_MQTT_CONNECTED) {
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
				}
			}
		#endif
		}
	}
	
	bool hasSignificantChange(float oldValue, float newValue, float threshold = 0.01f) {
		return fabsf(oldValue - newValue) > threshold;
	}

	bool hasValueChanged() {
		return hasSignificantChange(last_sent_shuntVoltage, shuntVoltage) ||
			hasSignificantChange(last_sent_busVoltage, busVoltage) ||
			hasSignificantChange(last_sent_loadVoltage, loadVoltage) ||
			hasSignificantChange(last_sent_current, current) ||
			hasSignificantChange(last_sent_current_mA, current_mA) ||
			hasSignificantChange(last_sent_power, power) ||
			hasSignificantChange(last_sent_power_mW, power_mW) ||
			(last_sent_overflow != overflow);
	}

#ifndef WLED_DISABLE_MQTT
	/**
	** Function to publish sensor data to MQTT
	**/
	bool onMqttMessage(char* topic, char* payload) override {
		if (!WLED_MQTT_CONNECTED || !enabled) return false;
		// Check if the message is for the correct topic
		if (strstr(topic, "/sensor/ina219") != nullptr) {
			StaticJsonDocument<512> jsonDoc;

			// Parse the JSON payload
			DeserializationError error = deserializeJson(jsonDoc, payload);
			if (error) {
				return false;
			}

			// Update the energy values
			dailyEnergy_kWh = jsonDoc["daily_energy_kWh"];
			monthlyEnergy_kWh = jsonDoc["monthly_energy_kWh"];
			totalEnergy_kWh = jsonDoc["total_energy_kWh"];
			dailyResetTime = jsonDoc["dailyResetTime"];
			monthlyResetTime = jsonDoc["monthlyResetTime"];

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
				strcat_P(subuf, PSTR("/sensor/ina219"));
				mqtt->subscribe(subuf, 0);
			}
		}
	}
#endif
		
	/**
	** Function to publish INA219 sensor data to MQTT
	**/
	void publishMqtt(float shuntVoltage, float busVoltage, float loadVoltage, 
					float current, float current_mA, float power, 
					float power_mW, bool overflow) {
		// Publish to MQTT only if the WLED MQTT feature is enabled
		#ifndef WLED_DISABLE_MQTT
			if (WLED_MQTT_CONNECTED) {
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

				// Energy calculations
				jsonDoc["daily_energy_kWh"] = dailyEnergy_kWh;
				jsonDoc["monthly_energy_kWh"] = monthlyEnergy_kWh;
				jsonDoc["total_energy_kWh"] = totalEnergy_kWh;

				// Reset timestamps
				jsonDoc["dailyResetTime"] = dailyResetTime;
				jsonDoc["monthlyResetTime"] = monthlyResetTime;
					
				// Serialize the JSON document into a character buffer
				char buffer[1024];
				size_t payload_size = serializeJson(jsonDoc, buffer, sizeof(buffer));

				// Construct the MQTT topic using the device topic
				char topic[128];
				snprintf_P(topic, sizeof(topic), "%s/sensor/ina219", mqttDeviceTopic);

				// Publish the serialized JSON data to the specified MQTT topic
				mqtt->publish(topic, 0, true, buffer, payload_size);
			}
		#endif
	}
	
	/**
	** Function to create Home Assistant sensor configuration
	**/
	void mqttCreateHassSensor(const String &name, const String &topic, 
							const String &deviceClass, const String &unitOfMeasurement, 
							const String &jsonKey, const String &SensorType) {
		String sanitizedName = name;
		sanitizedName.replace(' ', '-');

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
		snprintf_P(topic_S, sizeof(topic_S), "homeassistant/%s/%s/%s/config", SensorType, sanitizedMqttClientID.c_str(), sanitizedName.c_str());

		// Debug output for the Home Assistant topic and configuration
		DEBUG_PRINTLN(topic_S);
		DEBUG_PRINTLN(buffer);

		// Publish the sensor configuration to Home Assistant
		mqtt->publish(topic_S, 0, true, buffer, payload_size);
	}
	
	void mqttRemoveHassSensor(const String &name, const String &SensorType) {
		String sanitizedName = name;
		sanitizedName.replace(' ', '-');

		String sanitizedMqttClientID = sanitizeMqttClientID(mqttClientID);
		sanitizedMqttClientID += "-" + String(escapedMac.c_str());

		char sensorTopic[128];
		snprintf_P(sensorTopic, 127, "homeassistant/%s/%s/%s/config", SensorType.c_str(), sanitizedMqttClientID.c_str(), sanitizedName.c_str());

		// Publish an empty message with retain to delete the sensor from Home Assistant
		mqtt->publish(sensorTopic, 0, true, "");
	}

	// Sanitize the mqttClientID by replacing invalid characters.
	String sanitizeMqttClientID(const String &clientID) {
		String sanitizedID;

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
		return sanitizedID;
	}

	/**
	** Function to update energy calculations based on power and duration
	**/
	void updateEnergy(float power, unsigned long durationMs) {
		float durationHours = durationMs / 3600000.0; // Milliseconds to hours
		float energy_kWh = (power / 1000.0) * durationHours; // Watts to kilowatt-hours (kWh)
		totalEnergy_kWh += energy_kWh; // Update total energy consumed

		// Get current time
		time_t now = time(nullptr);
		if (now <= 0) return; // Safety check if time isn't available yet

		struct tm *timeinfo = localtime(&now);

		// Calculate day identifier (days since epoch)
		long currentDay = now / 86400;

		// Reset daily energy at midnight or if day changed
		if ((timeinfo->tm_hour == 0 && timeinfo->tm_min == 0 && dailyResetTime != currentDay) || 
			(currentDay > dailyResetTime && dailyResetTime > 0)) {
			dailyEnergy_kWh = 0;
			dailyResetTime = currentDay;
		}
		dailyEnergy_kWh += energy_kWh;

		// Calculate month identifier (year*12 + month)
		long currentMonth = (timeinfo->tm_year + 1900) * 12 + timeinfo->tm_mon;
		
		// Reset monthly energy on first day of month or if month changed
		if ((timeinfo->tm_mday == 1 && timeinfo->tm_hour == 0 && timeinfo->tm_min == 0 && 
			 monthlyResetTime != currentMonth) || (currentMonth > monthlyResetTime && monthlyResetTime > 0)) {
			monthlyEnergy_kWh = 0;
			monthlyResetTime = currentMonth;
		}
		monthlyEnergy_kWh += energy_kWh;
	}
	
	/**
	** Add energy consumption data to a JSON object for reporting
	**/
	void addToJsonInfo(JsonObject &root) {
		JsonObject user = root[F("u")];
		if (user.isNull()) {
			user = root.createNestedObject(F("u"));
		}

		// Create a nested array for energy data
		JsonArray energy_json_seperator = user.createNestedArray(F("------------------------------------"));
				
		JsonArray energy_json = user.createNestedArray(F("Energy Consumption:"));

		if (!enabled) {
			energy_json.add(F("disabled"));
		} else {
			// Create a nested array for daily energy
			JsonArray dailyEnergy_json = user.createNestedArray(F("Daily Energy"));
			dailyEnergy_json.add(dailyEnergy_kWh);
			dailyEnergy_json.add(F(" kWh"));

			// Create a nested array for monthly energy
			JsonArray monthlyEnergy_json = user.createNestedArray(F("Monthly Energy"));
			monthlyEnergy_json.add(monthlyEnergy_kWh);
			monthlyEnergy_json.add(F(" kWh"));

			// Create a nested array for total energy
			JsonArray totalEnergy_json = user.createNestedArray(F("Total Energy"));
			totalEnergy_json.add(totalEnergy_kWh);
			totalEnergy_json.add(F(" kWh"));
		}
	}
	
	/**
	** Add the current state of energy consumption to a JSON object
	**/
	void addToJsonState(JsonObject& root) override {
		if (!initDone) return;

		JsonObject usermod = root[FPSTR(_name)];
		if (usermod.isNull()) {
			usermod = root.createNestedObject(FPSTR(_name));
		}

		usermod["totalEnergy_kWh"] = totalEnergy_kWh;
		usermod["dailyEnergy_kWh"] = dailyEnergy_kWh;
		usermod["monthlyEnergy_kWh"] = monthlyEnergy_kWh;
		usermod["dailyResetTime"] = dailyResetTime;
		usermod["monthlyResetTime"] = monthlyResetTime;
	}
	
	/**
	** Read energy consumption data from a JSON object
	**/
	void readFromJsonState(JsonObject& root) override {
		if (!initDone) return; // Prevent crashes on boot if initialization is not done

		JsonObject usermod = root[FPSTR(_name)];
		if (!usermod.isNull()) {
			// Read values from JSON or retain existing values if not present
			totalEnergy_kWh = usermod["totalEnergy_kWh"] | totalEnergy_kWh;
			dailyEnergy_kWh = usermod["dailyEnergy_kWh"] | dailyEnergy_kWh;
			monthlyEnergy_kWh = usermod["monthlyEnergy_kWh"] | monthlyEnergy_kWh;
			dailyResetTime = usermod["dailyResetTime"] | dailyResetTime;
			monthlyResetTime = usermod["monthlyResetTime"] | monthlyResetTime;
		}
	}
	
	/**
	** Append configuration options to the Usermod menu.
	**/
	void addToConfig(JsonObject& root) override {
		JsonObject top = root.createNestedObject(F("INA219"));
		top["Enabled"] = enabled;
		top["i2c_address"] = static_cast<uint8_t>(_i2cAddress);
		top["check_interval"] = checkInterval / 1000;
		top["conversion_time"] = conversionTime;
		top["decimals"] = _decimalFactor;
		top["shunt_resistor"] = shuntResistor;

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
		oappend(F("dd=addDropdown('INA219','i2c_address');"));
		oappend(F("addOption(dd,'0x40 - Default',0x40, true);"));
		oappend(F("addOption(dd,'0x41 - A0 soldered',0x41);"));
		oappend(F("addOption(dd,'0x44 - A1 soldered',0x44);"));
		oappend(F("addOption(dd,'0x45 - A0 and A1 soldered',0x45);"));

		// Append the dropdown for ADC mode (resolution + samples)
		oappend(F("ct=addDropdown('INA219','conversion_time');"));
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

		// Append the dropdown for decimal precision (0 to 10)
		oappend(F("df=addDropdown('INA219','decimals');"));
		for (int i = 0; i <= 3; i++) {
			oappend(String("addOption(df,'" + String(i) + "'," + String(i) + (i == 2 ? ", true);" : ");")).c_str());
		}
	}
	
	/**
	** Read settings from the Usermod menu configuration
	**/
	bool readFromConfig(JsonObject& root) override {
		JsonObject top = root[FPSTR(_name)];

		bool configComplete = !top.isNull();
		configComplete &= getJsonValue(top["Enabled"], enabled);
		configComplete &= getJsonValue(top[F("i2c_address")], _i2cAddress);

		if (getJsonValue(top[F("check_interval")], checkInterval)) {
			if (1 <= checkInterval && checkInterval <= 600) {
				checkInterval *= 1000UL;
			} else {
				DEBUG_PRINTLN(F("INA219: Invalid check_interval value; using default."));
				checkInterval = _checkInterval * 1000UL;
			}
		} else {
			configComplete = false;
		}
		configComplete &= getJsonValue(top["conversion_time"], conversionTime);
		configComplete &= getJsonValue(top["decimals"], _decimalFactor);
		configComplete &= getJsonValue(top["shunt_resistor"], shuntResistor);

		#ifndef WLED_DISABLE_MQTT
			configComplete &= getJsonValue(top["mqtt_publish"], mqttPublish);
			configComplete &= getJsonValue(top["mqtt_publish_always"], mqttPublishAlways);
			configComplete &= getJsonValue(top["ha_discovery"], haDiscovery);

			haDiscoverySent = !haDiscovery;
		#endif

		updateINA219Settings();

		return configComplete;
	}
	
	/**
	** Get the unique identifier for this usermod.
	**/
	uint16_t getId() override {
		return USERMOD_ID_INA219;
	}
};

const char UsermodINA219::_name[] PROGMEM = "INA219";

static UsermodINA219 ina219_v2;
REGISTER_USERMOD(ina219_v2);