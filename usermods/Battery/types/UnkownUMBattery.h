#ifndef UMBUnkown_h
#define UMBUnkown_h

#include "../battery_defaults.h"
#include "../UMBattery.h"

/**
 *  Unkown / Default Battery
 * 
 */
class UnkownUMBattery : public UMBattery
{
    private:

    public:
        UnkownUMBattery() : UMBattery()
        {
            this->setMinVoltage(USERMOD_BATTERY_UNKOWN_MIN_VOLTAGE);
            this->setMaxVoltage(USERMOD_BATTERY_UNKOWN_MAX_VOLTAGE);
        }

        void update(batteryConfig cfg)
        {
            if(cfg.minVoltage) this->setMinVoltage(cfg.minVoltage); else this->setMinVoltage(USERMOD_BATTERY_UNKOWN_MIN_VOLTAGE);
            if(cfg.maxVoltage) this->setMaxVoltage(cfg.maxVoltage); else this->setMaxVoltage(USERMOD_BATTERY_UNKOWN_MAX_VOLTAGE);
        }

        float mapVoltage(float v, float min, float max) override
        {
            return this->linearMapping(v, min, max); // basic mapping
        };

        void calculateAndSetLevel(float voltage) override
        {
            this->setLevel(this->mapVoltage(voltage, this->getMinVoltage(), this->getMaxVoltage()));
        };
};

#endif