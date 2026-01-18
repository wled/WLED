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
#ifndef I2C_ENCODER_DEFAULT_ADDRESS
#define I2C_ENCODER_DEFAULT_ADDRESS 0x00
#endif

// v2 usermod for I2C Encoder
class UsermodI2CEncoderButton : public Usermod {
private:
    static const char _name[];
    i2cEncoderLibV2 * encoder_p;
    bool encoderButtonDown = false;
    uint32_t buttonPressStartTime = 0;  // Millis when button was pressed
    uint32_t buttonPressDuration = 0;
    const uint32_t buttonLongPressThreshold = 1000;  // Duration threshold for long press (millis)
    bool wasLongButtonPress = false;

    // EncoderMode keeps track of what function the encoder is controlling
    // 0 = brightness
    // 1 = effect
    uint8_t encoderMode = 0;
    // EncoderModes keeps track of what color the encoder LED should be for each mode
    const uint32_t encoderModes[2] = {0x0000FF, 0xFF0000};
    uint32_t lastInteractionTime = 0;
    const uint32_t modeResetTimeout = 30000;  // Timeout for reseting mode to 0
    const int8_t brightnessDelta = 16;
    bool enabled = I2C_ENCODER_DEFAULT_ENABLED;

    // Configurable pins and address (now user-configurable via JSON config)
    int8_t irqPin = I2C_ENCODER_DEFAULT_INT_PIN;  // Interrupt pin for I2C encoder
    uint8_t i2cAddress = I2C_ENCODER_DEFAULT_ADDRESS;  // I2C address of encoder

    void update() {
        stateUpdated(CALL_MODE_BUTTON);
        updateInterfaces(CALL_MODE_BUTTON);
    }

    void updateBrightness(bool increase) {
        int8_t delta = bri < 40 ? brightnessDelta / 2 : brightnessDelta;
        bri = constrain(bri + (increase ? delta : -delta), 0, 255);
        update();
    }

    void updateEffect(bool increase) {
        // Set new effect with rollover at 0 and MODE_COUNT
        effectCurrent = (effectCurrent + MODE_COUNT + (increase ? 1 : -1)) % MODE_COUNT;
        stateChanged = true;
        Segment& seg = strip.getSegment(strip.getMainSegmentId());
        seg.setMode(effectCurrent);
        update();
    }

    void setEncoderMode(uint8_t mode) {
        // Set new mode and update encoder LED color
        encoderMode = mode;
        encoder_p->writeRGBCode(encoderModes[encoderMode]);
    }

    void handleEncoderShortButtonPress() {
        DEBUG_PRINTLN(F("Encoder short button press"));
        toggleOnOff();
        update();
        setEncoderMode(0);
    }

    void handleEncoderLongButtonPress() {
        DEBUG_PRINTLN(F("Encoder long button press"));
        if (encoderMode == 0 && bri == 0) {
            applyPreset(1);
            update();
        } else {
            setEncoderMode((encoderMode + 1) % (sizeof(encoderModes) / sizeof(encoderModes[0])));
        }
        buttonPressStartTime = millis();
        wasLongButtonPress = true;
    }

    void encoderRotated(i2cEncoderLibV2 *obj) {
        DEBUG_PRINTLN(F("Encoder rotated"));
        switch (encoderMode) {
            case 0: updateBrightness(obj->readStatus(i2cEncoderLibV2::RINC)); break;
            case 1: updateEffect(obj->readStatus(i2cEncoderLibV2::RINC)); break;
        }
        lastInteractionTime = millis();
    }

    void encoderButtonPush(i2cEncoderLibV2 *obj) {
        DEBUG_PRINTLN(F("Encoder button pushed"));
        encoderButtonDown = true;
        buttonPressStartTime = lastInteractionTime = millis();
    }

    void encoderButtonRelease(i2cEncoderLibV2 *obj) {
        DEBUG_PRINTLN(F("Encoder button released"));
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
        // Clean up existing encoder if any
        if (encoder_p) {
            delete encoder_p;
            encoder_p = nullptr;
        }

        if (!enabled) {
            if (irqPin >= 0) PinManager::deallocatePin(irqPin, PinOwner::UM_I2C_ENCODER_BUTTON);
            return;
        }

        if (i2c_sda < 0 || i2c_scl < 0) {
            DEBUG_PRINTLN(F("I2C pins not set, disabling I2C encoder usermod."));
            enabled = false;
            return;
        } else {
            if (irqPin >= 0 && PinManager::allocatePin(irqPin, false, PinOwner::UM_I2C_ENCODER_BUTTON)) {
                pinMode(irqPin, INPUT);
            } else {
                DEBUG_PRINTLN(F("Unable to allocate interrupt pin, disabling I2C encoder usermod."));
                irqPin = -1;
                enabled = false;
                return;
            }
        }

        // (Re)initialize encoder with current config
        encoder_p = new i2cEncoderLibV2(i2cAddress);
        encoder_p->reset();
        encoder_p->begin(
            i2cEncoderLibV2::INT_DATA | i2cEncoderLibV2::WRAP_ENABLE | i2cEncoderLibV2::DIRE_RIGHT |
            i2cEncoderLibV2::IPUP_ENABLE | i2cEncoderLibV2::RMOD_X1 | i2cEncoderLibV2::RGB_ENCODER
        );
        encoder_p->writeCounter((int32_t)0);  // Reset the counter value
        encoder_p->writeMax((int32_t)255);  // Set the maximum threshold
        encoder_p->writeMin((int32_t)0);  // Set the minimum threshold
        encoder_p->writeStep((int32_t)1);  // Set the step to 1
        encoder_p->writeAntibouncingPeriod(20);
        encoder_p->writeFadeRGB(1);
        encoder_p->writeInterruptConfig(
            i2cEncoderLibV2::RINC | i2cEncoderLibV2::RDEC | i2cEncoderLibV2::PUSHP | i2cEncoderLibV2::PUSHR
        );
        setEncoderMode(0);
    }

    void loop() override {
        if (!enabled || !encoder_p) return;
        if (digitalRead(irqPin) == LOW) {
            if (encoder_p->updateStatus()) {
                if (encoder_p->readStatus(i2cEncoderLibV2::RINC) || encoder_p->readStatus(i2cEncoderLibV2::RDEC)) encoderRotated(encoder_p);
                if (encoder_p->readStatus(i2cEncoderLibV2::PUSHP)) encoderButtonPush(encoder_p);
                if (encoder_p->readStatus(i2cEncoderLibV2::PUSHR)) encoderButtonRelease(encoder_p);
            }
        }
        if (encoderButtonDown) buttonPressDuration = millis() - buttonPressStartTime;
        if (buttonPressDuration > buttonLongPressThreshold) handleEncoderLongButtonPress();
        if ((encoderMode != 0) && ((millis() - lastInteractionTime) > modeResetTimeout)) setEncoderMode(0);
    }

    void addToJsonInfo(JsonObject& root) override {
        JsonObject user = root["u"];
        if (user.isNull()) user = root.createNestedObject("u");
        JsonArray arr = user.createNestedArray(F("I2C Encoder"));
        arr.add(enabled ? F("Enabled") : F("Disabled"));
    }

    void addToConfig(JsonObject& root) override {
        // Add user-configurable pins and address to config
        JsonObject top = root.createNestedObject(FPSTR(_name));
        top["enabled"] = enabled;
        top["irq_pin"] = irqPin;
        top["i2c_address"] = i2cAddress;
    }

    bool readFromConfig(JsonObject& root) override {
        // Read user-configurable pins and address from config
        JsonObject top = root[FPSTR(_name)];
        bool configComplete = !top.isNull();
        configComplete &= getJsonValue(top["enabled"], enabled, I2C_ENCODER_DEFAULT_ENABLED);
        configComplete &= getJsonValue(top["irq_pin"], irqPin, I2C_ENCODER_DEFAULT_INT_PIN);
        configComplete &= getJsonValue(top["i2c_address"], i2cAddress, I2C_ENCODER_DEFAULT_ADDRESS);
        return configComplete;
    }

    uint16_t getId() override { return USERMOD_ID_I2C_ENCODER_BUTTON; }
};

const char UsermodI2CEncoderButton::_name[] PROGMEM = "i2c_encoder_button";

static UsermodI2CEncoderButton usermod_i2c_encoder_button;
REGISTER_USERMOD(usermod_i2c_encoder_button);
