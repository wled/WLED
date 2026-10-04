#include "wled.h"

//v2 usermod that allows to change brightness and color using a rotary encoder, 
//change between modes by pressing a button (many encoders have one included)
class UsermodBrightnessFollowSun : public Usermod
{
private:
  static const char _name[];
  static const char _enabled[];
  static const char _update_interval[];
  static const char _min_bri[];
  static const char _max_bri[];
  static const char _relax_hour[];

private:
  bool enabled = false; //WLEDMM
  unsigned long update_interval = 60;
  unsigned long update_interval_ms = 60000;
  int min_bri = 1;
  int max_bri = 255;
  float relax_hour = 0;
  int relaxSec = 0;
  unsigned long lastUMRun = 0;
public:

  void setup() {};

  float mapFloat(float inputValue, float inMin, float inMax, float outMin, float outMax) {
    if (inMax == inMin) 
      return outMin;
    
    inputValue = constrain(inputValue, inMin, inMax);
    
    return ((inputValue - inMin) * (outMax - outMin) / (inMax - inMin)) + outMin;
  }

  uint16_t getId() override
  {
    return USERMOD_ID_BRIGHTNESS_FOLLOW_SUN;
  }

  void update() 
  {
    if (sunrise == 0 || sunset == 0 || localTime == 0)
      return;

    int curSec = elapsedSecsToday(localTime);
    int sunriseSec = elapsedSecsToday(sunrise);
    int sunsetSec = elapsedSecsToday(sunset);
    int sunMiddleSec = sunriseSec + (sunsetSec-sunriseSec)/2;

    int relaxSecH = sunriseSec-relaxSec;
    int relaxSecE = sunsetSec+relaxSec;

    int briSet = 0;
    if (curSec >= relaxSecH && curSec <= relaxSecE) {
      float timeMapToAngle = curSec < sunMiddleSec ?
                    mapFloat(curSec, sunriseSec, sunMiddleSec, 0, M_PI/2.0) :
                    mapFloat(curSec, sunMiddleSec, sunsetSec, M_PI/2.0, M_PI);
      float sinValue = sin_t(timeMapToAngle);
      briSet = min_bri + (max_bri-min_bri)*sinValue;
    }

    bri = briSet;
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
}

  void loop() override
  {
    if (!enabled || strip.isUpdating())
      return;

    if (millis() - lastUMRun < update_interval_ms)
      return;
    lastUMRun = millis();

    update();
  }

  void addToConfig(JsonObject& root)
  {
      JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname

      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_update_interval)] = update_interval;
      top[FPSTR(_min_bri)] = min_bri;
      top[FPSTR(_max_bri)] = max_bri;
      top[FPSTR(_relax_hour)] = relax_hour;
  }

  bool readFromConfig(JsonObject& root)
  {
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINTF("[%s] No config found. (Using defaults.)\n", _name);
      return false;
    }

    bool configComplete = true;

    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);
    configComplete &= getJsonValue(top[FPSTR(_update_interval)], update_interval, 60);
    configComplete &= getJsonValue(top[FPSTR(_min_bri)], min_bri, 1);
    configComplete &= getJsonValue(top[FPSTR(_max_bri)], max_bri, 255);
    configComplete &= getJsonValue(top[FPSTR(_relax_hour)], relax_hour, 0);
    
    update_interval = constrain(update_interval, 1, SECS_PER_HOUR);
    min_bri = constrain(min_bri, 1, 255);
    max_bri = constrain(max_bri, 1, 255);
    relax_hour = constrain(relax_hour, 0, 6);

    update_interval_ms = update_interval*1000;
    relaxSec = SECS_PER_HOUR*relax_hour;

    lastUMRun = 0;
    update();

    return configComplete;
  }
};


const char UsermodBrightnessFollowSun::_name[]                PROGMEM = "Brightness Follow Sun";
const char UsermodBrightnessFollowSun::_enabled[]             PROGMEM = "Enabled";
const char UsermodBrightnessFollowSun::_update_interval[]     PROGMEM = "Update Interval Sec";
const char UsermodBrightnessFollowSun::_min_bri[]             PROGMEM = "Min Brightness";
const char UsermodBrightnessFollowSun::_max_bri[]             PROGMEM = "Max Brightness";
const char UsermodBrightnessFollowSun::_relax_hour[]          PROGMEM = "Relax Hour";

static UsermodBrightnessFollowSun usermod_brightness_follow_sun;
REGISTER_USERMOD(usermod_brightness_follow_sun);
