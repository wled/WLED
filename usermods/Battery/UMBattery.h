#ifndef UMBBattery_h
#define UMBBattery_h

#include "battery_defaults.h"

/**
 *  Battery base class
 *  all other battery classes should inherit from this
 */
class UMBattery
{
    private:

    protected:
        float minVoltage;
        float maxVoltage;
        float voltage;
        int8_t level = 100;
        float calibration; // offset or calibration value to fine tune the calculated voltage
        float voltageMultiplier; // ratio for the voltage divider

        float linearMapping(float v, float min, float max, float oMin = 0.0f, float oMax = 100.0f)
        {
            return (v-min) * (oMax-oMin) / (max-min) + oMin;
        }

    public:
        UMBattery()
        {
            this->setVoltageMultiplier(USERMOD_BATTERY_VOLTAGE_MULTIPLIER);
            this->setCalibration(USERMOD_BATTERY_CALIBRATION);
        }

        virtual ~UMBattery() = default;

        virtual void update(batteryConfig cfg)
        {
            if(cfg.minVoltage) this->setMinVoltage(cfg.minVoltage);
            if(cfg.maxVoltage) this->setMaxVoltage(cfg.maxVoltage);
            if(cfg.level) this->setLevel(cfg.level);
            if(cfg.calibration) this->setCalibration(cfg.calibration);
            if(cfg.voltageMultiplier) this->setVoltageMultiplier(cfg.voltageMultiplier);
        }

        /**
         * Corresponding battery curves
         * calculates the level in % (0-100) with given voltage and possible voltage range
         */
        virtual float mapVoltage(float v, float min, float max) = 0;

        void calculateAndSetLevel(float voltage)
        {
            this->setLevel(this->mapVoltage(voltage, this->getMinVoltage(), this->getMaxVoltage()));
        }



        /*
         *
         * Getter and Setter
         *
         */

        /*
        * Get lowest configured battery voltage
        */
        virtual float getMinVoltage()
        {
            return this->minVoltage;
        }

        /*
         * Set lowest battery voltage
         * can't be below 0 volt
         */
        virtual void setMinVoltage(float newVoltage)
        {
            this->minVoltage = max(0.0f, newVoltage);
        }

        /*
         * Get highest configured battery voltage
         */
        virtual float getMaxVoltage()
        {
            return this->maxVoltage;
        }

        /*
         * Set highest battery voltage
         * can't be below minVoltage
         */
        virtual void setMaxVoltage(float newVoltage)
        {
            this->maxVoltage = max(getMinVoltage()+.5f, newVoltage);
        }

        float getVoltage()
        {
            return this->voltage;
        }

        /**
         * check if voltage is within specified voltage range, allow 15% under / 10% over voltage
         */
        bool isValidVoltage(float newVoltage)
        {
            return newVoltage >= this->getMinVoltage() * 0.85f && newVoltage <= this->getMaxVoltage() * 1.1f;
        }

        // out-of-range readings are stored as -1 (invalid)
        void setVoltage(float newVoltage)
        {
            this->voltage = this->isValidVoltage(newVoltage) ? newVoltage : -1.0f;
        }

        int8_t getLevel()
        {
            return this->level;
        }

        /*
         * The 110% upper bound is headroom for charging / USB charger voltage above
         * the discharge curve's 100% point. Values above 100 are a voltage-derived
         * indication, not physical state of charge.
         */
        void setLevel(float newLevel)
        {
            this->level = (int8_t)constrain(newLevel, 0.0f, 110.0f);
        }

        // mark level as unknown (-1), e.g. after an out-of-range voltage reading
        void invalidateLevel()
        {
            this->level = -1;
        }

        /*
         * Get the configured calibration value
         * a offset value to fine-tune the calculated voltage.
         */
        virtual float getCalibration()
        {
            return calibration;
        }

        /*
         * Set the voltage calibration offset value
         * a offset value to fine-tune the calculated voltage.
         */
        virtual void setCalibration(float offset)
        {
            calibration = offset;
        }

        /*
         * Get the configured calibration value
         * a value to set the voltage divider ratio
         */
        virtual float getVoltageMultiplier()
        {
            return voltageMultiplier;
        }

        /*
         * Set the voltage multiplier value
         * a value to set the voltage divider ratio.
         */
        virtual void setVoltageMultiplier(float multiplier)
        {
            voltageMultiplier = multiplier;
        }
};

#endif
