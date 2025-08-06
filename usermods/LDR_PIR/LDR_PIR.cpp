#include "wled.h"

class LDR_PIR : public Usermod {

	private:

		// Private class members. You can declare variables and functions only accessible to your usermod here
		// default values for variables - these will be loaded from config
		bool enabled = false;
		bool initDone = false;
		bool HomeAssistantDiscovery = false;    // Publish Home Assistant Device Information
		unsigned long lastTime = 0;

		// config variables
		int8_t photoPin = 1;
		int8_t pirPin = 0;
		int16_t offDelay = 60;
		float thresholdOn = 0.4;
		float thresholdOff = 0.5;
		byte onPreset = 1;
		byte offPreset = 10;

		// other variables
		bool pirActivated = false;
		uint16_t ldrVal = 0;
		uint16_t lastLight = 0;
		float brightness = 0;
		float lastBrightness = 0;
		byte pirState = LOW;
		byte lastPir = LOW;
		bool onPresetActive = false;
		unsigned long lastMotion = 0;
		unsigned long lastPublish = 0;
		unsigned long curTime = 0;

		// MQTT topic strings for publishing Home Assistant discovery topics
		bool mqttInitialized = false;

		// string that are used multiple time (this will save some flash memory)
		static const char _name[];
		static const char _enabled[];
		static const char _photoPin[];
		static const char _pirPin[];
		static const char _thresholdOn[];
		static const char _thresholdOff[];
		static const char _onPreset[];
		static const char _offPreset[];
		static const char _offDelay[];
		static const char _HomeAssistantDiscovery[];

		// Procedure to define all MQTT discovery Topics 
		void _mqttInitialize()
		{
			char mqttMotionTopic[128];
			char mqttLightTopic[128];
			snprintf_P(mqttMotionTopic, 127, PSTR("%s/motion"), mqttDeviceTopic);
			snprintf_P(mqttLightTopic, 127, PSTR("%s/light"), mqttDeviceTopic);


			if (HomeAssistantDiscovery) {
				_createMqttSensor(F("Motion"), mqttMotionTopic, "motion", "");
				_createMqttSensor(F("Light"), mqttLightTopic, "illuminance", "");
			}
		}


		// Create an MQTT Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Loop.
		void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement)
		{
			String t = String(F("homeassistant/sensor/")) + mqttClientID + F("/") + name + F("/config");
    
			StaticJsonDocument<600> doc;
    
			doc[F("name")] = String(serverDescription) + " " + name;
			doc[F("state_topic")] = topic;
			doc[F("unique_id")] = String(mqttClientID) + name;
			if (unitOfMeasurement != "")
				doc[F("unit_of_measurement")] = unitOfMeasurement;
			if (deviceClass != "")
				doc[F("device_class")] = deviceClass;
			doc[F("expire_after")] = 1800;

			JsonObject device = doc.createNestedObject(F("device")); // attach the sensor to the same device
			device[F("name")] = serverDescription;
			device[F("identifiers")] = "wled-sensor-" + String(mqttClientID);
			device[F("manufacturer")] = F("WLED");
			device[F("model")] = F("FOSS");
			device[F("sw_version")] = versionString;

			String temp;
			serializeJson(doc, temp);
			DEBUG_PRINTLN(t);
			DEBUG_PRINTLN(temp);

			mqtt->publish(t.c_str(), 0, true, temp.c_str());
		}

		void publishMqtt(const char *topic, const char* state) {
		//Check if MQTT Connected, otherwise it will crash the 8266
			if (WLED_MQTT_CONNECTED) {
				char subuf[128];
				snprintf_P(subuf, 127, PSTR("%s/%s"), mqttDeviceTopic, topic);
				mqtt->publish(subuf, 0, false, state);
			}
		}

	public:

		inline void enable(bool enable) { enabled = enable; }

		inline bool isEnabled() { return enabled; }

		void setup() {
			if (enabled) {
					pinMode(photoPin, INPUT);
					pinMode(pirPin, INPUT_PULLUP);
			}
			initDone = true;
		}


		void connected() {
			//Serial.println("Connected to WiFi!");
		}


		void loop() {
			// if usermod is disabled or called during strip updating just exit
			// NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
			if (!enabled || strip.isUpdating()) return;

			// do your magic here
			curTime = millis();
			if (curTime - lastTime > 250) {
				ldrVal = analogRead(photoPin);
				pirState = digitalRead(pirPin);

				if ( (pirState != lastPir) || (abs(ldrVal-lastLight)>32) ) {
					lastLight = ldrVal;
#ifdef ESP32
    brightness = roundf( (float)ldrVal*100/(float)4095 )/100;
#else
    brightness = roundf( (float)ldrVal*100/(float)1023 )/100;
#endif
					if (pirState != lastPir) {
						if (pirState) {
							publishMqtt("motion","ON");
						} else {
							publishMqtt("motion","OFF");
						}
					}
					if (pirState == HIGH) {
						if (offMode && (brightness < thresholdOn) ) {
							applyPreset(onPreset);
							pirActivated = true;
							onPresetActive = true;
						} else if (pirActivated && !onPresetActive && (brightness < thresholdOff) ) {
							applyPreset(onPreset);
							onPresetActive = true;
						}
					} else if (pirActivated && onPresetActive) {
						if (brightness > thresholdOff) {
							applyPreset(offPreset);
							onPresetActive = false;
						} else if (lastPir == HIGH) {
							lastMotion = curTime;
						}
					}
					lastPir=pirState;
					lastLight=ldrVal;
				} else if (pirActivated) {
					if (offMode) {
						pirActivated = false;
					} else if ( onPresetActive && (pirState == LOW) && (curTime - lastMotion > offDelay*1000) ) {
						applyPreset(offPreset);
						onPresetActive = false;
					}
				}

				lastTime = curTime;
			}

			if ( (curTime - lastPublish > 10000 && brightness != lastBrightness) || (curTime - lastPublish > 600000) ) {
				char temp[5];
				publishMqtt("light", dtostrf(brightness, 0, 2, temp));
				lastBrightness = brightness;
				lastPublish = curTime;
			}

			if (curTime - lastMotion > 600000) {
				publishMqtt("motion","OFF");
				lastMotion = curTime;
			}
		}

		void onMqttConnect(bool sessionPresent) {
			if (WLED_MQTT_CONNECTED && !mqttInitialized) {
				_mqttInitialize();
				mqttInitialized = true;
			}
		}


		void onStateChange(uint8_t mode) {
			// do something if WLED state changed (color, brightness, effect, preset, etc)
		}

		void addToJsonInfo(JsonObject& root) {
			JsonObject user = root["u"];
			if (user.isNull()) user = root.createNestedObject("u");
			JsonArray lightArr = user.createNestedArray("Light"); //name
			lightArr.add(brightness); //value
			//lightArr.add("%"); //unit
			JsonArray motionArr = user.createNestedArray("Motion"); //name
			motionArr.add(pirState);
		}

		void addToConfig(JsonObject& root)
		{
			JsonObject top = root.createNestedObject(FPSTR(_name));
			top[FPSTR(_enabled)] = enabled;
			//save these vars persistently whenever settings are saved
			top[FPSTR(_photoPin)] = photoPin;
			top[FPSTR(_pirPin)] = pirPin;
			top[FPSTR(_thresholdOn)] = thresholdOn;
			top[FPSTR(_thresholdOff)] = thresholdOff;
			top[FPSTR(_onPreset)] = onPreset;
			top[FPSTR(_offPreset)] = offPreset;
			top[FPSTR(_offDelay)] = offDelay;
			top[FPSTR(_HomeAssistantDiscovery)] = HomeAssistantDiscovery;
		}


		bool readFromConfig(JsonObject& root)
		{
			// default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
			// setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

			JsonObject top = root[FPSTR(_name)];

			bool configComplete = !top.isNull();

			configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
			configComplete &= getJsonValue(top[FPSTR(_photoPin)], photoPin, -1);
			configComplete &= getJsonValue(top[FPSTR(_pirPin)], pirPin, -1);
			configComplete &= getJsonValue(top[FPSTR(_thresholdOn)], thresholdOn);
			configComplete &= getJsonValue(top[FPSTR(_thresholdOff)], thresholdOff);
			configComplete &= getJsonValue(top[FPSTR(_onPreset)], onPreset);
			configComplete &= getJsonValue(top[FPSTR(_offPreset)], offPreset);
			configComplete &= getJsonValue(top[FPSTR(_offDelay)], offDelay);
			configComplete &= getJsonValue(top[FPSTR(_HomeAssistantDiscovery)], HomeAssistantDiscovery, false);

			return configComplete;
		}

};


// add more strings here to reduce flash memory usage
const char LDR_PIR::_name[]		PROGMEM = "LDR PIR";
const char LDR_PIR::_enabled[] PROGMEM = "enabled";
const char LDR_PIR::_photoPin[] PROGMEM = "ldr_pin";
const char LDR_PIR::_pirPin[]	PROGMEM = "pir_pin";
const char LDR_PIR::_thresholdOn[] PROGMEM = "LDR On Threshold";
const char LDR_PIR::_thresholdOff[] PROGMEM = "LDR Off Threshold";
const char LDR_PIR::_onPreset[] PROGMEM = "PIR On Preset";
const char LDR_PIR::_offPreset[] PROGMEM = "PIR Off Preset";
const char LDR_PIR::_offDelay[] PROGMEM = "PIR Off Delay";
const char LDR_PIR::_HomeAssistantDiscovery[] PROGMEM = "Home Assistant Discovery";

static LDR_PIR ldr_pir;
REGISTER_USERMOD(ldr_pir);
