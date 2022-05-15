#pragma once

#warning "This usermod is deprecated and no longer maintained. It will be removed in a future WLED release. Please use usermod multi_relay which has more features."

#include "wled.h"
#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

#ifndef MQTTSWITCHPINS
#error "Please define MQTTSWITCHPINS in platformio_override.ini. e.g. -D MQTTSWITCHPINS="12, 0, 2" "
// The following define helps Eclipse's C++ parser but is never used in production due to the #error statement on the line before
#define MQTTSWITCHPINS 12, 0, 2
#endif

// Default behavior: All outputs active high
#ifndef MQTTSWITCHINVERT
#define MQTTSWITCHINVERT
#endif

// Default behavior: All outputs off
#ifndef MQTTSWITCHDEFAULTS
#define MQTTSWITCHDEFAULTS
#endif

static const uint8_t switchPins[] = { MQTTSWITCHPINS };
//This is a hack to get the number of pins defined by the user
#define NUM_SWITCH_PINS (sizeof(switchPins))
static const bool switchInvert[NUM_SWITCH_PINS] = { MQTTSWITCHINVERT};
//Make settings in config file more readable
#define ON 1
#define OFF 0
static const bool switchDefaults[NUM_SWITCH_PINS] = { MQTTSWITCHDEFAULTS};
#undef ON
#undef OFF

class UsermodMqttSwitch: public Usermod
{
private:
    bool mqttInitialized;
    bool switchState[NUM_SWITCH_PINS];

public:
    UsermodMqttSwitch() :
            mqttInitialized(false)
    {
    }

    void setup()
    {
        for (int pinNr = 0; pinNr < NUM_SWITCH_PINS; pinNr++) {
            setState(pinNr, switchDefaults[pinNr]);
            pinMode(switchPins[pinNr], OUTPUT);
        }
    }

    void loop()
    {
        if (!mqttInitialized) {
            mqttInit();
            return; // Try again in next loop iteration
        }
    }

    void mqttInit()
    {
        if (!mqtt)
            return;
        mqtt->onMessage(
                std::bind(&UsermodMqttSwitch::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
                          std::placeholders::_5, std::placeholders::_6));
        mqtt->onConnect(std::bind(&UsermodMqttSwitch::onMqttConnect, this, std::placeholders::_1));
        mqttInitialized = true;
    }

    void onMqttConnect(bool sessionPresent);

    void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void updateState(uint8_t pinNr);

    void setState(uint8_t pinNr, bool active)
    {
        if (pinNr > NUM_SWITCH_PINS)
            return;
        switchState[pinNr] = active;
        digitalWrite((char) switchPins[pinNr], (char) (switchInvert[pinNr] ? !active : active));
        updateState(pinNr);
    }
};

inline void UsermodMqttSwitch::onMqttConnect(bool sessionPresent)
{
    if (mqttDeviceTopic[0] == 0)
        return;

    for (int pinNr = 0; pinNr < NUM_SWITCH_PINS; pinNr++) {
        char buf[128];
        StaticJsonDocument<1024> json;
        sprintf(buf, "%s Switch %d", serverDescription, pinNr + 1);
        json[F("name")] = buf;

        sprintf(buf, "%s/switch/%d", mqttDeviceTopic, pinNr);
        json["~"] = buf;
        strcat(buf, "/set");
        mqtt->subscribe(buf, 0);

        json[F("stat_t")] = "~/state";
        json[F("cmd_t")] = "~/set";
        json[F("pl_off")] = F("OFF");
        json[F("pl_on")] = F("ON");

        char uid[16];
        sprintf(uid, "%s_sw%d", escapedMac.c_str(), pinNr);
        json[F("unique_id")] = uid;

        strcpy(buf, mqttDeviceTopic);
        strcat(buf, "/status");
        json[F("avty_t")] = buf;
        json[F("pl_avail")] = F("online");
        json[F("pl_not_avail")] = F("offline");
        //TODO: dev
        sprintf(buf, "homeassistant/switch/%s/config", uid);
        char json_str[1024];
        size_t payload_size = serializeJson(json, json_str);
        mqtt->publish(buf, 0, true, json_str, payload_size);
        updateState(pinNr);
    }
}

inline void UsermodMqttSwitch::onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    //Note: Payload is not necessarily null terminated. Check "len" instead.
    for (int pinNr = 0; pinNr < NUM_SWITCH_PINS; pinNr++) {
        char buf[64];
        sprintf(buf, "%s/switch/%d/set", mqttDeviceTopic, pinNr);
        if (strcmp(topic, buf) == 0) {
            //Any string starting with "ON" is interpreted as ON, everything else as OFF
            setState(pinNr, len >= 2 && payload[0] == 'O' && payload[1] == 'N');
            break;
        }
    }
}

inline void UsermodMqttSwitch::updateState(uint8_t pinNr)
{
    if (!mqttInitialized)
        return;

    if (pinNr > NUM_SWITCH_PINS)
        return;

    char buf[64];
    sprintf(buf, "%s/switch/%d/state", mqttDeviceTopic, pinNr);
    if (switchState[pinNr]) {
        mqtt->publish(buf, 0, false, "ON");
    } else {
        mqtt->publish(buf, 0, false, "OFF");
    }
}
