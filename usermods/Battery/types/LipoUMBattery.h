#ifndef UMBLipo_h
#define UMBLipo_h

#include "../battery_defaults.h"
#include "../UMBattery.h"

/**
 *  LiPo Battery
 * 
 */
class LipoUMBattery : public UMBattery
{
    private:

    public:
        LipoUMBattery() : UMBattery()
        {
            this->setMinVoltage(USERMOD_BATTERY_LIPO_MIN_VOLTAGE);
            this->setMaxVoltage(USERMOD_BATTERY_LIPO_MAX_VOLTAGE);
        }

        /**
         * LiPo batteries have a differnt discharge curve, see 
         * https://blog.ampow.com/lipo-voltage-chart/
         */
        float mapVoltage(float v, float min, float max) override 
        {
            float lvl = 0.0f;
            lvl = this->linearMapping(v, min, max); // basic mapping

            if (lvl < 40.0f) 
                lvl = this->linearMapping(lvl, 0, 40, 0, 12);       // last 45% -> drops very quickly
            else {
            if (lvl < 90.0f)
                lvl = this->linearMapping(lvl, 40, 90, 12, 95);     // 90% ... 40% -> almost linear drop
            else // level >  90%
                lvl = this->linearMapping(lvl, 90, 105, 95, 100);   // highest 15% -> drop slowly
            }

            return lvl;
        };

        void calculateAndSetLevel(float voltage) override
        {
            this->setLevel(this->mapVoltage(voltage, this->getMinVoltage(), this->getMaxVoltage()));
        };

        virtual void setMaxVoltage(float voltage) override
        {
            this->maxVoltage = max(getMinVoltage()+0.7f, voltage);
        }
};

#endif