#ifndef UMBLifepo4_h
#define UMBLifepo4_h

#include "../battery_defaults.h"
#include "../UMBattery.h"

/**
 *  LiFePO4 Battery
 *  Uses a lookup table based on typical LiFePO4 discharge curve data.
 *  LiFePO4 cells have a very flat discharge curve between ~3.3V and ~3.2V
 *  making voltage-based SoC estimation difficult in the mid-range.
 */
class Lifepo4UMBattery : public UMBattery
{
    private:
        // Typical LiFePO4 discharge curve
        static const LutEntry dischargeLut[] PROGMEM;
        static const uint8_t dischargeLutSize;

    public:
        Lifepo4UMBattery() : UMBattery()
        {
            this->setMinVoltage(USERMOD_BATTERY_LIFEPO4_MIN_VOLTAGE);
            this->setMaxVoltage(USERMOD_BATTERY_LIFEPO4_MAX_VOLTAGE);
        }

        float mapVoltage(float v) override
        {
            return this->lutInterpolate(v, dischargeLut, dischargeLutSize);
        };

        void setMaxVoltage(float voltage) override
        {
            this->maxVoltage = max(getMinVoltage()+0.5f, voltage);
        }
};

const UMBattery::LutEntry Lifepo4UMBattery::dischargeLut[] PROGMEM = {
    {3.60f, 100.0f},
    {3.40f,  99.0f},
    {3.35f,  90.0f},
    {3.33f,  80.0f},
    {3.30f,  70.0f},
    {3.28f,  55.0f},
    {3.25f,  40.0f},
    {3.20f,  20.0f},
    {3.10f,  10.0f},
    {3.00f,   5.0f},
    {2.80f,   0.0f},
};
const uint8_t Lifepo4UMBattery::dischargeLutSize = sizeof(Lifepo4UMBattery::dischargeLut) / sizeof(Lifepo4UMBattery::dischargeLut[0]);

#endif
