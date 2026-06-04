#include "wled.h"

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

class ACS712 : public Usermod {
	private:
		bool initPin = false;
		bool initMQTT = false;
		float current = 0;
		float lastCurrent = 0;
		double sumCurrent = 0;
		unsigned long lastTime = 0;
		String currentTopic = "";

		bool enabled = false;
		int8_t pin = -1;
		uint8_t currentRatio = 100.0;
		uint16_t resolution = 4095.0;
		int16_t offset = 0;

		static const char _name[];
		static const char _enabled[];
		static const char _current[];
		static const char _resolution[];
		static const char _offset[];


		void _mqttInitialize() {
			currentTopic = String(mqttDeviceTopic) + "/current";

			String ha = String("homeassistant/sensor/") + mqttClientID + "/" + FPSTR(_current) + "/config";
			StaticJsonDocument<1024> json;

			json[F("name")] = serverDescription+String(" Current");
			json[F("state_topic")] = currentTopic;
			json[F("device_class")] = FPSTR(_current);
			json[F("unique_id")] = String(mqttClientID);
			json[F("unit_of_measurement")] = F("mA");

			String jsonSer;
		    serializeJson(json, jsonSer);
		    mqtt->publish(ha.c_str(), 0, true, jsonSer.c_str());
  		}

	public:
		void setup() override {
        }

		void loop() override {
		    if (!enabled || strip.isUpdating() || pin == -1 || (millis() - lastTime < 5000)) return;
			if (!initPin) {
				pinMode(pin, INPUT);
				initPin = true;
			}

			if (!initMQTT && WLED_MQTT_CONNECTED) {
				_mqttInitialize();
				initMQTT = true;
			}

			sumCurrent = 0;
			for(int i = 0; i < 100; i++){
    			current = analogRead(pin);

				current = (current - (resolution / 2.0f)) * (5000.0f / (resolution * currentRatio));
				current = current * 1000;
				current = current + offset;

			    sumCurrent += current;
			}

			current = sumCurrent/100;
			if (initMQTT) mqtt->publish(currentTopic.c_str(), 0, true, String((current+lastCurrent)/2.0).c_str());

			lastCurrent = current;
			lastTime = millis();
    	}

		void addToConfig(JsonObject &root) override {
			JsonObject top = root.createNestedObject(FPSTR(_name));
			top[FPSTR(_enabled)] = enabled;
			top["pin"] = pin;
			top[FPSTR(_current)] = currentRatio;
			top[FPSTR(_resolution)] = resolution;
			top[FPSTR(_offset)] = offset;
		}

		bool readFromConfig(JsonObject& root) override {
			JsonObject top = root[FPSTR(_name)];
			bool configComplete = !top.isNull();
			configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, enabled);
			configComplete &= getJsonValue(top["pin"], pin, pin);
			configComplete &= getJsonValue(top["current"], currentRatio, currentRatio);
			configComplete &= getJsonValue(top["resolution"], resolution, resolution);
			configComplete &= getJsonValue(top["offset"], offset, offset);

			return configComplete;
    	}

		void appendConfigData() override {
			oappend(F("dd=addDropdown('"));
			oappend(String(FPSTR(_name)).c_str());
			oappend(F("','"));
			oappend(String(FPSTR(_current)).c_str());
			oappend(F("');"));
			oappend(F("addOption(dd,'5A',185.0);"));
			oappend(F("addOption(dd,'20A',100.0);"));
			oappend(F("addOption(dd,'30A',66.0);"));
			oappend(F("addInfo('"));
			oappend(String(FPSTR(_name)).c_str());
			oappend(F(":"));
			oappend(String(FPSTR(_offset)).c_str());
  			oappend(F("',1,'<i>(use to calibrate 0mA)</i>');"));
		}
};

const char ACS712::_name[]    PROGMEM = "ACS712";
const char ACS712::_enabled[] PROGMEM = "enabled";
const char ACS712::_current[] PROGMEM = "current";
const char ACS712::_resolution[] PROGMEM = "resolution";
const char ACS712::_offset[] PROGMEM = "offset";

static ACS712 acs712;
REGISTER_USERMOD(acs712);
