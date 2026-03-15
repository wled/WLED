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

    public:
        /**
         * Lookup table entry for voltage-to-percentage mapping.
         * Table must be sorted descending by voltage.
         */
        struct LutEntry { float voltage; float percent; };

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

        float lutInterpolate(float v, const LutEntry* lut, uint8_t size)
        {
            if (size == 0) return 0.0f;

            LutEntry first, last;
            memcpy_P(&first, &lut[0], sizeof(LutEntry));
            memcpy_P(&last, &lut[size-1], sizeof(LutEntry));

            if (v >= first.voltage) return first.percent;
            if (v <= last.voltage) return last.percent;

            for (uint8_t i = 0; i < size - 1; i++) {
                LutEntry hi, lo;
                memcpy_P(&hi, &lut[i], sizeof(LutEntry));
                memcpy_P(&lo, &lut[i+1], sizeof(LutEntry));

                if (v >= lo.voltage) {
                    float span = hi.voltage - lo.voltage;
                    if (fabsf(span) < 1e-6f) return hi.percent;
                    float ratio = (v - lo.voltage) / span;
                    return lo.percent + ratio * (hi.percent - lo.percent);
                }
            }
            return last.percent;
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
         * calculates the level in % (0-100) with given voltage
         */
        virtual float mapVoltage(float v) = 0;

        void calculateAndSetLevel(float voltage)
        {
            this->setLevel(this->mapVoltage(voltage));
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
        virtual void setMinVoltage(float voltage)
        {
            this->minVoltage = max(0.0f, voltage);
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
        virtual void setMaxVoltage(float voltage)
        {
            this->maxVoltage = max(getMinVoltage()+.5f, voltage);
        }

        float getVoltage()
        {
            return this->voltage;
        }

        /**
         * check if voltage is within specified voltage range, allow 10% over/under voltage
         */
        void setVoltage(float voltage)
        {
            // this->voltage = ( (voltage < this->getMinVoltage() * 0.85f) || (voltage > this->getMaxVoltage() * 1.1f) ) 
            //     ? -1.0f 
            //     : voltage;
            this->voltage = voltage;
        }

        int8_t getLevel()
        {
            return this->level;
        }

        void setLevel(float level)
        {
            this->level = (int8_t)constrain(level, 0.0f, 110.0f);
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