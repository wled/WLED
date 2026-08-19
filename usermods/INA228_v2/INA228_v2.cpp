#include "wled.h"
#include <INA228.h>

/*
 * INA228_v2 Usermod for WLED
 * 
 * This usermod integrates the INA228 power monitoring IC to measure:
 * - Bus Voltage (up to 85V)
 * - Current (bidirectional through shunt resistor)
 * - Temperature (±1°C accuracy)
 * 
 * Based on RobTillaart's INA228 library
 * See: https://github.com/RobTillaart/INA228
 * 
 * All configuration is managed through WLED's usermod settings page.
 * Default I2C address is 0x40.
 * Register addresses and constants are defined in the INA228 library.
 */

// Global tachometer pulse counter for ISR (must be in DRAM for IRAM ISR access)
static volatile uint16_t DRAM_ATTR g_tachPulseCount = 0;

// ISR for tachometer (must be outside class and in IRAM)
static void IRAM_ATTR tachISR() {
	g_tachPulseCount++;
}

class UsermodINA228 : public Usermod {

private:
	static const char _name[];

	// INA228 sensor object (RobTillaart's INA228 library)
	INA228 *_ina228 = nullptr;

	// Settings (configurable via WLED settings page)
	bool _settingEnabled = true;
	uint8_t _i2cAddress = 0x40;      // Default I2C address for INA228
	float _shuntResistor = 0.015;     // 15 milliohms (typical for breakout boards)
	float _maxCurrent = 1.5;          // Max expected current in Amps
	uint16_t _checkInterval = 1000;   // milliseconds
	uint8_t _averageSamples = 4;      // 0=1, 1=4, 2=16, 3=64, 4=128, 5=256, 6=512, 7=1024
	
	// Fan control settings
	bool _fanEnabled = false;         // Enable temperature-controlled PWM fan
	int8_t _fanPwmPin = -1;           // PWM output pin for fan control
	int8_t _fanTachoPin = -1;         // Tachometer input pin (optional)
	float _fanStartTemp = 30.0f;      // Temperature to start fan (°C)
	float _fanMaxTemp = 50.0f;        // Temperature for max fan speed (°C)
	
	// Emergency shutdown settings
	bool _shutdownEnabled = false;    // Enable emergency shutdown
	int8_t _alertPin = -1;            // INA228 ALERT pin (active low)
	float _tempLimit = 85.0f;         // Temperature limit for shutdown (°C, 0 = disabled)
	float _powerLimit = 25.0f;         // Power limit for shutdown (W, 0 = disabled)
	bool _shutdownTriggered = false;  // Shutdown state flag
	const char* _shutdownReason = nullptr;  // Reason for shutdown ("INA228 Alert Pin" or "Firmware Check")
	float _shutdownTemp = 0.0f;       // Temperature at time of shutdown
	float _shutdownPower = 0.0f;      // Power at time of shutdown
	char _shutdownTime[25] = {0};     // Formatted time string when shutdown occurred
	
	// Buzzer settings for shutdown alert
	bool _buzzerEnabled = false;      // Enable buzzer alarm
	int8_t _buzzerPin = -1;           // GPIO pin for piezo buzzer (-1 to disable)
	uint16_t _buzzerFrequency = 2000; // Beep frequency in Hz
	uint16_t _buzzerDuration = 500;   // Beep duration in ms (also used for pause)
	uint8_t _buzzerBeepCount = 3;     // Number of beeps to make
	
	// Buzzer state tracking for non-blocking operation
	bool _buzzerActive = false;       // Whether buzzer is currently beeping
	uint8_t _buzzerBeepIndex = 0;     // Current beep number
	unsigned long _buzzerStartTime = 0; // When buzzer started
	bool _buzzerInPause = false;      // Whether in pause between beeps
	
	// State tracking
	bool _initDone = false;
	unsigned long _lastReadTime = 0;
	uint8_t _lastStatus = 0;
	bool _alertConfigured = false;    // Whether INA228 alert is configured
	
	// Previous settings for change detection (initialized in readFromConfig)
	uint8_t _prevI2cAddress = 0x40;
	float _prevShuntResistor = 0.015f;
	float _prevMaxCurrent = 1.5f;
	uint8_t _prevAverageSamples = 4;
	float _prevTempLimit = 85.0f;
	float _prevPowerLimit = 0.0f;
	
	// Last read values
	float _lastBusVoltage = 0.0f;
	float _lastCurrent = 0.0f;
	float _lastPower = 0.0f;
	float _lastTemperature = 0.0f;
	uint8_t _lastFanDutyCycle = 0;    // Last PWM duty cycle (0-255)
	uint8_t _lastFanSpeedPercent = 0; // Fan speed percentage for display
	
	// Tachometer (RPM) tracking
	uint16_t _fanRPM = 0;  // Current fan RPM
	
	// Utility function to truncate to 2 decimal places
	float truncateDecimals(float val) {
		return roundf(val * 100.0f) / 100.0f;
	}
	
	// Check if critical INA228 settings have changed
	bool hasSettingsChanged() {
		return (_i2cAddress != _prevI2cAddress ||
		        _shuntResistor != _prevShuntResistor ||
		        _maxCurrent != _prevMaxCurrent ||
		        _averageSamples != _prevAverageSamples ||
		        _tempLimit != _prevTempLimit ||
		        _powerLimit != _prevPowerLimit);
	}
	
	// Update previous settings after reinitialization
	void updatePreviousSettings() {
		_prevI2cAddress = _i2cAddress;
		_prevShuntResistor = _shuntResistor;
		_prevMaxCurrent = _maxCurrent;
		_prevAverageSamples = _averageSamples;
		_prevTempLimit = _tempLimit;
		_prevPowerLimit = _powerLimit;
	}

	// Calculate RPM from pulse count
	void calculateRPM() {
		if (_fanTachoPin < 0) return;
		
		// Get pulse count and reset
		noInterrupts();
		uint16_t pulses = g_tachPulseCount;
		g_tachPulseCount = 0;
		interrupts();
		
		// Calculate RPM: 2 pulses per rotation, multiply by intervals per minute
		// RPM = (pulses / 2) * (60000 / _checkInterval)
		_fanRPM = (pulses * 60000UL) / (_checkInterval * 2);
		
		// Debug: Log pulse count
		DEBUG_PRINT(F("INA228: Tach pulses this interval: "));
		DEBUG_PRINTLN(pulses);
	}	
	// Sound buzzer alarm for shutdown (non-blocking state machine)
	void initiateBuzzerAlarm() {
		if (!_buzzerEnabled || _buzzerPin < 0 || _buzzerBeepCount == 0) return;
		
		// Start the buzzer sequence
		_buzzerActive = true;
		_buzzerBeepIndex = 0;
		_buzzerInPause = false;
		_buzzerStartTime = millis();
		
		// Begin first beep
		tone(_buzzerPin, _buzzerFrequency, _buzzerDuration);
	}
	
	// Update buzzer state machine (call from loop)
	void updateBuzzerState() {
		if (!_buzzerActive || _buzzerPin < 0) return;
		
		unsigned long elapsed = millis() - _buzzerStartTime;
		
		if (!_buzzerInPause) {
			// Currently in beep phase
			if (elapsed >= _buzzerDuration) {
				_buzzerBeepIndex++;
				
				if (_buzzerBeepIndex >= _buzzerBeepCount) {
					// All beeps done
					_buzzerActive = false;
					noTone(_buzzerPin);
					return;
				}
				
				// Start pause before next beep
				_buzzerInPause = true;
				_buzzerStartTime = millis();
			}
		} else {
			// Currently in pause phase
			if (elapsed >= _buzzerDuration) {
				// Start next beep
				_buzzerInPause = false;
				_buzzerStartTime = millis();
				tone(_buzzerPin, _buzzerFrequency, _buzzerDuration);
			}
		}
	}
	// Initialize the INA228 sensor
	void initializeINA228() {
		if (_ina228 != nullptr) {
			delete _ina228;
			_ina228 = nullptr;
		}

		// Create new INA228 instance
		_ina228 = new INA228(_i2cAddress, &Wire);
		
		if (!_ina228->begin()) {
			DEBUG_PRINTLN(F("INA228: Initialization failed!"));
			return;
		}
		
		// Update tracked previous settings
		updatePreviousSettings();

		// Configure the sensor
		_ina228->setMaxCurrentShunt(_maxCurrent, _shuntResistor);
		_ina228->setAverage(_averageSamples);
		_ina228->setMode(INA228_MODE_CONT_TEMP_BUS_SHUNT);  // Continuous mode for all measurements
		
		// Configure emergency shutdown alerts if enabled
		_alertConfigured = false;
		if (_shutdownEnabled && (_tempLimit > 0 || _powerLimit > 0)) {
			// Enable alert latching (bit 15 of diagnose/alert register)
			// This makes the ALERT pin stay LOW until manually cleared
			uint16_t diagAlert = _ina228->getDiagnoseAlert();
			diagAlert |= (1 << 15); // Set ALERT_LATCH bit
			_ina228->setDiagnoseAlert(diagAlert);
			DEBUG_PRINTLN(F("INA228: Alert latching enabled"));
			
			// Set temperature over-limit if configured
			// Temperature LSB = 7.8125e-3 °C
			if (_tempLimit > 0) {
				uint16_t tempThreshold = (uint16_t)(_tempLimit / 7.8125e-3);
				_ina228->setTemperatureOverLimitTH(tempThreshold);
				DEBUG_PRINT(F("INA228: Temperature alert set to "));
				DEBUG_PRINT(_tempLimit);
				DEBUG_PRINTLN(F(" °C"));
			}
			// Set power over-limit if configured
			// Power threshold = Power / (256 × 3.2 × current_LSB)
			if (_powerLimit > 0) {
				// Calculate power threshold register value
				float powerLSB = 3.2 * (_maxCurrent / 524288.0); // current_LSB calculation
				uint16_t powerThreshold = (uint16_t)(_powerLimit / (256.0 * powerLSB));
				_ina228->setPowerOverLimitTH(powerThreshold);
				DEBUG_PRINT(F("INA228: Power alert set to "));
				DEBUG_PRINT(_powerLimit);
				DEBUG_PRINTLN(F(" W"));
			}
			_alertConfigured = true;
		}
		
		DEBUG_PRINTLN(F("INA228: Initialized successfully"));
	}

	// Read sensor values
	void readSensorValues() {
		if (_ina228 == nullptr || !_settingEnabled) return;

		_lastBusVoltage = truncateDecimals(_ina228->getBusVoltage());
		_lastCurrent = truncateDecimals(_ina228->getCurrent());
		_lastPower = truncateDecimals(_ina228->getPower());
		_lastTemperature = truncateDecimals(_ina228->getTemperature());
		
		_lastReadTime = millis();
	}

	// Update PWM fan speed based on temperature
	void updateFanSpeed() {
		if (!_fanEnabled || _fanPwmPin < 0) {
			_lastFanDutyCycle = 0;
			_lastFanSpeedPercent = 0;
			return;
		}

		float temp = _lastTemperature;
		uint8_t dutyCycle = 0;

		if (temp <= _fanStartTemp) {
			// Below start threshold: fan off
			dutyCycle = 0;
		} else if (temp >= _fanMaxTemp) {
			// Above max threshold: full speed
			dutyCycle = 255;
		} else {
			// Between thresholds: linear interpolation
			float range = _fanMaxTemp - _fanStartTemp;
			float tempAboveMin = temp - _fanStartTemp;
			dutyCycle = (uint8_t)((tempAboveMin / range) * 255.0f);
		}

		// Apply PWM duty cycle
		analogWrite(_fanPwmPin, dutyCycle);
		_lastFanDutyCycle = dutyCycle;
		_lastFanSpeedPercent = (uint8_t)((dutyCycle * 100) / 255);
	}

	// Reset the INA228 and clear alert state
	void resetAlert() {
		if (_ina228 == nullptr) return;
		
		DEBUG_PRINTLN(F("INA228: Resetting sensor and clearing alert"));
		
		// Reset the INA228 device (this clears all registers including alert)
		_ina228->reset();
		delay(10); // Small delay after reset
		
		// Recalibrate the sensor
		_ina228->setMaxCurrentShunt(_maxCurrent, _shuntResistor);
		_ina228->setAverage(_averageSamples);
		_ina228->setMode(INA228_MODE_CONT_TEMP_BUS_SHUNT);
		
		// Reconfigure shutdown thresholds if enabled
		if (_shutdownEnabled && (_tempLimit > 0 || _powerLimit > 0)) {
			_ina228->setADCRange(false); // 164 mV range
			float current_LSB = _maxCurrent * 1.9073486328125e-6;
			
			if (_tempLimit > 0) {
				uint16_t tempThreshold = (uint16_t)(_tempLimit / 7.8125e-3);
				_ina228->setTemperatureOverLimitTH(tempThreshold);
			}
			
			if (_powerLimit > 0) {
				float powerLSB = 3.2 * current_LSB;
				uint16_t powerThreshold = (uint16_t)(_powerLimit / (256.0 * powerLSB));
				_ina228->setPowerOverLimitTH(powerThreshold);
			}
		}
		
		// Clear usermod shutdown state
		_shutdownTriggered = false;
		_shutdownReason = nullptr;
		_shutdownTemp = 0.0f;
		_shutdownPower = 0.0f;
		_shutdownTime[0] = 0;
		
		DEBUG_PRINTLN(F("INA228: Reset complete"));
	}

	// Check for emergency shutdown conditions
	void checkEmergencyShutdown() {
		if (!_shutdownEnabled) return;

		bool criticalCondition = false;
		const char* reason = nullptr;

		// Check alert pin if configured (hardware alert - latched until cleared)
		if (_alertConfigured && _alertPin >= 0) {
			// INA228 ALERT pin is active LOW
			if (digitalRead(_alertPin) == LOW) {
				criticalCondition = true;
				reason = "INA228 Alert Pin";
			}
		}

		// Software checks as additional protection (always check, independent of alert pin)
		if (!criticalCondition) {
			if (_tempLimit > 0 && _lastTemperature >= _tempLimit) {
				criticalCondition = true;
				reason = "Firmware Check";
			} else if (_powerLimit > 0 && _lastPower >= _powerLimit) {
				criticalCondition = true;
				reason = "Firmware Check";
			}
		}

		if (criticalCondition) {
			// Store shutdown info on first trigger
			if (!_shutdownTriggered) {
				_shutdownTriggered = true;
				_shutdownReason = reason;
				_shutdownTemp = _lastTemperature;
				_shutdownPower = _lastPower;
				getTimeString(_shutdownTime);  // Store formatted time string
				DEBUG_PRINTLN(F("INA228: EMERGENCY SHUTDOWN TRIGGERED!"));
				DEBUG_PRINT(F("INA228: Reason: "));
				DEBUG_PRINTLN(reason);
				initiateBuzzerAlarm();  // Initiate the buzzer (non-blocking)
			}
			
			// Always enforce shutdown when critical condition exists
			if (bri > 0) {
				DEBUG_PRINTLN(F("INA228: Enforcing emergency shutdown (LEDs were turned back on)"));
				briLast = bri;
				bri = 0;
				stateUpdated(CALL_MODE_DIRECT_CHANGE);
			}
			
			// Turn fan to full speed if available
			if (_fanEnabled && _fanPwmPin >= 0) {
				analogWrite(_fanPwmPin, 255);
				_lastFanDutyCycle = 255;
				_lastFanSpeedPercent = 100;
			}
		}
		// Note: Shutdown is NOT automatically cleared when conditions improve.
		// The ALERT pin is latched and must be cleared via the Reset Alert button.
		// This ensures the user acknowledges the event before resuming operation.
	}


public:
	/**
	 * Enable/Disable the usermod
	 */
	inline void enable(bool enable) { _settingEnabled = enable; }

	/**
	 * Get usermod enabled/disabled state
	 */
	inline bool isEnabled() { return _settingEnabled; }

	/**
	 * setup() is called once at boot. WiFi is not yet connected at this point.
	 * readFromConfig() is called prior to setup()
	 * Uses global I2C GPIO settings configured in WLED Settings → Usermods → Usermods setup
	 */
	void setup() override {
		// Only initialize if enabled
		if (!_settingEnabled) {
			DEBUG_PRINTLN(F("INA228: Usermod is disabled, skipping initialization"));
			return;
		}
		
		// INA228 will use the global I2C bus initialized by WLED
		// Check if I2C pins are configured (i2c_sda and i2c_scl globals)
		if (i2c_sda < 0 || i2c_scl < 0) {
			DEBUG_PRINTLN(F("INA228: Global I2C pins not configured. Set them in Usermods setup."));
			return;
		}
		
		initializeINA228();
		
		// Initialize fan control if enabled
		if (_fanEnabled && _fanPwmPin >= 0) {
			if (PinManager::allocatePin(_fanPwmPin, true, PinOwner::UM_INA228)) {
				pinMode(_fanPwmPin, OUTPUT);
				analogWrite(_fanPwmPin, 0); // Start with fan off
				DEBUG_PRINTLN(F("INA228: Fan PWM pin initialized"));
			} else {
				DEBUG_PRINTLN(F("INA228: Failed to allocate fan PWM pin"));
				_fanPwmPin = -1;
				_fanEnabled = false;
			}
		}
		
		// Initialize tachometer pin if specified
		if (_fanEnabled && _fanTachoPin >= 0) {
			if (PinManager::allocatePin(_fanTachoPin, false, PinOwner::UM_INA228)) {
				pinMode(_fanTachoPin, INPUT_PULLUP);
				g_tachPulseCount = 0;  // Reset global counter
				attachInterrupt(_fanTachoPin, tachISR, FALLING);
				DEBUG_PRINT(F("INA228: Fan tachometer pin initialized on GPIO "));
				DEBUG_PRINTLN(_fanTachoPin);
			} else {
				DEBUG_PRINT(F("INA228: Failed to allocate fan tachometer pin GPIO "));
				DEBUG_PRINTLN(_fanTachoPin);
			}
		}
		
		// Initialize alert pin if configured
		if (_shutdownEnabled && _alertPin >= 0) {
			if (PinManager::allocatePin(_alertPin, false, PinOwner::UM_INA228)) {
				pinMode(_alertPin, INPUT_PULLUP); // ALERT is active LOW, needs pullup
				DEBUG_PRINTLN(F("INA228: Alert pin initialized"));
			} else {
				DEBUG_PRINTLN(F("INA228: Failed to allocate alert pin"));
				_alertPin = -1;
			}
		}
		
		// Initialize buzzer pin if configured
		if (_buzzerPin >= 0) {
			if (PinManager::allocatePin(_buzzerPin, true, PinOwner::UM_INA228)) {
				pinMode(_buzzerPin, OUTPUT);
				digitalWrite(_buzzerPin, LOW);  // Ensure buzzer is off
				DEBUG_PRINTLN(F("INA228: Buzzer pin initialized"));
			} else {
				DEBUG_PRINTLN(F("INA228: Failed to allocate buzzer pin"));
				_buzzerPin = -1;
			}
		}
		
		_initDone = true;
	}

	/**
	 * loop() is called continuously. Here you read sensors periodically.
	 */
	void loop() override {
		// Update buzzer state (non-blocking)
		updateBuzzerState();
		
		if (!_settingEnabled || !_initDone || strip.isUpdating()) return;

		unsigned long currentTime = millis();
		if (currentTime - _lastReadTime >= _checkInterval) {
			readSensorValues();
			updateFanSpeed(); // Update fan after reading temperature
			calculateRPM(); // Calculate RPM from pulse count
			checkEmergencyShutdown(); // Check for emergency conditions
			
			// Debug: Log tach info
			if (_fanEnabled && _fanTachoPin >= 0) {
				DEBUG_PRINT(F("INA228 Tach - RPM: "));
				DEBUG_PRINT(_fanRPM);
				DEBUG_PRINT(F(", Duty: "));
				DEBUG_PRINT(_lastFanDutyCycle);
				DEBUG_PRINT(F(" ("));
				DEBUG_PRINT(_lastFanSpeedPercent);
				DEBUG_PRINTLN(F("%)"));
			}
		}
	}


	/**
	 * addToJsonInfo() adds custom entries to the /json/info part of the JSON API.
	 * This displays sensor data in the WLED web UI info section.
	 */
	void addToJsonInfo(JsonObject& root) override {
		if (!_initDone || !_settingEnabled) return;

		JsonObject user = root["u"];
		if (user.isNull()) user = root.createNestedObject("u");

		// Check if we have any readings yet
		if (_lastReadTime == 0) {
			JsonArray arr = user.createNestedArray(F("INA228"));
			arr.add(F("Not read yet"));
			return;
		}

		// Display actual current next to estimated current in the info section
		JsonArray currentArr = user.createNestedArray(F("INA228 Current"));
		currentArr.add(_lastCurrent);
		currentArr.add(F(" A"));

		// Bus voltage
		JsonArray voltageArr = user.createNestedArray(F("INA228 Bus Voltage"));
		voltageArr.add(_lastBusVoltage);
		voltageArr.add(F(" V"));

		// Power
		JsonArray powerArr = user.createNestedArray(F("INA228 Power"));
		powerArr.add(_lastPower);
		powerArr.add(F(" W"));

		// Temperature
		JsonArray tempArr = user.createNestedArray(F("INA228 Temperature"));
		tempArr.add(_lastTemperature);
		tempArr.add(F(" °C"));

		// Fan speed (if enabled)
		if (_fanEnabled && _fanPwmPin >= 0) {
			JsonArray fanArr = user.createNestedArray(F("Fan Speed"));
			fanArr.add(_lastFanSpeedPercent);
			fanArr.add(F("%"));

			// Display RPM if tachometer is configured
			if (_fanTachoPin >= 0) {
				fanArr.add(F(" / "));
				fanArr.add(_fanRPM);
				fanArr.add(F(" RPM"));
			}
		}

		// Emergency shutdown status
		if (_shutdownEnabled) {
			JsonArray shutdownArr = user.createNestedArray(F("Emergency Shutdown"));
			if (_shutdownTriggered) {
				shutdownArr.add(F("🚨 TRIGGERED"));
				
				// Display shutdown time
				if (_shutdownTime[0] != 0) {
					JsonArray timeArr = user.createNestedArray(F(" ↪ Time"));
					timeArr.add(_shutdownTime);
				}
				
				// Display shutdown reason
				if (_shutdownReason) {
					JsonArray reasonArr = user.createNestedArray(F(" ↪ Source"));
					reasonArr.add(_shutdownReason);
				}
				
				// Display temperature at shutdown
				JsonArray tempShutdownArr = user.createNestedArray(F(" ↪ Temperature"));
				tempShutdownArr.add(_shutdownTemp);
				tempShutdownArr.add(F(" °C"));
				
				// Display power at shutdown
				JsonArray powerShutdownArr = user.createNestedArray(F(" ↪ Power"));
				powerShutdownArr.add(_shutdownPower);
				powerShutdownArr.add(F(" W"));
				
				// Add reset button when shutdown is triggered
				JsonArray resetArr = user.createNestedArray(F("Clear Shutdown"));
				resetArr.add(F("<button class=\"btn btn-lg\" onclick=\"requestJson({INA228:{resetAlert:true}})\">Clear</button>"));
			} else {
				shutdownArr.add(F("✓ Armed"));
			}
		}
	}


	/**
	 * addToJsonState() adds sensor data to /json/state
	 */
void addToJsonState(JsonObject& root) override {
	if (!_initDone || !_settingEnabled) return;

	JsonObject usermod = root[FPSTR(_name)];
	if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));
		usermod[F("voltage")] = _lastBusVoltage;
		usermod[F("current")] = _lastCurrent;
		usermod[F("power")] = _lastPower;
		usermod[F("temperature")] = _lastTemperature;
		if (_fanEnabled && _fanPwmPin >= 0) {
			usermod[F("fanSpeed")] = _lastFanSpeedPercent;
			usermod[F("fanDutyCycle")] = _lastFanDutyCycle;
		}
		if (_shutdownEnabled) {
			usermod[F("shutdownTriggered")] = _shutdownTriggered;
		}
	}

	/**
	 * readFromJsonState() - handle reset button clicks
	 */
	void readFromJsonState(JsonObject& root) override {
		if (!_initDone || !_settingEnabled) return;
		
		JsonObject usermod = root[FPSTR(_name)];
		if (usermod.isNull()) return;
		
		// Check for reset alert command
		if (usermod[F("resetAlert")].as<bool>()) {
			resetAlert();
		}
	}


	/**
	 * addToConfig() saves custom persistent settings to cfg.json
	 */
	void addToConfig(JsonObject& root) override {
		JsonObject top = root.createNestedObject(FPSTR(_name));
		
		// Core INA228 settings
		top[F("Enabled")] = _settingEnabled;
		top[F("I2C Address")] = _i2cAddress;
		top[F("Check Interval")] = _checkInterval;
		top[F("Shunt Resistor")] = serialized(String(_shuntResistor, 4));
		top[F("Max Current")] = serialized(String(_maxCurrent, 2));
		top[F("Average Samples")] = _averageSamples;
		
		// Emergency shutdown settings (nested)
		JsonObject shutdown = top.createNestedObject(F("Emergency Shutdown"));
		shutdown[F("Enabled")] = _shutdownEnabled;
		shutdown[F("Alert Pin")] = _alertPin;
		shutdown[F("Temp Limit")] = serialized(String(_tempLimit, 1));
		shutdown[F("Power Limit")] = serialized(String(_powerLimit, 1));
		
		// Fan control settings (nested)
		JsonObject fan = top.createNestedObject(F("PWM Fan Control"));
		fan[F("Enabled")] = _fanEnabled;
		fan[F("PWM Pin")] = _fanPwmPin;
		fan[F("Tach Pin")] = _fanTachoPin;
		fan[F("Start Temp")] = serialized(String(_fanStartTemp, 1));
		fan[F("Max Temp")] = serialized(String(_fanMaxTemp, 1));
		
		// Buzzer settings (nested)
		JsonObject buzzer = top.createNestedObject(F("Piezo Buzzer Alarm"));
		buzzer[F("Enabled")] = _buzzerEnabled;
		buzzer[F("Buzzer Pin")] = _buzzerPin;
		buzzer[F("Frequency")] = _buzzerFrequency;
		buzzer[F("Duration")] = _buzzerDuration;
		buzzer[F("Beep Count")] = _buzzerBeepCount;

		DEBUG_PRINTLN(F("INA228: Config saved"));
	}


	/**
	 * readFromConfig() reads custom settings from cfg.json
	 */
	bool readFromConfig(JsonObject& root) override {
		JsonObject top = root[FPSTR(_name)];

		bool configComplete = !top.isNull();
		if (!configComplete) return false;
		// Required core settings
		configComplete &= getJsonValue(top[F("I2C Address")], _i2cAddress, (uint8_t)0x40);
		configComplete &= getJsonValue(top[F("Check Interval")], _checkInterval, (uint16_t)1000);
		configComplete &= getJsonValue(top[F("Shunt Resistor")], _shuntResistor, 0.015f);
		configComplete &= getJsonValue(top[F("Max Current")], _maxCurrent, 1.5f);
		configComplete &= getJsonValue(top[F("Average Samples")], _averageSamples, (uint8_t)4);
		
		// Optional settings with defaults (including Enabled, since older configs may not have it)
		getJsonValue(top[F("Enabled")], _settingEnabled, true);
		
		// Emergency shutdown settings (nested)
		JsonObject shutdown = top[F("Emergency Shutdown")];
		if (!shutdown.isNull()) {
			getJsonValue(shutdown[F("Enabled")], _shutdownEnabled, false);
			getJsonValue(shutdown[F("Alert Pin")], _alertPin, (int8_t)-1);
			getJsonValue(shutdown[F("Temp Limit")], _tempLimit, 85.0f);
			getJsonValue(shutdown[F("Power Limit")], _powerLimit, 25.0f);
		}
		
		// Fan control settings (nested)
		JsonObject fan = top[F("PWM Fan Control")];
		if (!fan.isNull()) {
			getJsonValue(fan[F("Enabled")], _fanEnabled, false);
			getJsonValue(fan[F("PWM Pin")], _fanPwmPin, (int8_t)-1);
			getJsonValue(fan[F("Tach Pin")], _fanTachoPin, (int8_t)-1);
			getJsonValue(fan[F("Start Temp")], _fanStartTemp, 30.0f);
			getJsonValue(fan[F("Max Temp")], _fanMaxTemp, 50.0f);
		}
		
		// Buzzer settings (nested)
		JsonObject buzzer = top[F("Piezo Buzzer Alarm")];
		if (!buzzer.isNull()) {
			getJsonValue(buzzer[F("Enabled")], _buzzerEnabled, false);
			getJsonValue(buzzer[F("Buzzer Pin")], _buzzerPin, (int8_t)-1);
			getJsonValue(buzzer[F("Frequency")], _buzzerFrequency, (uint16_t)2000);
			getJsonValue(buzzer[F("Duration")], _buzzerDuration, (uint16_t)200);
			getJsonValue(buzzer[F("Beep Count")], _buzzerBeepCount, (uint8_t)3);
		}

		// Reinitialize only if critical settings have changed and already initialized
		if (_initDone && hasSettingsChanged()) {
			DEBUG_PRINTLN(F("INA228: Settings changed, reinitializing..."));
			initializeINA228();
		}

		DEBUG_PRINTLN(F("INA228: Config loaded"));
		return configComplete;
	}


	/**
	 * appendConfigData() adds metadata for usermod settings page
	 */
	void appendConfigData() override {
		// Core INA228 settings
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":I2C Address")); 
		oappend(F("',1,'Default 0x40 (64)');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Check Interval")); 
		oappend(F("',1,'ms');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Shunt Resistor")); 
		oappend(F("',1,'Ω');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Max Current")); 
		oappend(F("',1,'A');"));
		
		oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','Average Samples');"));
		oappend(F("addOption(dd,'1 sample',0);"));
		oappend(F("addOption(dd,'4 samples',1);"));
		oappend(F("addOption(dd,'16 samples',2);"));
		oappend(F("addOption(dd,'64 samples',3);"));
		oappend(F("addOption(dd,'128 samples',4);"));
		oappend(F("addOption(dd,'256 samples',5);"));
		oappend(F("addOption(dd,'512 samples',6);"));
		oappend(F("addOption(dd,'1024 samples',7);"));
		
		// Emergency shutdown settings
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Emergency Shutdown:Enabled"));
		oappend(F("',1,'');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Emergency Shutdown:Alert Pin"));
		oappend(F("',1,'');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Emergency Shutdown:Temp Limit"));
		oappend(F("',1,'°C');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Emergency Shutdown:Power Limit"));
		oappend(F("',1,'W');"));
		
		// Fan control settings
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":PWM Fan Control:Enabled"));
		oappend(F("',1,'');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":PWM Fan Control:PWM Pin"));
		oappend(F("',1,'');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":PWM Fan Control:Tach Pin"));
		oappend(F("',1,'');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":PWM Fan Control:Start Temp"));
		oappend(F("',1,'°C');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":PWM Fan Control:Max Temp"));
		oappend(F("',1,'°C');"));
		
		// Buzzer settings
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Piezo Buzzer Alarm:Enabled"));
		oappend(F("',1,'');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Piezo Buzzer Alarm:Buzzer Pin"));
		oappend(F("',1,'');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Piezo Buzzer Alarm:Frequency"));
		oappend(F("',1,'Hz');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Piezo Buzzer Alarm:Duration"));
		oappend(F("',1,'ms');"));
		
		oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":Piezo Buzzer Alarm:Beep Count"));
		oappend(F("',1,'');"));
	}

	/**
	 * getId() returns unique ID for this usermod
	 */
	uint16_t getId() override {
		return USERMOD_ID_INA228;
	}

	/**
	 * Destructor - cleanup
	 */
	~UsermodINA228() {
		// Detach interrupt if tachometer was configured
		if (_fanTachoPin >= 0) {
			detachInterrupt(_fanTachoPin);
		}
		if (_ina228 != nullptr) {
			delete _ina228;
			_ina228 = nullptr;
		}
	}
};

// Static member definitions
const char UsermodINA228::_name[] PROGMEM = "INA228";

// Register the usermod
static UsermodINA228 ina228_usermod;
REGISTER_USERMOD(ina228_usermod);
