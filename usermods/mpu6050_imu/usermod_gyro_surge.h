#pragma once

/* This usermod uses gyro data to provide a "surge" effect on movement

Requires lib_deps = bolderflight/Bolder Flight Systems Eigen@^3.0.0

*/

#include "wled.h"

// Eigen include block
#ifdef A0
namespace { constexpr size_t A0_temp {A0}; }
#undef A0
static constexpr size_t A0 {A0_temp};
#endif

#ifdef A1
namespace { constexpr size_t A1_temp {A1}; }
#undef A1
static constexpr size_t A1 {A1_temp};
#endif

#ifdef B0
namespace { constexpr size_t B0_temp {B0}; }
#undef B0
static constexpr size_t B0 {B0_temp};
#endif

#ifdef B1
namespace { constexpr size_t B1_temp {B1}; }
#undef B1
static constexpr size_t B1 {B1_temp};
#endif

#ifdef D0
namespace { constexpr size_t D0_temp {D0}; }
#undef D0
static constexpr size_t D0 {D0_temp};
#endif

#ifdef D1
namespace { constexpr size_t D1_temp {D1}; }
#undef D1
static constexpr size_t D1 {D1_temp};
#endif

#ifdef D2
namespace { constexpr size_t D2_temp {D2}; }
#undef D2
static constexpr size_t D2 {D2_temp};
#endif

#ifdef D3
namespace { constexpr size_t D3_temp {D3}; }
#undef D3
static constexpr size_t D3 {D3_temp};
#endif

#include "eigen.h"
#include <Eigen/Geometry>

constexpr auto ESTIMATED_G = 9.801;  // m/s^2
constexpr auto ESTIMATED_G_COUNTS = 8350.;
constexpr auto ESTIMATED_ANGULAR_RATE = (M_PI * 2000) / (INT16_MAX * 180); // radians per second

// Horribly lame digital filter code
// Currently implements a static IIR filter.
template<typename T, unsigned C>
class xir_filter {
    typedef Eigen::Array<T, C, 1> array_t;
    const array_t a_coeff, b_coeff;
    const T gain;
    array_t x, y;

    public:
    xir_filter(T gain_, array_t a, array_t b) : a_coeff(std::move(a)), b_coeff(std::move(b)), gain(gain_), x(array_t::Zero()), y(array_t::Zero()) {};

    T operator()(T input) {
        x.head(C-1) = x.tail(C-1);  // shift by one
        x(C-1) = input / gain;
        y.head(C-1) = y.tail(C-1);  // shift by one
        y(C-1) = (x * b_coeff).sum();
        y(C-1) -= (y.head(C-1) * a_coeff.head(C-1)).sum();
        return y(C-1);
    }

    T last() { return y(C-1); };
};



class GyroSurge : public Usermod {
  private:
    static const char _name[];
    bool enabled = true;

    // Params
    uint8_t max = 0;
    float sensitivity = 0;

    // State
    uint32_t last_sample;
    // 100hz input
    // butterworth low pass filter at 20hz
    xir_filter<float, 3> filter = { 1., { -0.36952738, 0.19581571, 1.}, {0.20657208, 0.41314417, 0.20657208} };
                                  // { 1., { 0., 0., 1.}, { 0., 0., 1. } }; // no filter


  public:

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     */
    void setup() {};


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));

      //save these vars persistently whenever settings are saved
      top["max"] = max;
      top["sensitivity"] = sensitivity;
    }


    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
     * 
     * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
     * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
     * 
     * This function is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root)
    {
      // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top["max"], max, 0);
      configComplete &= getJsonValue(top["sensitivity"], sensitivity, 10);

      return configComplete;
    }

    void loop() {
      // get IMU data
      um_data_t *um_data;
      if (!UsermodManager::getUMData(&um_data, USERMOD_ID_IMU)) {
        // Apply max
        strip.getSegment(0).fadeToBlackBy(max);
        return;
      }
      uint32_t sample_count = *(uint32_t*)(um_data->u_data[8]);

      if (sample_count != last_sample) {        
        last_sample = sample_count;
        // Calculate based on new data
        // We use the raw gyro data (angular rate)        
        auto gyros = (int16_t*)um_data->u_data[4];  // 16384 == 2000 deg/s

        // Compute the overall rotation rate
        // For my application (a plasma sword) we ignore X axis rotations (eg. around the long axis)
        auto gyro_q = Eigen::AngleAxis<float> {
                        //Eigen::AngleAxis<float>(ESTIMATED_ANGULAR_RATE * gyros[0], Eigen::Vector3f::UnitX()) *
                        Eigen::AngleAxis<float>(ESTIMATED_ANGULAR_RATE * gyros[1], Eigen::Vector3f::UnitY()) *
                        Eigen::AngleAxis<float>(ESTIMATED_ANGULAR_RATE * gyros[2], Eigen::Vector3f::UnitZ()) };
        
        // Filter the results
        filter(std::min(sensitivity * gyro_q.angle(), 1.0f));   // radians per second
/*
        Serial.printf("[%lu] Gy: %d, %d, %d -- ", millis(), (int)gyros[0], (int)gyros[1], (int)gyros[2]);
        Serial.print(gyro_q.angle());
        Serial.print(", ");
        Serial.print(sensitivity * gyro_q.angle());
        Serial.print(" --> ");
        Serial.println(filter.last());
*/
      }
    }; // noop

    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw()
    {

      // TODO: some kind of timing analysis for filtering ...

      // Calculate brightness boost
      auto r_float = std::max(std::min(filter.last(), 1.0f), 0.f);
      auto result = (uint8_t) (r_float * max);
      //Serial.printf("[%lu] %d -- ", millis(), result);
      //Serial.println(r_float);
      // TODO - multiple segment handling??
      strip.getSegment(0).fadeToBlackBy(max - result);
    }
};

const char GyroSurge::_name[] PROGMEM = "GyroSurge";