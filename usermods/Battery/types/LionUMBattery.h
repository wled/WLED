#ifndef UMBLion_h
#define UMBLion_h

#include "../battery_defaults.h"
#include "../UMBattery.h"

/**
 *  LiOn Battery
 * 
 */
class LionUMBattery : public UMBattery
{
    private:

    public:
        LionUMBattery() : UMBattery()
        {
            this->setMinVoltage(USERMOD_BATTERY_LION_MIN_VOLTAGE);
            this->setMaxVoltage(USERMOD_BATTERY_LION_MAX_VOLTAGE);
        }

        float mapVoltage(float v, float min, float max) override
        {
            return this->linearMapping(v, min, max); // basic mapping
        };

        void calculateAndSetLevel(float voltage) override
        {
            this->setLevel(this->mapVoltage(voltage, this->getMinVoltage(), this->getMaxVoltage()));
        };

        virtual void setMaxVoltage(float voltage) override
        {
            this->maxVoltage = max(getMinVoltage()+1.0f, voltage);
        }
};

#endif