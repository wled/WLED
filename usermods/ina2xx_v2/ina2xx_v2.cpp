#include "ina2xx_v2.h"

const char UsermodINA2xx::_name[] PROGMEM = "INA2xx";

UsermodINA2xx ina2xx_v2;
REGISTER_USERMOD(ina2xx_v2);

// Function to truncate decimals based on the configured decimal factor
float UsermodINA2xx::roundDecimals(float val) {
	_logUsermodInaSensor("Truncating value %.6f with factor %d", val, _decimalFactor);
	if (_decimalFactor == 0) {
		return roundf(val);
	}

	static const float factorLUT[4] = {1.f, 10.f, 100.f, 1000.f};
	float factor = (_decimalFactor <= 3) ? factorLUT[_decimalFactor]
									: powf(10.0f, _decimalFactor);

	return roundf(val * factor) / factor;
}

bool UsermodINA2xx::hasSignificantChange(float oldValue, float newValue, float threshold) {
	bool changed = fabsf(oldValue - newValue) > threshold;
	if (changed) {
		_logUsermodInaSensor("Significant change detected: old=%.6f, new=%.6f, diff=%.6f", oldValue, newValue, fabsf(oldValue - newValue));
	}
	return changed;
}

bool UsermodINA2xx::hasValueChanged() {
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

bool UsermodINA2xx::isTimeValid() const {
	return localTime >= 1577836800UL && localTime <= 4102444800UL;
}

void UsermodINA2xx::applyMqttRestoreIfReady() {
	if (!mqttRestorePending || mqttStateRestored) {
		return;
	}

	if (!isTimeValid()) {
		if (!mqttRestoreDeferredLogged) {
			_logUsermodInaSensor("Deferring MQTT energy restore until time sync is valid");
			mqttRestoreDeferredLogged = true;
		}
		return;
	}

	mqttRestoreDeferredLogged = false;
	long currentDay = localTime / 86400;
	long currentMonth = year(localTime) * 12 + month(localTime) - 1;

	if (mqttRestoreData.hasTotalEnergy) {
		totalEnergy_kWh += mqttRestoreData.totalEnergy;
		_logUsermodInaSensor("Applied total energy from MQTT: +%.6f kWh => %.6f kWh",
			mqttRestoreData.totalEnergy, totalEnergy_kWh);
	}

	auto restoredDailyDay = [&]() {
		if (mqttRestoreData.hasDailyResetTime) {
			return static_cast<long>(mqttRestoreData.dailyResetTime);
		}
		if (mqttRestoreData.hasDailyResetTimestamp) {
			return static_cast<long>(mqttRestoreData.dailyResetTimestamp / 86400UL);
		}
		return static_cast<long>(dailyResetTime);
	};

	auto dailyResetMatches = [&]() {
		if (mqttRestoreData.hasDailyResetTime) {
			return static_cast<long>(mqttRestoreData.dailyResetTime) == currentDay;
		}
		if (mqttRestoreData.hasDailyResetTimestamp) {
			return static_cast<long>(mqttRestoreData.dailyResetTimestamp / 86400UL) == currentDay;
		}
		if (dailyResetTime != 0) {
			return static_cast<long>(dailyResetTime) == currentDay;
		}
		return true; // no reset info available, assume same day
	};

	if (mqttRestoreData.hasDailyEnergy) {
		if (dailyResetMatches()) {
			dailyEnergy_kWh += mqttRestoreData.dailyEnergy;
			_logUsermodInaSensor("Applied daily energy from MQTT: +%.6f kWh => %.6f kWh",
				mqttRestoreData.dailyEnergy, dailyEnergy_kWh);
			if (mqttRestoreData.hasDailyResetTime) {
				dailyResetTime = mqttRestoreData.dailyResetTime;
			}
			if (mqttRestoreData.hasDailyResetTimestamp) {
				dailyResetTimestamp = mqttRestoreData.dailyResetTimestamp;
			}
		} else {
			long restoredDay = restoredDailyDay();
			if (restoredDay > currentDay) {
				dailyEnergy_kWh += mqttRestoreData.dailyEnergy;
				dailyResetTime = currentDay;
				dailyResetTimestamp = localTime - (localTime % 86400UL);
				_logUsermodInaSensor("Restored daily energy with future reset; clamping reset window to today.");
			} else {
				dailyResetTime = currentDay;
				dailyResetTimestamp = localTime - (localTime % 86400UL);
				_logUsermodInaSensor("Skipped daily MQTT restore (different day). Resetting daily window to today.");
			}
		}
	}

	auto restoredMonthlyId = [&]() {
		if (mqttRestoreData.hasMonthlyResetTime) {
			return static_cast<long>(mqttRestoreData.monthlyResetTime);
		}
		if (mqttRestoreData.hasMonthlyResetTimestamp) {
			return static_cast<long>(year(mqttRestoreData.monthlyResetTimestamp)) * 12L +
				static_cast<long>(month(mqttRestoreData.monthlyResetTimestamp)) - 1L;
		}
		return static_cast<long>(monthlyResetTime);
	};

	auto monthlyResetMatches = [&]() {
		if (mqttRestoreData.hasMonthlyResetTime) {
			return static_cast<long>(mqttRestoreData.monthlyResetTime) == currentMonth;
		}
		if (mqttRestoreData.hasMonthlyResetTimestamp) {
			long restoredMonth = year(mqttRestoreData.monthlyResetTimestamp) * 12 +
				month(mqttRestoreData.monthlyResetTimestamp) - 1;
			return restoredMonth == currentMonth;
		}
		if (monthlyResetTime != 0) {
			return static_cast<long>(monthlyResetTime) == currentMonth;
		}
		return true; // no reset info available, assume same month
	};

	if (mqttRestoreData.hasMonthlyEnergy) {
		if (monthlyResetMatches()) {
			monthlyEnergy_kWh += mqttRestoreData.monthlyEnergy;
			_logUsermodInaSensor("Applied monthly energy from MQTT: +%.6f kWh => %.6f kWh",
				mqttRestoreData.monthlyEnergy, monthlyEnergy_kWh);
			if (mqttRestoreData.hasMonthlyResetTime) {
				monthlyResetTime = mqttRestoreData.monthlyResetTime;
			}
			if (mqttRestoreData.hasMonthlyResetTimestamp) {
				monthlyResetTimestamp = mqttRestoreData.monthlyResetTimestamp;
			}
		} else {
			long restoredMonth = restoredMonthlyId();
			if (restoredMonth > currentMonth) {
				monthlyEnergy_kWh += mqttRestoreData.monthlyEnergy;
				monthlyResetTime = currentMonth;
				monthlyResetTimestamp = localTime - ((day(localTime) - 1) * 86400UL) - (localTime % 86400UL);
				_logUsermodInaSensor("Restored monthly energy with future reset; clamping reset window to current month.");
			} else {
				monthlyResetTime = currentMonth;
				monthlyResetTimestamp = localTime - ((day(localTime) - 1) * 86400UL) - (localTime % 86400UL);
				_logUsermodInaSensor("Skipped monthly MQTT restore (different month). Resetting monthly window to current month.");
			}
		}
	}

	mqttStateRestored = true;
	mqttRestorePending = false;
	mqttRestoreData = MqttRestoreData{};
	_logUsermodInaSensor("MQTT energy restore applied");
}

// Update INA2xx settings and reinitialize sensor if necessary
bool UsermodINA2xx::updateINA2xxSettings() {
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
	_ina2xx = new INA_SENSOR_CLASS(_i2cAddress);

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

	_logUsermodInaSensor("Setting correction factor to %.4f", correctionFactor);
	_ina2xx->setCorrectionFactor(correctionFactor);

#if INA_SENSOR_TYPE == 219
	_logUsermodInaSensor("Setting shunt resistor to %.4f Ohms", shuntResistor);
	_ina2xx->setShuntSizeInOhms(shuntResistor);

	_logUsermodInaSensor("Setting ADC mode to %d", conversionTime);
	_ina2xx->setADCMode(conversionTime);

	_logUsermodInaSensor("Setting PGA gain to %d", pGain);
	_ina2xx->setPGain(pGain);

	_logUsermodInaSensor("Setting bus range to %d", busRange);
	_ina2xx->setBusRange(busRange);

	_logUsermodInaSensor("Setting shunt voltage offset to %.3f mV", shuntVoltOffset_mV);
	_ina2xx->setShuntVoltOffset_mV(shuntVoltOffset_mV);

#elif INA_SENSOR_TYPE == 226

	_ina2xx->setMeasureMode(CONTINUOUS);
	_ina2xx->setAverage(average); 
	_ina2xx->setConversionTime(conversionTime);
	_ina2xx->setResistorRange(shuntResistor,currentRange); // choose resistor 100 mOhm (default )and gain range up to 10 A (1.3A default)

	_ina2xx->readAndClearFlags();

	_ina2xx->waitUntilConversionCompleted(); //if you comment this line the first data might be zero
#endif

	_logUsermodInaSensor("INA2xx sensor configured successfully");
	return true;
}

// Sanitize the mqttClientID by replacing invalid characters.
String UsermodINA2xx::sanitizeMqttClientID(const String &clientID) {
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
void UsermodINA2xx::updateEnergy(float power, unsigned long durationMs) {
	float durationHours = durationMs / 3600000.0; // Milliseconds to hours
	_logUsermodInaSensor("Updating energy - Power: %.3f W, Duration: %lu ms (%.6f hours)", power, durationMs, durationHours);

	// Skip time-based resets if time seems invalid (before 2020 or unrealistic future)
	if (!isTimeValid()) { // Jan 1 2020 to Jan 1 2100
		_logUsermodInaSensor("SKIPPED: Invalid time detected (%lu), waiting for NTP sync", localTime);
		return;
	}

	float energy_kWh = 0.0f;
	if (power < 0.01f) {
		_logUsermodInaSensor("Power too low (%.3f W) – skipping accumulation.", power);
	} else {
		energy_kWh = (power / 1000.0f) * durationHours; // Watts to kilowatt-hours (kWh)
		_logUsermodInaSensor("Calculated energy: %.6f kWh", energy_kWh);

		// Skip negative values or unrealistic spikes
		if (energy_kWh < 0 || energy_kWh > 10.0f) { // 10 kWh in a few seconds is unrealistic
			_logUsermodInaSensor("SKIPPED: Energy value out of range (%.6f kWh)", energy_kWh);
			energy_kWh = 0.0f;
		}
	}

	if (energy_kWh > 0.0f) {
		totalEnergy_kWh += energy_kWh; // Update total energy consumed
		_logUsermodInaSensor("Total energy updated to: %.6f kWh", totalEnergy_kWh);

		// Sanity check on accumulated total (e.g., 100,000 kWh = ~11 years at 1kW continuous)
		if (totalEnergy_kWh > 100000.0f) {
			_logUsermodInaSensor("WARNING: Total energy suspiciously high (%.6f kWh), possible corruption", totalEnergy_kWh);
		}
	}

	// Calculate day identifier (days since epoch)
	long currentDay = localTime / 86400;
	_logUsermodInaSensor("Current day: %ld, Last reset day: %lu", currentDay, dailyResetTime);

	// --- initialize reset values if they are unset (first run) ---
	if (dailyResetTime == 0) {
		dailyResetTime = currentDay;
		// Calculate midnight timestamp for current day
		dailyResetTimestamp = localTime - (localTime % 86400UL);
		_logUsermodInaSensor("Initializing daily reset: day=%ld, ts=%lu", dailyResetTime, dailyResetTimestamp);
	}

	// Fix for missing dailyResetTimestamp when dailyResetTime was already restored
	if (dailyResetTimestamp == 0 && dailyResetTime != 0) {
		dailyResetTimestamp = localTime - (localTime % 86400UL);
		_logUsermodInaSensor("Fixing missing daily reset timestamp: ts=%lu", dailyResetTimestamp);
	}

	// Reset daily energy at midnight or if day changed
	if (currentDay > dailyResetTime) {
	//if ((hour(localTime) == 0 && minute(localTime) == 0 && dailyResetTime != currentDay) ||
		//(currentDay > dailyResetTime && dailyResetTime > 0)) {
		_logUsermodInaSensor("Resetting daily energy counter (day change detected)");
		dailyEnergy_kWh = 0;
		dailyResetTime = currentDay;
		// Set timestamp to midnight of the current day
		dailyResetTimestamp = localTime - (localTime % 86400UL);
	}
	dailyEnergy_kWh += energy_kWh;
	_logUsermodInaSensor("Daily energy updated to: %.6f kWh", dailyEnergy_kWh);

	// Calculate month identifier (year*12 + month)
	long currentMonth = year(localTime) * 12 + month(localTime) - 1; // month() is 1-12
	_logUsermodInaSensor("Current month: %ld, Last reset month: %lu", currentMonth, monthlyResetTime);

	if (monthlyResetTime == 0) {
		monthlyResetTime = currentMonth;
		// Calculate midnight timestamp for first day of current month
		// Formula: subtract (current_day - 1) days worth of seconds, then subtract time-of-day
		monthlyResetTimestamp = localTime - ((day(localTime) - 1) * 86400UL) - (localTime % 86400UL);
		//monthlyResetTimestamp = localTime - (localTime % 86400UL); // midnight seconds (first of month)
		_logUsermodInaSensor("Initializing monthly reset: month=%ld, ts=%lu", monthlyResetTime, monthlyResetTimestamp);
	}

	// Fix for missing monthlyResetTimestamp when monthlyResetTime was already restored
	if (monthlyResetTimestamp == 0 && monthlyResetTime != 0) {
		monthlyResetTimestamp = localTime - ((day(localTime) - 1) * 86400UL) - (localTime % 86400UL);
		_logUsermodInaSensor("Fixing missing monthly reset timestamp: ts=%lu", monthlyResetTimestamp);
	}

	// Reset monthly energy on first day of month or if month changed
	if (currentMonth > monthlyResetTime) {
	//if ((day(localTime) == 1 && hour(localTime) == 0 && minute(localTime) == 0 &&
			//monthlyResetTime != currentMonth) || (currentMonth > monthlyResetTime && monthlyResetTime > 0)) {
		_logUsermodInaSensor("Resetting monthly energy counter (month change detected)");
		monthlyEnergy_kWh = 0;
		monthlyResetTime = currentMonth;

		// Calculate midnight timestamp for first day of current month
		// Formula: subtract (current_day - 1) days worth of seconds, then subtract time-of-day
		monthlyResetTimestamp = localTime - ((day(localTime) - 1) * 86400UL) - (localTime % 86400UL);

		//monthlyResetTimestamp = localTime - (localTime % 86400UL);
	}
	monthlyEnergy_kWh += energy_kWh;
	_logUsermodInaSensor("Monthly energy updated to: %.6f kWh", monthlyEnergy_kWh);
}

#ifndef WLED_DISABLE_MQTT
/**
** Function to publish INA2xx sensor data to MQTT
**/
void UsermodINA2xx::publishMqtt(float shuntVoltage, float busVoltage, float loadVoltage,
				float current, float current_mA, float power,
				float power_mW, bool overflow) {
	if (!WLED_MQTT_CONNECTED) {
		_logUsermodInaSensor("MQTT not connected, skipping publish");
		return;
	}

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

		jsonDoc["dailyResetTime"] = dailyResetTime;
		jsonDoc["monthlyResetTime"] = monthlyResetTime;
		jsonDoc["dailyResetTimestamp"] = dailyResetTimestamp;
		jsonDoc["monthlyResetTimestamp"] = monthlyResetTimestamp;
	} else {
		_logUsermodInaSensor("Skipping energy fields until MQTT state restored");
	}

	// Reset timestamps
	//if (dailyResetTime > 0) {
		//jsonDoc["dailyResetTime"] = dailyResetTime;
	//}
	//if (monthlyResetTime > 0) {
		//jsonDoc["monthlyResetTime"] = monthlyResetTime;
	//}

	// Serialize the JSON document into a character buffer
	char buffer[1024];
	size_t payload_size = serializeJson(jsonDoc, buffer, sizeof(buffer));

	if (payload_size >= sizeof(buffer) - 1) {
		_logUsermodInaSensor("JSON serialization truncated – buffer too small (%u bytes)", sizeof(buffer));
	}

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
void UsermodINA2xx::mqttCreateHassSensor(const String &name, const String &topic,
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
	doc[F("val_tpl")] = String("{{ value_json.") + jsonKey + String(" }}");
	if (unitOfMeasurement != "")
		doc[F("unit_of_meas")] = unitOfMeasurement;
	if (deviceClass != "")
		doc[F("dev_cla")] = deviceClass;
	if (SensorType != "binary_sensor")
		doc[F("exp_aft")] = 1800;

	// --- set appropriate state_class and last_reset for energy/measurement sensors ---
	if (jsonKey == "total_energy_kWh") {
		// total energy never resets -> total_increasing
		doc[F("stat_cla")] = "total_increasing";
	} else if (jsonKey == "daily_energy_kWh" || jsonKey == "monthly_energy_kWh") {
		// daily/monthly energy resets -> use "total" with last_reset_value_template
		doc[F("stat_cla")] = "total";

		// Provide last_reset for daily/monthly
		if (jsonKey == "daily_energy_kWh") {
			doc[F("last_reset_value_template")] = "{{ value_json.dailyResetTimestamp | int | timestamp_local if value_json.dailyResetTimestamp | int > 0 else none }}";
		} else if (jsonKey == "monthly_energy_kWh") {
			doc[F("last_reset_value_template")] = "{{ value_json.monthlyResetTimestamp | int | timestamp_local if value_json.monthlyResetTimestamp | int > 0 else none }}";
		}
	}
	else if (jsonKey == "current_A" || jsonKey == "current_mA" ||
			jsonKey == "power_W" || jsonKey == "power_mW" ||
			jsonKey == "bus_voltage_V" || jsonKey == "load_voltage_V" ||
			jsonKey == "shunt_voltage_mV") {
		doc[F("stat_cla")] = "measurement";
	}

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

	if (payload_size >= sizeof(buffer) - 1) {
		_logUsermodInaSensor("HA config JSON truncated – buffer too small (%u bytes)", sizeof(buffer));
	}

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

void UsermodINA2xx::mqttRemoveHassSensor(const String &name, const String &SensorType) {
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

/**
** Function to publish sensor data to MQTT
**/
bool UsermodINA2xx::onMqttMessage(char* topic, char* payload) {
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
			mqttRestoreData = MqttRestoreData{};

			if (jsonDoc.containsKey("daily_energy_kWh")) {
				float restored = jsonDoc["daily_energy_kWh"];
				if (!isnan(restored) && restored >= 0) {
					mqttRestoreData.hasDailyEnergy = true;
					mqttRestoreData.dailyEnergy = restored;
				}
			}
			if (jsonDoc.containsKey("monthly_energy_kWh")) {
				float restored = jsonDoc["monthly_energy_kWh"];
				if (!isnan(restored) && restored >= 0) {
					mqttRestoreData.hasMonthlyEnergy = true;
					mqttRestoreData.monthlyEnergy = restored;
				}
			}
			if (jsonDoc.containsKey("total_energy_kWh")) {
				float restored = jsonDoc["total_energy_kWh"];
				if (!isnan(restored) && restored >= 0) {
					mqttRestoreData.hasTotalEnergy = true;
					mqttRestoreData.totalEnergy = restored;
				}
			}
			if (jsonDoc.containsKey("dailyResetTime")) {
				uint32_t restored = jsonDoc["dailyResetTime"].as<uint32_t>();
				if (restored > 0 && restored < 1000000) { // reasonable day count since epoch
					mqttRestoreData.hasDailyResetTime = true;
					mqttRestoreData.dailyResetTime = restored;
				}
			}
			if (jsonDoc.containsKey("monthlyResetTime")) {
				uint32_t restored = jsonDoc["monthlyResetTime"].as<uint32_t>();
				if (restored > 0 && restored < 100000) { // reasonable month count
					mqttRestoreData.hasMonthlyResetTime = true;
					mqttRestoreData.monthlyResetTime = restored;
				}
			}
			if (jsonDoc.containsKey("dailyResetTimestamp")) {
				uint32_t restored = jsonDoc["dailyResetTimestamp"].as<uint32_t>();
				if (restored >= 1577836800UL && restored <= 4102444800UL) {
					mqttRestoreData.hasDailyResetTimestamp = true;
					mqttRestoreData.dailyResetTimestamp = restored;
				}
			}
			if (jsonDoc.containsKey("monthlyResetTimestamp")) {
				uint32_t restored = jsonDoc["monthlyResetTimestamp"].as<uint32_t>();
				if (restored >= 1577836800UL && restored <= 4102444800UL) {
					mqttRestoreData.hasMonthlyResetTimestamp = true;
					mqttRestoreData.monthlyResetTimestamp = restored;
				}
			}

			mqttRestorePending = true;
			applyMqttRestoreIfReady();
		}
		return true;
	}
	return false;
}

/**
** Subscribe to MQTT topic for controlling the usermod
**/
void UsermodINA2xx::onMqttConnect(bool sessionPresent) {
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

// Destructor to clean up INA2xx object
UsermodINA2xx::~UsermodINA2xx() {
	if (_ina2xx) {
		_logUsermodInaSensor("Cleaning up INA2xx sensor object");
		delete _ina2xx;
		_ina2xx = nullptr;
	}
}

// Setup function called once on boot or restart
void UsermodINA2xx::setup() {
	_logUsermodInaSensor("Setting up INA2xx sensor usermod");
	initDone = updateINA2xxSettings();  // Configure INA2xx settings
	if (initDone) {
		_logUsermodInaSensor("INA2xx setup complete and successful");
	} else {
		_logUsermodInaSensor("INA2xx setup failed");
	}
}

#if INA_SENSOR_TYPE == 226
void UsermodINA2xx::checkForI2cErrors(){
  byte errorCode = _ina2xx->getI2cErrorCode();
  if(errorCode){
    Serial.print("I2C error: ");
    Serial.println(errorCode);
    _logUsermodInaSensor("I2C error: %u", errorCode);
    switch(errorCode){
      case 1:
		_logUsermodInaSensor("Data too long to fit in transmit buffer");
        break;
      case 2:
		_logUsermodInaSensor("Received NACK on transmit of address");
        break;
      case 3: 
		_logUsermodInaSensor("Received NACK on transmit of data");
        break;
      case 4:
		_logUsermodInaSensor("Other error");
        break;
      case 5:
		_logUsermodInaSensor("Timeout");
        break;
      default: 
		_logUsermodInaSensor("Can't identify the error");
    }
    if(errorCode){
		enabled = false;
		initDone = false;
		_logUsermodInaSensor("Disabling INA2xx usermod after I2C error");
    }
  }
}
#endif

// Loop function called continuously
void UsermodINA2xx::loop() {
	applyMqttRestoreIfReady();

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

#if INA_SENSOR_TYPE == 219
	overflow = _ina2xx->getOverflow() != 0;
#elif INA_SENSOR_TYPE == 226
	overflow     = _ina2xx->overflow;
	checkForI2cErrors();
#endif

	_logUsermodInaSensor("Sensor readings - Shunt: %.3f mV, Bus: %.3f V, Load: %.3f V", shuntVoltage, busVoltage, loadVoltage);
	_logUsermodInaSensor("Sensor readings - Current: %.3f A (%.3f mA), Power: %.3f W (%.3f mW)", current, current_mA, power, power_mW);
	_logUsermodInaSensor("Overflow status: %s", overflow ? "TRUE" : "FALSE");

	// Update energy consumption
	if (lastPublishTime != 0) {
		if (power >= 0) {
			// Handle millis() overflow when calculating duration
			unsigned long duration = (lastCheck >= lastPublishTime)
					? (lastCheck - lastPublishTime)
					: (0xFFFFFFFFUL - lastPublishTime + lastCheck + 1);
			updateEnergy(power, duration);
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
			bool valueChanged = hasValueChanged();
			if (mqttPublishAlways || valueChanged) {
				_logUsermodInaSensor("Publishing MQTT data (always=%d, changed=%d)", mqttPublishAlways, valueChanged);
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
			mqttRemoveHassSensor(F("Shunt Resistor"), F("sensor"));
			mqttRemoveHassSensor(F("Overflow"), F("sensor"));
			mqttRemoveHassSensor(F("Daily Energy"), F("sensor"));
			mqttRemoveHassSensor(F("Monthly Energy"), F("sensor"));
			mqttRemoveHassSensor(F("Total Energy"), F("sensor"));

			haDiscoverySent = false; // Mark as sent to avoid repeating
			_logUsermodInaSensor("Home Assistant discovery removal complete");
		}
	}
#endif
}

/**
** Add energy consumption data to a JSON object for reporting
**/
void UsermodINA2xx::addToJsonInfo(JsonObject &root) {
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
void UsermodINA2xx::addToJsonState(JsonObject& root) {
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
	usermod["dailyResetTimestamp"]   = dailyResetTimestamp;
	usermod["monthlyResetTimestamp"] = monthlyResetTimestamp;

	_logUsermodInaSensor("Added sensor readings to JSON state: V=%.3fV, I=%.3fA, P=%.3fW", loadVoltage, current, power);
}

/**
** Read energy consumption data from a JSON object
**/
void UsermodINA2xx::readFromJsonState(JsonObject& root) {
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
		if (usermod.containsKey("dailyResetTimestamp")) {
			unsigned long prevDailyRT = dailyResetTimestamp;
			dailyResetTimestamp = usermod["dailyResetTimestamp"] | dailyResetTimestamp;
			if (dailyResetTimestamp != prevDailyRT) {
				_logUsermodInaSensor("Daily reset timestamp updated from JSON: %lu", dailyResetTimestamp);
			}
		}
		if (usermod.containsKey("monthlyResetTimestamp")) {
			unsigned long prevMonthlyRT = monthlyResetTimestamp;
			monthlyResetTimestamp = usermod["monthlyResetTimestamp"] | monthlyResetTimestamp;
			if (monthlyResetTimestamp != prevMonthlyRT) {
				_logUsermodInaSensor("Monthly reset timestamp updated from JSON: %lu", monthlyResetTimestamp);
			}
		}
	} else {
		_logUsermodInaSensor("No usermod data found in JSON state");
	}
}

/**
** Append configuration options to the Usermod menu.
**/
void UsermodINA2xx::addToConfig(JsonObject& root) {
	JsonObject top = root.createNestedObject(F("INA2xx"));
	top["Enabled"] = enabled;
	top["i2c_address"] = static_cast<uint8_t>(_i2cAddress);
	top["check_interval"] = checkInterval / 1000;
	top["conversion_time"] = conversionTime;
	top["decimals"] = _decimalFactor;
	top["shunt_resistor"] = shuntResistor;
	top["correction_factor"] = correctionFactor;
#if INA_SENSOR_TYPE == 219
	top["pga_gain"]        = pGain;
	top["bus_range"]       = busRange;
	top["shunt_offset"]    = shuntVoltOffset_mV;
#elif INA_SENSOR_TYPE == 226
	top["average"]    = average;
	top["currentRange"]    = currentRange;  
#endif

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
void UsermodINA2xx::appendConfigData() {
	// Append the dropdown for I2C address selection
	oappend("dd=addDropdown('INA2xx','i2c_address');");
	oappend("addOption(dd,'0x40 - Default',0x40, true);");
	oappend("addOption(dd,'0x41 - A0 soldered',0x41);");
	oappend("addOption(dd,'0x44 - A1 soldered',0x44);");
	oappend("addOption(dd,'0x45 - A0 and A1 soldered',0x45);");

	// Append the dropdown for decimal precision (0 to 3)
	oappend("df=addDropdown('INA2xx','decimals');");
	for (int i = 0; i <= 3; i++) {
		oappend(String("addOption(df,'" + String(i) + "'," + String(i) + (i == _decimalFactor ? ", true);" : ");")).c_str());
	}

#if INA_SENSOR_TYPE == 219
	// Append the dropdown for ADC mode (resolution + samples)
	oappend("ct=addDropdown('INA2xx','conversion_time');");
	oappend("addOption(ct,'9-Bit (84 µs)',0);");
	oappend("addOption(ct,'10-Bit (148 µs)',1);");
	oappend("addOption(ct,'11-Bit (276 µs)',2);");
	oappend("addOption(ct,'12-Bit (532 µs) (default)',3, true);");
	oappend("addOption(ct,'2 samples (1.06 ms)',9);");
	oappend("addOption(ct,'4 samples (2.13 ms)',10);");
	oappend("addOption(ct,'8 samples (4.26 ms)',11);");
	oappend("addOption(ct,'16 samples (8.51 ms)',12);");
	oappend("addOption(ct,'32 samples (17.02 ms)',13);");
	oappend("addOption(ct,'64 samples (34.05 ms)',14);");
	oappend("addOption(ct,'128 samples (68.10 ms)',15);");
	
	oappend("pg=addDropdown('INA2xx','pga_gain');");
	oappend("addOption(pg,'40mV',0);");
	oappend("addOption(pg,'80mV',2048);");
	oappend("addOption(pg,'160mV',4096);");
	oappend("addOption(pg,'320mV (default)',6144, true);");

	oappend("br=addDropdown('INA2xx','bus_range');");
	oappend("addOption(br,'16V',0);");
	oappend("addOption(br,'32V (default)',8192, true);");
#elif INA_SENSOR_TYPE == 226
	oappend("ct=addDropdown('INA2xx','conversion_time');");
	oappend("addOption(ct,'140 µs',0);");
	oappend("addOption(ct,'204 µs',1);");
	oappend("addOption(ct,'332 µs',2);");
	oappend("addOption(ct,'588 µs',3);");
	oappend("addOption(ct,'1.1 ms (default)',4, true);");
	oappend("addOption(ct,'2.116 ms',5);");
	oappend("addOption(ct,'4.156 ms',6);");
	oappend("addOption(ct,'8.244 ms',7);");

	oappend("dda=addDropdown('INA2xx','average');");
	oappend("addOption(dda,'1 (default)',0, true);");
	oappend("addOption(dda,'4',512);");
	oappend("addOption(dda,'16',1024);");
	oappend("addOption(dda,'64',1536);");
	oappend("addOption(dda,'128',2048);");
	oappend("addOption(dda,'256',2560);");
	oappend("addOption(dda,'512',3072);");
	oappend("addOption(dda,'1024',3584);");

	oappend("df = addDropdown('INA2xx','currentRange');");
	for (int i = 1; i <= 100; i++) {
		float amp = i * 0.1f;               // 0.1, 0.2, …, 10.0
		String strVal = String(amp, 1);    // “0.1”, “0.2”, …, “1.3”, …, “10.0”

		// Make “1.3” the default
		bool selected = (fabs(amp - 1.3f) < 0.001f);

		// Build the label: e.g. “1.3A (default)” or “3.7A”
		String label = strVal + "A";
		if (selected) label += " (default)";

		// addOption(df,'3.7A',3.7);
		String line = String("addOption(df,'")
			+ label
			+ "',"
			+ strVal
			+ (selected ? ", true);" : ");");
		oappend(line.c_str());
	}
#endif
}

/**
** Read settings from the Usermod menu configuration
**/
bool UsermodINA2xx::readFromConfig(JsonObject& root) {
	JsonObject top = root[FPSTR(_name)];

	bool configComplete = !top.isNull();

	_logUsermodInaSensor("Checking if configuration has changed:");
	UPDATE_CONFIG(top, "Enabled", enabled, "%u");
	UPDATE_CONFIG(top, "i2c_address",       _i2cAddress,       "0x%02X");
	UPDATE_CONFIG(top, "conversion_time",    conversionTime,    "%u");
	UPDATE_CONFIG(top, "decimals",           _decimalFactor,    "%u");
	UPDATE_CONFIG(top, "shunt_resistor",     shuntResistor,     "%.6f Ohms");
	UPDATE_CONFIG(top, "correction_factor",  correctionFactor,  "%.3f");
#if INA_SENSOR_TYPE == 219
	UPDATE_CONFIG(top, "pga_gain",           pGain,             "%d");
	UPDATE_CONFIG(top, "bus_range",          busRange,          "%d");
	UPDATE_CONFIG(top, "shunt_offset",       shuntVoltOffset_mV,"%.3f mV");
#elif INA_SENSOR_TYPE == 226
	UPDATE_CONFIG(top, "average", average, "%d");
	UPDATE_CONFIG(top, "currentRange", currentRange, "%.1f");
#endif

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
			_logUsermodInaSensor("Invalid check_interval value %u; using default %u seconds", tempInterval, INA2XX_CHECK_INTERVAL);
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
uint16_t UsermodINA2xx::getId() {
	return USERMOD_ID_INA2XX;
}
