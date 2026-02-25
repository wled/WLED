#ifndef UMBUnknown_h
#define UMBUnknown_h

#include "../battery_defaults.h"
#include "../UMBattery.h"

/**
 *  Unknown / Default Battery
 *  Uses a generic discharge curve LUT that approximates a typical
 *  single-cell lithium battery. Since the chemistry is unknown,
 *  the curve is a mild non-linear shape between configured min/max.
 */
class UnknownUMBattery : public UMBattery
{
    private:
        // Generic single-cell discharge curve
        // Mild non-linear shape, works reasonably for most lithium chemistries
        static const LutEntry dischargeLut[] PROGMEM;
        static const uint8_t dischargeLutSize;

    public:
        UnknownUMBattery() : UMBattery()
        {
            this->setMinVoltage(USERMOD_BATTERY_UNKNOWN_MIN_VOLTAGE);
            this->setMaxVoltage(USERMOD_BATTERY_UNKNOWN_MAX_VOLTAGE);
        }

        float mapVoltage(float v) override
        {
            return this->lutInterpolate(v, dischargeLut, dischargeLutSize);
        };
};

const UMBattery::LutEntry UnknownUMBattery::dischargeLut[] PROGMEM = {
    {4.20f, 100.0f},
    {4.10f,  90.0f},
    {3.95f,  75.0f},
    {3.80f,  50.0f},
    {3.70f,  30.0f},
    {3.60f,  15.0f},
    {3.50f,   5.0f},
    {3.30f,   0.0f},
};
const uint8_t UnknownUMBattery::dischargeLutSize = sizeof(UnknownUMBattery::dischargeLut) / sizeof(UnknownUMBattery::dischargeLut[0]);

#endif
