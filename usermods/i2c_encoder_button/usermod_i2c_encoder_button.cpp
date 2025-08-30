#include "wled.h"
#include <Wire.h>
#include <i2cEncoderLibV2.h>

// Default values for I2C encoder pins and address
#ifndef I2C_ENCODER_DEFAULT_ENABLED
#define I2C_ENCODER_DEFAULT_ENABLED false
#endif
#ifndef I2C_ENCODER_DEFAULT_INT_PIN
#define I2C_ENCODER_DEFAULT_INT_PIN 1
#endif
#ifndef I2C_ENCODER_DEFAULT_SDA_PIN
#define I2C_ENCODER_DEFAULT_SDA_PIN 0
#endif
#ifndef I2C_ENCODER_DEFAULT_SCL_PIN
#define I2C_ENCODER_DEFAULT_SCL_PIN 2
#endif
#ifndef I2C_ENCODER_DEFAULT_ADDRESS
#define I2C_ENCODER_DEFAULT_ADDRESS 0x00
#endif

// v2 usermod for I2C Encoder
class UsermodI2CEncoderButton : public Usermod {
private:
    i2cEncoderLibV2 * encoder_p;
    bool encoderButtonDown = false;
    uint32_t buttonPressStartTime = 0; // millis when button was pressed
    uint32_t buttonPressDuration = 0;
    const uint32_t buttonLongPressThreshold = 1000; // duration threshold for long press (millis)
    bool wasLongButtonPress = false;

    // encoderMode keeps track of what function the encoder is controlling
    // 0 = brightness
    // 1 = effect
    uint8_t encoderMode = 0;
    // encoderModes keeps track of what color the encoder LED should be for each mode
    const uint32_t encoderModes[2] = {0x0000FF, 0xFF0000};
    uint32_t lastInteractionTime = 0;
    const uint32_t modeResetTimeout = 30000; // timeout for reseting mode to 0
    const uint32_t brightnessDelta = 16;
    bool enabled = false;
    bool initDone = false;
    // Configurable pins and address (now user-configurable via JSON config)
    int8_t intPin = I2C_ENCODER_DEFAULT_INT_PIN;  // Interrupt pin for I2C encoder
    int8_t sdaPin = I2C_ENCODER_DEFAULT_SDA_PIN;  // I2C SDA pin
    int8_t sclPin = I2C_ENCODER_DEFAULT_SCL_PIN;  // I2C SCL pin
    uint8_t i2cAddress = I2C_ENCODER_DEFAULT_ADDRESS;  // I2C address of encoder

    void updateBrightness(int8_t deltaBrightness) {
        bri = constrain(bri + deltaBrightness, 0, 255);
        colorUpdated(CALL_MODE_BUTTON);
    }

    void updateEffect(int8_t deltaEffect)
    {
        // set new effect with rollover at 0 and MODE_COUNT
        effectCurrent = (effectCurrent + MODE_COUNT + deltaEffect) % MODE_COUNT;
        colorUpdated(CALL_MODE_FX_CHANGED);
    }

    void setEncoderMode(uint8_t mode)
    {
        // set new mode and update encoder LED color
        encoderMode = mode;
        encoder_p->writeRGBCode(encoderModes[encoderMode]);
    }
    void handleEncoderShortButtonPress() {
        toggleOnOff();
        colorUpdated(CALL_MODE_BUTTON);
        setEncoderMode(0);
    }
    void handlEncoderLongButtonPress() {
        if (encoderMode == 0 && bri == 0) {
            applyPreset(1);
            colorUpdated(CALL_MODE_FX_CHANGED);
        } else {
            setEncoderMode((encoderMode + 1) % (sizeof(encoderModes) / sizeof(encoderModes[0])));
        }
        buttonPressStartTime = millis();
        wasLongButtonPress = true;
    }
    void encoderRotated(i2cEncoderLibV2 *obj) {
        switch (encoderMode) {
            case 0: updateBrightness(obj->readStatus(i2cEncoderLibV2::RINC) ? brightnessDelta : -brightnessDelta); break;
            case 1: updateEffect(obj->readStatus(i2cEncoderLibV2::RINC) ? 1 : -1); break;
        }
        lastInteractionTime = millis();
    }
    void encoderButtonPush(i2cEncoderLibV2 *obj) {
        encoderButtonDown = true;
        buttonPressStartTime = lastInteractionTime = millis();
    }
    void encoderButtonRelease(i2cEncoderLibV2 *obj) {
        encoderButtonDown = false;
        if (!wasLongButtonPress) handleEncoderShortButtonPress();
        wasLongButtonPress = false;
        buttonPressDuration = 0;
        lastInteractionTime = millis();
    }
public:
    UsermodI2CEncoderButton() {
        encoder_p = nullptr;
    }
    void setup() override {
        // (Re)initialize encoder with current config
        if (encoder_p) delete encoder_p;
        encoder_p = new i2cEncoderLibV2(i2cAddress);
        pinMode(intPin, INPUT);
        Wire.begin(sdaPin, sclPin);
        encoder_p->reset();
        encoder_p->begin(
            i2cEncoderLibV2::INT_DATA | i2cEncoderLibV2::WRAP_ENABLE | i2cEncoderLibV2::DIRE_RIGHT |
            i2cEncoderLibV2::IPUP_ENABLE | i2cEncoderLibV2::RMOD_X1 | i2cEncoderLibV2::RGB_ENCODER
        );

        encoder_p->writeCounter(0); /* Reset the counter value */
        encoder_p->writeMax(255);   /* Set the maximum threshold*/
        encoder_p->writeMin(0);     /* Set the minimum threshold */
        encoder_p->writeStep(1);    /* Set the step to 1*/
        encoder_p->writeAntibouncingPeriod(5);
        encoder_p->writeFadeRGB(1);
        encoder_p->writeInterruptConfig(
            i2cEncoderLibV2::RINC | i2cEncoderLibV2::RDEC | i2cEncoderLibV2::PUSHP | i2cEncoderLibV2::PUSHR
        );
        setEncoderMode(0);
        initDone = true;
    }
    void loop() override {
        if (!enabled) return;
        if (digitalRead(intPin) == LOW) {
            if (encoder_p->updateStatus()) {
                if (encoder_p->readStatus(i2cEncoderLibV2::RINC) || encoder_p->readStatus(i2cEncoderLibV2::RDEC)) encoderRotated(encoder_p);
                if (encoder_p->readStatus(i2cEncoderLibV2::PUSHP)) encoderButtonPush(encoder_p);
                if (encoder_p->readStatus(i2cEncoderLibV2::PUSHR)) encoderButtonRelease(encoder_p);
            }
        }
        if (encoderButtonDown) buttonPressDuration = millis() - buttonPressStartTime;
        if (buttonPressDuration > buttonLongPressThreshold) handlEncoderLongButtonPress();
        if (encoderMode != 0 && millis() - lastInteractionTime > modeResetTimeout) setEncoderMode(0);
    }
    void addToJsonInfo(JsonObject& root) override {
        JsonObject user = root["u"];
        if (user.isNull()) user = root.createNestedObject("u");
        JsonArray arr = user.createNestedArray(F("I2C Encoder"));
        arr.add(enabled ? F("Enabled") : F("Disabled"));
    }
    void addToConfig(JsonObject& root) override {
        // Add user-configurable pins and address to config
        JsonObject top = root.createNestedObject(F("I2C_Encoder_Button"));
        top["enabled"] = enabled;
        top["intPin"] = intPin;
        top["sdaPin"] = sdaPin;
        top["sclPin"] = sclPin;
        top["i2cAddress"] = i2cAddress;
    }
    bool readFromConfig(JsonObject& root) override {
        // Read user-configurable pins and address from config
        JsonObject top = root["I2C_Encoder_Button"];
        bool configComplete = !top.isNull();
        configComplete &= getJsonValue(top["enabled"], enabled, I2C_ENCODER_DEFAULT_ENABLED);
        configComplete &= getJsonValue(top["intPin"], intPin, I2C_ENCODER_DEFAULT_INT_PIN);
        configComplete &= getJsonValue(top["sdaPin"], sdaPin, I2C_ENCODER_DEFAULT_SDA_PIN);
        configComplete &= getJsonValue(top["sclPin"], sclPin, I2C_ENCODER_DEFAULT_SCL_PIN);
        configComplete &= getJsonValue(top["i2cAddress"], i2cAddress, I2C_ENCODER_DEFAULT_ADDRESS);
        return configComplete;
    }
    uint16_t getId() override { return USERMOD_I2C_ENCODER_BUTTON; }
};

static UsermodI2CEncoderButton usermod_i2c_encoder_button;
REGISTER_USERMOD(usermod_i2c_encoder_button);