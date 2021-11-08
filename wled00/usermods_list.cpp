#include "wled.h"
/*
 * Register your v2 usermods here!
 *   (for v1 usermods using just usermod.cpp, you can ignore this file)
 */

/*
 * Add/uncomment your usermod filename here (and once more below)
 * || || ||
 * \/ \/ \/
 */
//#include "../usermods/EXAMPLE_v2/usermod_v2_example.h"

#ifdef USERMOD_BATTERY_STATUS_BASIC
#include "../usermods/battery_status_basic/usermod_v2_battery_status_basic.h"
#endif

#ifdef USERMOD_DALLASTEMPERATURE
#include "../usermods/Temperature/usermod_temperature.h"
#endif

#ifdef USERMOD_SN_PHOTORESISTOR
#include "../usermods/SN_Photoresistor/usermod_sn_photoresistor.h"
#endif

#ifdef USERMOD_PWM_FAN
#include "../usermods/PWM_fan/usermod_PWM_fan.h"
#endif

#ifdef USERMOD_BUZZER
#include "../usermods/buzzer/usermod_v2_buzzer.h"
#endif
#ifdef USERMOD_SENSORSTOMQTT
#include "../usermods/sensors_to_mqtt/usermod_v2_SensorsToMqtt.h"
#endif
#ifdef USERMOD_PIRSWITCH
#include "../usermods/PIR_sensor_switch/usermod_PIR_sensor_switch.h"
#endif

#ifdef USERMOD_MODE_SORT
#include "../usermods/usermod_v2_mode_sort/usermod_v2_mode_sort.h"
#endif

// BME280 v2 usermod. Define "USERMOD_BME280" in my_config.h
#ifdef USERMOD_BME280
#include "../usermods/BME280_v2/usermod_bme280.h"
#endif
#ifdef USERMOD_FOUR_LINE_DISPLAY
  #ifdef USE_ALT_DISPlAY
    #include "../usermods/usermod_v2_four_line_display_ALT/usermod_v2_four_line_display_ALT.h"
  #else 
    #include "../usermods/usermod_v2_four_line_display/usermod_v2_four_line_display.h"
  #endif
#endif
#ifdef USERMOD_ROTARY_ENCODER_UI
  #ifdef USE_ALT_DISPlAY
    #include "../usermods/usermod_v2_rotary_encoder_ui_ALT/usermod_v2_rotary_encoder_ui_ALT.h"
  #else
    #include "../usermods/usermod_v2_rotary_encoder_ui/usermod_v2_rotary_encoder_ui.h"
  #endif
#endif
#ifdef USERMOD_AUTO_SAVE
#include "../usermods/usermod_v2_auto_save/usermod_v2_auto_save.h"
#endif

#ifdef USERMOD_DHT
#include "../usermods/DHT/usermod_dht.h"
#endif

#ifdef USERMOD_VL53L0X_GESTURES
#include <Wire.h> //it's needed here to correctly resolve dependencies
#include "../usermods/VL53L0X_gestures/usermod_vl53l0x_gestures.h"
#endif

#ifdef USERMOD_ANIMATED_STAIRCASE
#include "../usermods/Animated_Staircase/Animated_Staircase.h"
#endif

#ifdef USERMOD_MULTI_RELAY
#include "../usermods/multi_relay/usermod_multi_relay.h"
#endif

#ifdef USERMOD_RTC
#include "../usermods/RTC/usermod_rtc.h"
#endif

#ifdef USERMOD_ELEKSTUBE_IPS
#include "../usermods/EleksTube_IPS/usermod_elekstube_ips.h"
#endif

#ifdef USERMOD_ROTARY_ENCODER_BRIGHTNESS_COLOR
#include "../usermods/usermod_rotary_brightness_color/usermod_rotary_brightness_color.h"
#endif

#ifdef RGB_ROTARY_ENCODER
#include "../usermods/rgb-rotary-encoder/rgb-rotary-encoder.h"
#endif

#ifdef USERMOD_SEVEN_SEGMENT
#include "../usermods/seven_segment_display/usermod_v2_seven_segment_display.h"
#endif

#ifdef QUINLED_AN_PENTA
#include "../usermods/quinled-an-penta/quinled-an-penta.h"
#endif

void registerUsermods()
{
/*
   * Add your usermod class name here
   * || || ||
   * \/ \/ \/
   */
  //usermods.add(new MyExampleUsermod());

  #ifdef USERMOD_BATTERY_STATUS_BASIC
  usermods.add(new UsermodBatteryBasic());
  #endif

  #ifdef USERMOD_DALLASTEMPERATURE
  usermods.add(new UsermodTemperature());
  #endif

  #ifdef USERMOD_SN_PHOTORESISTOR
  usermods.add(new Usermod_SN_Photoresistor());
  #endif

  #ifdef USERMOD_PWM_FAN
  usermods.add(new PWMFanUsermod());
  #endif

  #ifdef USERMOD_BUZZER
  usermods.add(new BuzzerUsermod());
  #endif

  #ifdef USERMOD_BME280
  usermods.add(new UsermodBME280());
  #endif
  #ifdef USERMOD_SENSORSTOMQTT
  usermods.add(new UserMod_SensorsToMQTT());
  #endif
  #ifdef USERMOD_PIRSWITCH
  usermods.add(new PIRsensorSwitch());
  #endif

  #ifdef USERMOD_MODE_SORT
  usermods.add(new ModeSortUsermod());
  #endif
  #ifdef USERMOD_FOUR_LINE_DISPLAY
  usermods.add(new FourLineDisplayUsermod());
  #endif
  #ifdef USERMOD_ROTARY_ENCODER_UI
  usermods.add(new RotaryEncoderUIUsermod()); // can use USERMOD_FOUR_LINE_DISPLAY
  #endif
  #ifdef USERMOD_AUTO_SAVE
  usermods.add(new AutoSaveUsermod());  // can use USERMOD_FOUR_LINE_DISPLAY
  #endif

  #ifdef USERMOD_DHT
  usermods.add(new UsermodDHT());
  #endif

  #ifdef USERMOD_VL53L0X_GESTURES
  usermods.add(new UsermodVL53L0XGestures());
  #endif

  #ifdef USERMOD_ANIMATED_STAIRCASE
  usermods.add(new Animated_Staircase());
  #endif

  #ifdef USERMOD_MULTI_RELAY
  usermods.add(new MultiRelay());
  #endif

  #ifdef USERMOD_RTC
  usermods.add(new RTCUsermod());
  #endif

  #ifdef USERMOD_ELEKSTUBE_IPS
  usermods.add(new ElekstubeIPSUsermod());
  #endif

  #ifdef USERMOD_ROTARY_ENCODER_BRIGHTNESS_COLOR
  usermods.add(new RotaryEncoderBrightnessColor());
  #endif

  #ifdef RGB_ROTARY_ENCODER
  usermods.add(new RgbRotaryEncoderUsermod());
  #endif

  #ifdef USERMOD_SEVEN_SEGMENT
  usermods.add(new SevenSegmentDisplay());
  #endif

  #ifdef QUINLED_AN_PENTA
  usermods.add(new QuinLEDAnPentaUsermod());
  #endif
}
