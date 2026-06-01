#include <Arduino.h>
#include "wled.h"

#define LORA_SERIAL Serial2
#define LORA_RX_PIN 16
#define LORA_TX_PIN 17
#define AUX_PIN     4

const char _name[] PROGMEM = "DmxLoRaComms";

class DmxLoRaComms : public Usermod {
    private:
        // bool startupDone = false;
        int dmxChannel = 0;
        int currentPreset = -1;

    void setLoRaConfig() {
        Serial.println("Starting LoRa configuration...");

        delay(1500);
        LORA_SERIAL.println("AT+DEFAULT");
        delay(1500);
        LORA_SERIAL.println("+++");
        delay(1000); // Longer delay to ensure command mode

        // Check for response after +++
        if (LORA_SERIAL.available()) {
            String response = LORA_SERIAL.readString();
            Serial.println("Command mode response: " + response);
        }

        LORA_SERIAL.println("AT+SF7");
        delay(500);
        if (LORA_SERIAL.available()) {
            Serial.println("SF response: " + LORA_SERIAL.readString());
        }


        LORA_SERIAL.println("AT+HELP");
        delay(500);
        if (LORA_SERIAL.available()) {
            Serial.println("HELP response: " + LORA_SERIAL.readString());
        }

        LORA_SERIAL.println("+++");
        delay(1000); // Longer delay after exit
        if (LORA_SERIAL.available()) {
            Serial.println("EXIT response: " + LORA_SERIAL.readString());
        }

        Serial.println("LoRa config completed - check responses above.");
    }

    void setup() {
        Serial.begin(115200);
        LORA_SERIAL.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
        LORA_SERIAL.setTimeout(100);
        setLoRaConfig();
        Serial.println("=== LR02 RECEIVER READY ===");
        // startupDone = true;
    }

    void loop() override {
        if (LORA_SERIAL.available()) {
            String incoming = LORA_SERIAL.readStringUntil('\n');
            incoming.trim();

            DynamicJsonDocument doc(524);
            DeserializationError err = deserializeJson(doc, incoming);
            if (err || !doc.is<JsonArray>()) {
                Serial.println("Invalid JSON: " + incoming);
                return;
            }

            Serial.println("Received LoRa message: " + incoming);
            for (JsonObject item : doc.as<JsonArray>()) {
                for (JsonPair kv : item) {
                    if (String(kv.key().c_str()).toInt() == dmxChannel) {
                        uint8_t presetId = kv.value().as<uint8_t>();
                        if (presetId > 0 && presetId != currentPreset) {
                            Serial.println("Applying preset " + String(presetId) + " for DMX channel " + String(dmxChannel));
                            applyPreset(presetId, CALL_MODE_BUTTON_PRESET);
                            currentPreset = presetId;
                        }
                        break;
                    }
                }
            }

        }
    }

    void addToConfig(JsonObject& root) override {
        JsonObject top = root.createNestedObject("DMX LoRaComms");
        top["dmxChannel"] = dmxChannel;
    }

    bool readFromConfig(JsonObject& root) override {
        JsonObject top = root["DMX LoRaComms"];
        bool ok = !top.isNull();
        ok &= getJsonValue(top["dmxChannel"], dmxChannel, 0 /*default*/);
        return ok;  // return false to have WLED write defaults to disk
    }
};

static DmxLoRaComms dmx_lora_comms;
REGISTER_USERMOD(dmx_lora_comms);