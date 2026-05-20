#include "wled.h"

#if not defined(CONFIG_IDF_TARGET_ESP32C3)
//#error Adapted only for ESP32C3
#endif

#if not defined(THERMISTOR_GPU_PIN) or THERMISTOR_GPU_PIN not_eq 0
#error THERMISTOR_GPU_PIN NOT defined or not equals GPIO0
#endif

#if not defined(THERMISTOR_FAN_PIN) or THERMISTOR_FAN_PIN not_eq 1
#error THERMISTOR_FAN_PIN NOT defined or not equals GPIO1
#endif

#if not defined(FAN_TACHOMETER_PIN) or FAN_TACHOMETER_PIN not_eq 6
#error FAN_TACHOMETER_PIN NOT defined
#endif

#if not defined(FAN_PWM_PIN) or FAN_PWM_PIN not_eq 10
#error FAN_PWM_PIN NOT defined
#endif

#include <esp_adc_cal.h>

namespace {
    float float_map(float x, float in_min, float in_max, float out_min, float out_max) {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    class NTCThermistor {
    private:
        float r0;
        float inv_t0;
        float beta;
    public:
        explicit NTCThermistor(float r0 = 10000.0f, float beta = 3950.0f, float t0 = 25.0f)
                : r0(r0), inv_t0(1.0f / (t0 + 273.15f)), beta(beta) {}

        float calculateTemperatureFromResistance(float resistance) const {
            if (resistance <= 0.0f) return NAN;
            float inv_t = logf(resistance / r0) / beta + inv_t0;
            return 1.0f / inv_t - 273.15f;
        }
    };

    struct KobaEGPUUserModCoreConfig {
        float fan_max_speed_threshold_temperature = 70.0f;
        float fan_min_speed_threshold_temperature = 45.0f;
    };

    class KobaEGPUUserModCore {
    private:
        bool _configured = false;
        uint32_t _delta_update_time_ms = 100;
        uint32_t _pulses_count = 0;
        float _current_rpm = NAN;
        uint64_t _last_check_tachometer_speed = 0;
        ledc_channel_t _fan_pwm_channel = LEDC_CHANNEL_5;
        esp_adc_cal_characteristics_t *_adc_chars = nullptr;
        float _last_thr_gpu_temp = NAN;
        float _last_thr_fan_temp = NAN;
        NTCThermistor _thermistor = NTCThermistor(10000.0f, 3950.0f, 25.0f);
        KobaEGPUUserModCoreConfig _cfg;

        float _calc_temp(int adc_value) {
            if (adc_value > 4000 or adc_value < 100) return NAN;
            float voltage = (static_cast<float>(adc_value) / 4095.0f) * 3.0f;
            float rt = 10000.0f * (voltage / (3.3f - voltage));
            return _thermistor.calculateTemperatureFromResistance(rt);
        }

        void _update_fan_speed() {
            if (isnan(_last_thr_gpu_temp)) {
                return;
            }

            uint8_t new_pwm = _last_thr_gpu_temp > _cfg.fan_max_speed_threshold_temperature ? 255 : 0;
            if (_last_thr_gpu_temp > _cfg.fan_min_speed_threshold_temperature and
                _last_thr_gpu_temp < _cfg.fan_max_speed_threshold_temperature) {
                new_pwm = uint8_t(float_map(
                        _last_thr_gpu_temp,
                        _cfg.fan_min_speed_threshold_temperature, _cfg.fan_max_speed_threshold_temperature,
                        0, 255
                ));
            }
            _set_fan_speed(new_pwm);
        }

        void _update_temperatures() {
            auto gpu_temp = _calc_temp(adc1_get_raw(adc1_channel_t(THERMISTOR_GPU_PIN)));
            auto fan_temp = _calc_temp(adc1_get_raw(adc1_channel_t(THERMISTOR_FAN_PIN)));

            bool need_update_fan_speed = false;
            if (not isnan(fan_temp) and fan_temp not_eq _last_thr_fan_temp) {
                _last_thr_fan_temp = fan_temp;
                need_update_fan_speed = true;
            }
            if (not isnan(gpu_temp) and gpu_temp not_eq _last_thr_gpu_temp) {
                _last_thr_gpu_temp = gpu_temp;
                need_update_fan_speed = true;
            }
            if (need_update_fan_speed) _update_fan_speed();
        }

        void _update_tachometer_speed(uint32_t delta_ms) {
            float new_rpm = float(_pulses_count) * 60.0f * 1000.0f / float(delta_ms);
            _last_check_tachometer_speed = millis();
            _pulses_count = 0;
            if (not isnan(new_rpm) and new_rpm not_eq _current_rpm) {
                _current_rpm = new_rpm;
            }
        }

        void _set_fan_speed(uint8_t pwm_value) {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, _fan_pwm_channel, pwm_value));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, _fan_pwm_channel));
        }

        IRAM_ATTR static void tachometer_isr(void *ctx) {
            reinterpret_cast<KobaEGPUUserModCore *>(ctx)->_pulses_count++;
        }

    public:
        KobaEGPUUserModCore() = default;

        ~KobaEGPUUserModCore() {
            //  todo destroy adc
            delete _adc_chars;
            gpio_intr_disable(GPIO_NUM_8);
        }

        KobaEGPUUserModCoreConfig get_config() {
            return _cfg;
        }

        float get_fan_rpm() const {
            return _current_rpm;
        }

        float get_gpu_temp() const {
            return _last_thr_gpu_temp;
        }

        float get_fan_temp() const {
            return _last_thr_fan_temp;
        }

        void set_cfg(KobaEGPUUserModCoreConfig cfg) {
            if (cfg.fan_min_speed_threshold_temperature >= cfg.fan_max_speed_threshold_temperature or
                cfg.fan_min_speed_threshold_temperature < 0.0f or cfg.fan_max_speed_threshold_temperature < 10.0f) {
                return;
            }
            _cfg = cfg;
        }

        void setup() {
            ledc_timer_config_t ledc_timer = {
                    .speed_mode       = LEDC_LOW_SPEED_MODE,
                    .duty_resolution  = LEDC_TIMER_8_BIT,
                    .timer_num        = LEDC_TIMER_0,
                    .freq_hz          = 20000,
                    .clk_cfg          = LEDC_AUTO_CLK
            };
            ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
            ledc_channel_config_t ledc_channel = {
                    .gpio_num       = FAN_PWM_PIN,
                    .speed_mode     = LEDC_LOW_SPEED_MODE,
                    .channel        = _fan_pwm_channel,
                    .intr_type      = LEDC_INTR_DISABLE,
                    .timer_sel      = LEDC_TIMER_0,
                    .duty           = 0, // Set duty to 0%
                    .hpoint         = 0
            };
            ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
            _adc_chars = new esp_adc_cal_characteristics_t;
            esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, _adc_chars);
            ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
            ESP_ERROR_CHECK(adc1_config_channel_atten(adc1_channel_t(THERMISTOR_GPU_PIN), ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(adc1_channel_t(THERMISTOR_FAN_PIN), ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(gpio_set_direction(gpio_num_t(FAN_TACHOMETER_PIN), GPIO_MODE_INPUT));
            ESP_ERROR_CHECK(gpio_set_intr_type(gpio_num_t(FAN_TACHOMETER_PIN), GPIO_INTR_NEGEDGE));
            ESP_ERROR_CHECK(gpio_install_isr_service(0));
            ESP_ERROR_CHECK(gpio_isr_handler_add(gpio_num_t(FAN_TACHOMETER_PIN), tachometer_isr, this));
            _last_check_tachometer_speed = millis();
            _configured = true;
        }

        void task_iter() {
            if (not _configured) return;
            static uint64_t last_check_temp = 0;
            if (millis() - last_check_temp > _delta_update_time_ms) {
                _update_temperatures();
                last_check_temp = millis();
            }
            uint32_t delta = millis() - _last_check_tachometer_speed;
            if (delta > _delta_update_time_ms) {
                _update_tachometer_speed(delta);
            }
        }
    };
}

class KobaEGPUUsermod : public Usermod {
private:

    bool _init_done = false;
    bool _is_enabled = true;
    KobaEGPUUserModCore _core;

    static const char _mod_name_key[];
    static const char _mod_enabled_string[];
    static const char _unavailable_string[];
    static const char _celsius_prefix_string[];
    static const char _rpm_prefix_string[];
    static const char _fan_speed_string[];
    static const char _gpu_temp_string[];
    static const char _fan_temp_string[];
    static const char _fan_min_speed_threshold_temperature_key[];
    static const char _fan_max_speed_threshold_temperature_key[];
    

public:

    void setup() override {
        _core.setup();
        _init_done = true;
    }


    void loop() override {
        if (not _is_enabled or strip.isUpdating()) return;
        _core.task_iter();
    }

    void addToJsonInfo(JsonObject &root) override {
        JsonObject user = root["u"];
        if (user.isNull()) user = root.createNestedObject("u");
        {
            JsonArray infoArr = user.createNestedArray(FPSTR(_mod_name_key));
            String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({'");
            uiDomString += FPSTR(_mod_name_key);
            uiDomString += F("':{'");
            uiDomString += FPSTR(_mod_enabled_string);
            uiDomString += F("':");
            uiDomString += _is_enabled ? "false" : "true";
            uiDomString += F("}});\"><i class=\"icons ");
            uiDomString += _is_enabled ? "on" : "off";
            uiDomString += F("\">&#xe08f;</i></button>");
            infoArr.add(uiDomString);
        }
        if (_is_enabled) {
            {
                JsonArray data = user.createNestedArray(_fan_speed_string);
                if (not isnan(_core.get_fan_rpm())) {
                    data.add(uint32_t(_core.get_fan_rpm()));
                    data.add(_rpm_prefix_string);
                } else {
                    data.add(_unavailable_string);
                }
            }
            {
                JsonArray data = user.createNestedArray(_gpu_temp_string);
                if (not isnan(_core.get_gpu_temp())) {
                    data.add(_core.get_gpu_temp());
                    data.add(_celsius_prefix_string);
                } else {
                    data.add(_unavailable_string);
                }
            }
            {
                JsonArray data = user.createNestedArray(_fan_temp_string);
                if (not isnan(_core.get_fan_temp())) {
                    data.add(_core.get_fan_temp());
                    data.add(_celsius_prefix_string);
                } else {
                    data.add(_unavailable_string);
                }
            }
        }
    }


    void readFromJsonState(JsonObject &root) override {
        // from info section
        if (not _init_done) return;
        JsonObject usermod = root[FPSTR(_mod_name_key)];

        if (usermod.isNull()) return;
        if (usermod[FPSTR(_mod_enabled_string)].is<bool>()) {
            _is_enabled = usermod[FPSTR(_mod_enabled_string)].as<bool>();
        }
    }

    void addToConfig(JsonObject &root) override {
        JsonObject top = root.createNestedObject(FPSTR(_mod_name_key));
        auto cfg = _core.get_config();
        top[FPSTR(_mod_enabled_string)] = _is_enabled;
        top[FPSTR(_fan_min_speed_threshold_temperature_key)] = cfg.fan_min_speed_threshold_temperature;
        top[FPSTR(_fan_max_speed_threshold_temperature_key)] = cfg.fan_max_speed_threshold_temperature;
    }

    bool readFromConfig(JsonObject &root) override {
        // from usermod settings
        JsonObject top = root[FPSTR(_mod_name_key)];
        if (top.isNull()) {
            return false;
        }
        _is_enabled = top[FPSTR(_mod_enabled_string)] | _is_enabled;
        KobaEGPUUserModCoreConfig cfg;
        cfg.fan_min_speed_threshold_temperature =
                top[FPSTR(_fan_min_speed_threshold_temperature_key)] | cfg.fan_min_speed_threshold_temperature;
        cfg.fan_max_speed_threshold_temperature =
                top[FPSTR(_fan_max_speed_threshold_temperature_key)] | cfg.fan_max_speed_threshold_temperature;
        _core.set_cfg(cfg);
        return not top[FPSTR(_mod_enabled_string)].isNull();
    }

    uint16_t getId() override { return USERMOD_ID_KOBA_EGPU; }
};


const char KobaEGPUUsermod::_mod_name_key[] PROGMEM = "KOBA eGPU";
const char KobaEGPUUsermod::_mod_enabled_string[] PROGMEM = "Mod enabled";
const char KobaEGPUUsermod::_unavailable_string[] PROGMEM = "n/d";
const char KobaEGPUUsermod::_celsius_prefix_string[] PROGMEM = " (C)";
const char KobaEGPUUsermod::_rpm_prefix_string[] PROGMEM = " RPM";
const char KobaEGPUUsermod::_fan_speed_string[] PROGMEM = "FAN Speed";
const char KobaEGPUUsermod::_gpu_temp_string[] = "GPU Temp";
const char KobaEGPUUsermod::_fan_temp_string[] = "FAN Temp";
const char KobaEGPUUsermod::_fan_min_speed_threshold_temperature_key[] = "Fan min speed threshold temperature (C)";
const char KobaEGPUUsermod::_fan_max_speed_threshold_temperature_key[] = "Fan max speed threshold temperature (C)";

static KobaEGPUUsermod koba_egpu;
REGISTER_USERMOD(koba_egpu);