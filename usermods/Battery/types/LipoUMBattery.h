#ifndef UMBLipo_h
#define UMBLipo_h

#include "../battery_defaults.h"
#include "../UMBattery.h"

/**
 *  LiPo Battery
 *  Uses a lookup table based on typical 1S LiPo discharge curve data.
 *  see https://blog.ampow.com/lipo-voltage-chart/
 */
class LipoUMBattery : public UMBattery
{
    private:
        // Typical 1S LiPo discharge curve
        static const LutEntry dischargeLut[] PROGMEM;
        static const uint8_t dischargeLutSize;

    public:
        LipoUMBattery() : UMBattery()
        {
            this->setMinVoltage(USERMOD_BATTERY_LIPO_MIN_VOLTAGE);
            this->setMaxVoltage(USERMOD_BATTERY_LIPO_MAX_VOLTAGE);
        }

        float mapVoltage(float v) override
        {
            return this->lutInterpolate(v, dischargeLut, dischargeLutSize);
        };

        void setMaxVoltage(float voltage) override
        {
            this->maxVoltage = max(getMinVoltage()+0.7f, voltage);
        }
};

const UMBattery::LutEntry LipoUMBattery::dischargeLut[] PROGMEM = {
    {4.20f, 100.0f},
    {4.15f,  95.0f},
    {4.11f,  90.0f},
    {4.08f,  85.0f},
    {4.02f,  80.0f},
    {3.98f,  75.0f},
    {3.95f,  70.0f},
    {3.91f,  65.0f},
    {3.87f,  60.0f},
    {3.85f,  55.0f},
    {3.84f,  50.0f},
    {3.82f,  45.0f},
    {3.80f,  40.0f},
    {3.79f,  35.0f},
    {3.77f,  30.0f},
    {3.75f,  25.0f},
    {3.73f,  20.0f},
    {3.71f,  15.0f},
    {3.69f,  10.0f},
    {3.61f,   5.0f},
    {3.20f,   0.0f},
};
const uint8_t LipoUMBattery::dischargeLutSize = sizeof(LipoUMBattery::dischargeLut) / sizeof(LipoUMBattery::dischargeLut[0]);

#endif
