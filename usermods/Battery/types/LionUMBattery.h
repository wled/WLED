#ifndef UMBLion_h
#define UMBLion_h

#include "../battery_defaults.h"
#include "../UMBattery.h"

/**
 *  LiOn Battery
 *  Uses a lookup table based on typical 18650 discharge curve data.
 */
class LionUMBattery : public UMBattery
{
    private:
        // Typical 18650 Li-Ion discharge curve
        static const LutEntry dischargeLut[] PROGMEM;
        static const uint8_t dischargeLutSize;

    public:
        LionUMBattery() : UMBattery()
        {
            this->setMinVoltage(USERMOD_BATTERY_LION_MIN_VOLTAGE);
            this->setMaxVoltage(USERMOD_BATTERY_LION_MAX_VOLTAGE);
        }

        float mapVoltage(float v) override
        {
            return this->lutInterpolate(v, dischargeLut, dischargeLutSize);
        };

        void setMaxVoltage(float voltage) override
        {
            this->maxVoltage = max(getMinVoltage()+1.0f, voltage);
        }
};

const UMBattery::LutEntry LionUMBattery::dischargeLut[] PROGMEM = {
    {4.20f, 100.0f},
    {4.10f,  90.0f},
    {3.97f,  80.0f},
    {3.87f,  70.0f},
    {3.79f,  60.0f},
    {3.73f,  50.0f},
    {3.68f,  40.0f},
    {3.63f,  30.0f},
    {3.55f,  20.0f},
    {3.40f,  10.0f},
    {3.20f,   5.0f},
    {2.60f,   0.0f},
};
const uint8_t LionUMBattery::dischargeLutSize = sizeof(LionUMBattery::dischargeLut) / sizeof(LionUMBattery::dischargeLut[0]);

#endif
