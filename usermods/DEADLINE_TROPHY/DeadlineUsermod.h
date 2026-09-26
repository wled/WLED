#pragma once

#include "WiFiUdp.h"
#include "DeadlineTrophy.h"

class DeadlineUsermod : public Usermod
{
public:

    static const int PIN_LOGOTHERM = 35;
    static const int PIN_INPUTVOLTAGE_V1 = 33;
    static const int PIN_INPUTVOLTAGE_V2 = 36;

    // for the analogRead(), average over some samples to smoothen fluctuations.
    static const int AVERAGE_SAMPLES = 10;

    static constexpr float KELVIN_OFFSET = 273.15;
    /*
        logoTherm goes via voltage divider with R6, Thermistor TH1, R7

        R6: 15kOhm (Upper, Above the Division)
        TH1: ERT-J0EM103J (Thermistor, Below the Division) - R = 10 kOhm @ 25°, B-Value ~ 3900 K
        R7: 1kOhm (Lower, Below the Division, in Series with the Thermistor)
        Input Voltage V_CC = 5V in an ideal world

        Voltage Division:
        V_Therm = V_CC * (TH1 + R7) / (R6 + TH1 + R7) = V_CC * R6 / R_ges
        -> TH1 = 1 / (V_CC / V_Therm - 1) * R6 - R7

        12-bit ADC: (is somewhat non-linear, but who the shit cares.)
        V_Therm = 3.3V * AnalogReadResult / 4095 = AnalogReadResult * voltageAdcCoeff;

        -> TH1 = 1 / (V_CC / voltageAdcCoeff / AnalogReadResult - 1) * R6 - R7

        Steinhart-Hart (first-order, we are not THAT hart).
        1/T = 1/T0 + 1/B * ln(TH1 / R0)
        T0 = 298.15 K (nominal temperature where R(TH1) = R0)
        R0 = 1e4 Ohm (nominal resistance at T = T0)

        -> T = 1. / ( invT0 + invB * ln(TH1 / R0) ) in Kelvin

        it seems like we can use logf(), the float-valued natural logarithm.
    */
    static constexpr float logoR6_Ohm = 1.5e4;
    static constexpr float logoR7_Ohm = 1e3;
    static constexpr float logoThermistor_R0_Ohm = 1e4;
    static constexpr float logoTherm_OneOverT0 = 1. / (25. + KELVIN_OFFSET);
    static constexpr float logoTherm_OneOverB = 1. / 3900.;
    static constexpr float voltageAdcCoeff = 3.3 / 4095.; // 12bit ADC

    /*
        input voltage V_CC goes via voltage divider with R23 = R24, i.e.
        V_pin33 = V_CC * (R23) / (R23 + R24)
        -> V_CC = 2. * V_pin33
                = 2. * (AnalogRead33Result) / 4095 * 3.3V
                = 2. * (AnalogRead33Result) * voltageAdcCoeff

    */

    const float maxCurrent = ABL_MILLIAMPS_DEFAULT;

    bool udpSenderEnabled = true;
    float udpSenderIntervalSec = 0.07; // must not überforder the ESP32 WiFi! this CAN happen. 70ms is about minimum (?)
    bool doDebugLogUdp = false;
    bool doOneVerboseDebugLogUdp = false;

    bool sendLiveview(AsyncWebSocketClient*);

    // incoming UDP packets, using the built-ins made the firmware crash for reasons unknown
    static const uint8_t customPacketVersion = 210;
    bool parseNotifyPacket(const uint8_t *udpPacket);

private:

    IPAddress udpSenderIp = INADDR_NONE;
    uint16_t udpSenderPort = DEADLINE_UDP_SENDER_PORT;
    // <-- let's use the same port as local port as well as remote, 'cause why not.
    bool udpSenderConnected = false;
    WiFiUDP udpSender;
    static const unsigned int MAX_UDP_SIZE = 1024;
    byte udpPacket[MAX_UDP_SIZE];
    void sendTrophyUdp();
    static const unsigned int N_RGB_VALUES = 3 * DeadlineTrophy::N_LEDS_TOTAL;
    byte rgbValues[N_RGB_VALUES];
    void readRgbValues(bool printDebug = false);
    bool applyMasterBrightnessToRgbValues = false;
    bool applyMasterBrightnessWithScaleVideo = false;

    float sendUdpInSec = 0;

    bool debugLogUdp = false;
    unsigned long lastLoggedUdpAt = 0;

    // for the JSON to monitor the power & temperature
    char jsonBuffer[512];
    float totalAmpere = 0, totalAmpereMin = 0, totalAmpereMax = 0;

    // analog readings
    uint16_t val_logoTherm[AVERAGE_SAMPLES];
    bool hasEnoughSamples = false;
    int sampleCursor = 0;
    float currentLogoTempKelvin = 0.;

    uint16_t val_inputVolt[AVERAGE_SAMPLES];
    float currentInputVoltage = 0.;

    // for troubleshooting
    float maxLogoTempKelvin = 0.;
    float minLogoTempKelvin = 9999.;
    float maxInputVoltage = 0.;
    float minInputVoltage = 9999.;

    int valueForInputCurrentSwitch = 0;
    float inputCurrentSwitch_waitSeconds = 1.;
    float inputCurrentSwitch_riseSeconds = 1.;

    long now;
    float runningSec;
    float elapsedSec;
    float justBefore;

    // logo temp values
    float _logoRead;
    float _voltageRatio;
    float _R_TH1;
    float _logRatio;

    // current temp values
    float _vccRead;

    // limit maximum current / brightness to avoid damage
    // this is multiplied on the brightness.
    // it didn't work with strip.ablMilliampsMax due to estimateCurrentAndLimitBri()
    float attenuateFactor = 1.;

    bool limit_becauseVoltageDrop = false;
    bool limit_becauseTooHot = false;

    long inputVoltageThresholdReachedAt;
    bool inputVoltageWasReachedOnce = false;
    float secondsSinceVoltageThresholdReached = 0.;

    float limit_inputVoltageThreshold = 4.6;
    // Limit by Input Voltage, implemented with guessed values
    float attenuateByV_attackFactorWhenCritical = 0.4;
    float attenuateByV_releasePerSecond = 0.005;

    float limit_criticalTempDegC = 60;
    // Limit by Temperature, also guessed values
    float attenuateByT_attackFactorWhenCritical = 0.2;
    float attenuateByT_releasePerSecond = 0.002;

    HardwareVersion hwVersion = HardwareVersion::V1;

public:

    void setup()
    {
        DEBUG_PRINTF("[DEADLINE_TROPHY] QM watches you! (non-creepily.) Say Hi to qm@z10.info\n");

#ifdef DEADLINE_UDP_SENDER_IP
        // If the default (0.0.0.0) equals "broadcast", this is going to be WAY too slow, I suppose.
        // anyway...
        if (!udpSenderIp.fromString(DEADLINE_UDP_SENDER_IP)) {
            DEBUG_PRINTF("[DEADLINE_TROPHY] UDP SENDER IP failed to set from \"%s\", is \"%s\"",
                DEADLINE_UDP_SENDER_IP,
                udpSenderIp.toString().c_str()
            );
        }
#endif
        DEBUG_PRINTF("[QM_DEBUG] UDP SENDER IP = \"%s\"\n", udpSenderIp.toString().c_str());

        justBefore = millis();
        runningSec = 0;

        hasEnoughSamples = false;
        inputVoltageWasReachedOnce = false;

        attenuateFactor = 0.;
        BusManager::setMilliampsMax(static_cast<uint16_t>(maxCurrent));

        analogSetWidth(12);

        if (digitalRead(PIN_INPUTVOLTAGE_V1) == LOW) {
            hwVersion = HardwareVersion::V2;
        } else {
            hwVersion = HardwareVersion::V1;
        }

        if (hwVersion == HardwareVersion::V1) {
            // set InputCurrentSwitch to zero.
            dacWrite(DAC1, 0);
        }

        // precalc the coordinates so their first usage doesn't totally lag
        DeadlineTrophy::logoCoordinates();
        DeadlineTrophy::baseCoordinates();
    }

    void loop()
    {
        now = millis();
        elapsedSec = 1e-3 * (now - justBefore);
        runningSec += elapsedSec;

        readRgbValues();
        sendTrophyUdp();

        readUsedCurrents();

        // qm210: below are the temperature control loops, once considered super-important
        // but then never tested and somehow also not required anymore (needs low enough constant Ampere limit)

        attenuateFactor = calcMaxCurrentLimiter(elapsedSec);

        readAnalogValues();
        if (!hasEnoughSamples) {
            return;
        }

        calcInputVoltage();
        calcLogoTherm();

        setInputCurrentSwitch_onlyV1(runningSec);

        if (currentLogoTempKelvin > maxLogoTempKelvin) {
            maxLogoTempKelvin = currentLogoTempKelvin;
        }
        if (currentLogoTempKelvin < minLogoTempKelvin) {
            minLogoTempKelvin = currentLogoTempKelvin;
        }
        if (currentInputVoltage > maxInputVoltage) {
            maxInputVoltage = currentInputVoltage;
        }
        if (currentInputVoltage < minInputVoltage) {
            minInputVoltage = currentInputVoltage;
        }

        justBefore = now;
    }

    uint8_t getAttenuated(uint8_t brightness) {
        return static_cast<uint8_t>(static_cast<float>(brightness) * attenuateFactor);
    }

    void setInputCurrentSwitch_onlyV1(float runningSec) {
        if (hwVersion != HardwareVersion::V1) {
            return;
        }
        if (!inputVoltageWasReachedOnce) {
            return;
        }
        secondsSinceVoltageThresholdReached = runningSec - inputVoltageThresholdReachedAt;
        if (secondsSinceVoltageThresholdReached > inputCurrentSwitch_waitSeconds + inputCurrentSwitch_riseSeconds) {
            return;
        }
        attenuateFactor = constrain(
            (secondsSinceVoltageThresholdReached - inputCurrentSwitch_waitSeconds) / inputCurrentSwitch_riseSeconds,
            0,
            1.
        );
        valueForInputCurrentSwitch = static_cast<int>(255 * attenuateFactor);
        dacWrite(DAC1, valueForInputCurrentSwitch);
    }

    float getAverage(uint16_t samples[])
    {
        float avg = 0;
        for (int s = 0; s < AVERAGE_SAMPLES; s++) {
            avg += samples[s];
        }
        return avg / AVERAGE_SAMPLES;
    }

    int readInputVoltage() const {
        int pin = hwVersion == HardwareVersion::V2
            ? PIN_INPUTVOLTAGE_V2
            : PIN_INPUTVOLTAGE_V1;
        return analogRead(pin);
    }

    void readAnalogValues()
    {
        // read with a delay, as ppl on se internet do it - they must know =P
        val_logoTherm[sampleCursor] = analogRead(PIN_LOGOTHERM);
        delay(10);
        val_inputVolt[sampleCursor] = readInputVoltage();
        delay(10);

        sampleCursor++;

        if (sampleCursor >= AVERAGE_SAMPLES)
        {
            hasEnoughSamples = true;
            sampleCursor = 0;
        }
    }

    void calcInputVoltage()
    {
        _vccRead = getAverage(val_inputVolt);
        currentInputVoltage = 2. * voltageAdcCoeff * _vccRead;
    }

    void calcLogoTherm()
    {
        _logoRead = getAverage(val_logoTherm);
        _R_TH1 = -logoR7_Ohm;
        _voltageRatio = currentInputVoltage / (voltageAdcCoeff * _logoRead);
        _R_TH1 += 1. / (_voltageRatio - 1) * logoR6_Ohm;
        _logRatio = logf(_R_TH1 / logoThermistor_R0_Ohm);
        currentLogoTempKelvin = 1. / ( logoTherm_OneOverT0 + logoTherm_OneOverB * _logRatio );

        // now check for NaN (in case of _logoRead == 0 or _voltageRatio == 1)
        if (currentLogoTempKelvin != currentLogoTempKelvin) {
            // unclear whether 0K is top premium choice, but it makes visible that something went wrong :P
            currentLogoTempKelvin = 0.;
        }
    }

    float calcMaxCurrentLimiter(float dt) {
        if (!hasEnoughSamples) {
            return 0;
        }

        if (!inputVoltageWasReachedOnce) {
            if (currentInputVoltage > limit_inputVoltageThreshold) {
                DEBUG_PRINTF("[DEADLINE_TROPHY] reached for the first time at %.2f\n", runningSec);
                inputVoltageWasReachedOnce = true;
                inputVoltageThresholdReachedAt = runningSec;
                limit_becauseVoltageDrop = false;
            } else {
                return 0;
            }
        }

        if (currentInputVoltage < limit_inputVoltageThreshold) {
            if (!limit_becauseVoltageDrop) {
                limit_becauseVoltageDrop = true;
                attenuateFactor *= attenuateByV_attackFactorWhenCritical;
            } else {
                attenuateFactor += attenuateByV_releasePerSecond * dt;
            }
        }

        if (currentLogoTempKelvin >= limit_criticalTempDegC + KELVIN_OFFSET) {
            if (!limit_becauseTooHot) {
                limit_becauseTooHot = true;
                attenuateFactor *= attenuateByT_attackFactorWhenCritical;
            } else {
                attenuateFactor += attenuateByT_releasePerSecond * dt;
            }
        }

        if (attenuateFactor > 1.) {
            limit_becauseVoltageDrop = false;
            limit_becauseTooHot = false;
            attenuateFactor = 1.;
        }

        return attenuateFactor;
    }

    void addToConfig(JsonObject& doc);
    void appendConfigData(Print& settingsScript);
    bool readFromConfig(JsonObject& root);
    void addToJsonState(JsonObject& obj);
    void readFromJsonState(JsonObject& obj);

    uint16_t getId()
    {
        return USERMOD_ID_DEADLINE_TROPHY;
    }

    void readUsedCurrents() {
        totalAmpere = 0.001f * BusManager::currentMilliamps();
        totalAmpereMin = totalAmpere < totalAmpereMin ? totalAmpere : totalAmpereMin;
        totalAmpereMax = totalAmpere > totalAmpereMax ? totalAmpere : totalAmpereMax;
    }

    const char* buildSocketJson()
    {
        // QM is wondering -- should we use the requestJSONBufferLock here as well?

        if (!hasEnoughSamples) {
            sprintf(jsonBuffer, "{\"error\": \"not enough samples taken yet.\"}");
            return jsonBuffer;
        }

        sprintf(
            jsonBuffer,
            "{\"dl\":{\"T\": %.3f,\"minT\": %.3f,\"maxT\": %.3f,\"VCC\": %.3f,\"maxVCC\": %.3f,\"minVCC\": %.3f,"
            "\"adcT\": %.1f,\"adcV\": %.1f,\"att\": %.2f,\"gtT\":%d,\"ltV\":%d,"
            "\"ablmA\":%d,\"A\":%.3f,\"minA\":%.3f,\"maxA\":%.3f,\"busA\":[",
            currentLogoTempKelvin - KELVIN_OFFSET,
            minLogoTempKelvin - KELVIN_OFFSET,
            maxLogoTempKelvin - KELVIN_OFFSET,
            currentInputVoltage,
            maxInputVoltage,
            minInputVoltage,
            _logoRead,
            _vccRead,
            attenuateFactor,
            limit_becauseTooHot,
            limit_becauseVoltageDrop,
            BusManager::ablMilliampsMax(),
            totalAmpere,
            totalAmpereMin,
            totalAmpereMax
        );
        // inner iteration; REMEMBER: jsonBuffer can only be referenced in itself,
        // when passed as first parameter -- it overwrites itself otherwise..! true story.
        int nBusses = BusManager::busses.size();
        for (int b = 0; b < nBusses; b++) {
            sprintf(jsonBuffer, "%s%.2f%s",
                jsonBuffer,
                0.001f * BusManager::busses[b]->getUsedCurrent(),
                b < nBusses - 1 ? "," : "]}}"
            );
        }
        return jsonBuffer;
    }

};

#define GET_DEADLINE_USERMOD() ((DeadlineUsermod*)UsermodManager::lookup(USERMOD_ID_DEADLINE_TROPHY))
