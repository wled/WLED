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

#ifdef USERMOD_BATTERY
#include "../usermods/Battery/usermod_v2_Battery.h"
#endif

#ifdef USERMOD_DALLASTEMPERATURE
#include "../usermods/Temperature/usermod_temperature.h"
#endif

#ifdef USERMOD_SHT
#include "../usermods/sht/usermod_sht.h"
#endif

#ifdef USERMOD_SN_PHOTORESISTOR
#include "../usermods/SN_Photoresistor/usermod_sn_photoresistor.h"
#endif

#ifdef USERMOD_PWM_FAN
// requires DALLASTEMPERATURE or SHT included before it
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

#ifdef USERMOD_BH1750
#include "../usermods/BH1750_v2/usermod_bh1750.h" //WLEDMM: usermod_bh1750.h in small caps!
#endif

// BME280 v2 usermod. Define "USERMOD_BME280" in my_config.h
#ifdef USERMOD_BME280
#include "../usermods/BME280_v2/usermod_bme280.h"
#endif

#ifdef USERMOD_FOUR_LINE_DISPLAY
#if defined(USE_ALT_DISPLAY) || defined(USE_ALT_DISPlAY)
#include "../usermods/usermod_v2_four_line_display_ALT/usermod_v2_four_line_display_ALT.h"
#else
#include "../usermods/usermod_v2_four_line_display/usermod_v2_four_line_display.h"
#endif
#endif

#ifdef USERMOD_ROTARY_ENCODER_UI
#if defined(USE_ALT_DISPLAY) || defined(USE_ALT_DISPlAY)
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

#ifdef USERMOD_ST7789_DISPLAY
#include "../usermods/ST7789_display/ST7789_Display.h"
#endif

#ifdef USERMOD_SEVEN_SEGMENT
#include "../usermods/seven_segment_display/usermod_v2_seven_segment_display.h"
#endif

#ifdef USERMOD_SSDR
#include "../usermods/seven_segment_display_reloaded/usermod_seven_segment_reloaded.h"
#endif

#ifdef USERMOD_CRONIXIE
#include "../usermods/Cronixie/usermod_cronixie.h"
#endif

#ifdef QUINLED_AN_PENTA
#include "../usermods/quinled-an-penta/quinled-an-penta.h"
#endif

#ifdef USERMOD_WIZLIGHTS
#include "../usermods/wizlights/wizlights.h"
#endif

#ifdef USERMOD_WORDCLOCK
#include "../usermods/usermod_v2_word_clock/usermod_v2_word_clock.h"
#endif

#ifdef USERMOD_MY9291
#include "../usermods/MY9291/usermode_MY9291.h"
#endif

#ifdef USERMOD_SI7021_MQTT_HA
#include "../usermods/Si7021_MQTT_HA/usermod_si7021_mqtt_ha.h"
#endif

#ifdef USERMOD_SMARTNEST
#include "../usermods/smartnest/usermod_smartnest.h"
#endif

#ifdef USERMOD_AUDIOREACTIVE
#include "../usermods/audioreactive/audio_reactive.h"
#endif

#ifdef USERMOD_ANALOG_CLOCK
#include "../usermods/Analog_Clock/Analog_Clock.h"
#endif

#ifdef USERMOD_PING_PONG_CLOCK
#include "../usermods/usermod_v2_ping_pong_clock/usermod_v2_ping_pong_clock.h"
#endif

#ifdef USERMOD_ADS1115
#include "../usermods/ADS1115_v2/usermod_ads1115.h"
#endif

#ifdef USERMOD_KLIPPER_PERCENTAGE
#include "..\usermods\usermod_v2_klipper_percentage\usermod_v2_klipper_percentage.h"
#endif

#ifdef USERMOD_BOBLIGHT
#include "../usermods/boblight/boblight.h"
#endif

#ifdef USERMOD_MCUTEMP
#include "../usermods/mcu_temp/mcuTemp.h"
#endif

#if defined(WLED_USE_SD_MMC) || defined(WLED_USE_SD_SPI)
// This include of SD.h and SD_MMC.h must happen here, else they won't be
// resolved correctly (when included in mod's header only)
#ifdef WLED_USE_SD_MMC
#include "SD_MMC.h"
#elif defined(WLED_USE_SD_SPI)
#include "SD.h"
#include "SPI.h"
#endif
#include "../usermods/sd_card/usermod_sd_card.h"
#endif

#ifdef USERMOD_PWM_OUTPUTS
#include "../usermods/pwm_outputs/usermod_pwm_outputs.h"
#endif

#ifdef USERMOD_LDR_DUSK_DAWN
#include "../usermods/LDR_Dusk_Dawn_v2/usermod_LDR_Dusk_Dawn_v2.h"
#endif

//WLEDMM ARTIFX
#ifdef USERMOD_ARTIFX
#include "../usermods/artifx/usermod_v2_artifx.h"
#endif

#ifdef USERMOD_WEATHER
#include "../usermods/usermod_v2_weather/usermod_v2_weather.h"
#endif

#ifdef USERMOD_MPU6050_IMU
#include "../usermods/mpu6050_imu/usermod_mpu6050_imu.h"
#endif
#ifdef USERMOD_GAMES
#include "../usermods/usermod_v2_games/usermod_v2_games.h"
#endif
#ifdef USERMOD_ANIMARTRIX
#include "../usermods/usermod_v2_animartrix/usermod_v2_animartrix.h"
#endif

void registerUsermods()
{
  /*
   * Add your usermod class name here
   * || || ||
   * \/ \/ \/
   */
  //usermods.add(new MyExampleUsermod());
#ifdef USERMOD_BATTERY
  usermods.add(new UsermodBattery("Battery", false));  // WLEDMM
#endif

#ifdef USERMOD_DALLASTEMPERATURE
  usermods.add(new UsermodTemperature("Temperature", true));
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

#ifdef USERMOD_BH1750
  usermods.add(new Usermod_BH1750("BH1750", false));
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
  usermods.add(new RTCUsermod("RTC", false));  //WLEDMM
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

#ifdef USERMOD_ST7789_DISPLAY
  usermods.add(new St7789DisplayUsermod());
#endif

#ifdef USERMOD_SEVEN_SEGMENT
  usermods.add(new SevenSegmentDisplay());
#endif

#ifdef USERMOD_SSDR
  usermods.add(new UsermodSSDR());
#endif

#ifdef USERMOD_CRONIXIE
  usermods.add(new UsermodCronixie());
#endif

#ifdef QUINLED_AN_PENTA
  usermods.add(new QuinLEDAnPentaUsermod());
#endif

#ifdef USERMOD_WIZLIGHTS
  usermods.add(new WizLightsUsermod());
#endif

#ifdef USERMOD_WORDCLOCK
  usermods.add(new WordClockUsermod());
#endif

#ifdef USERMOD_MY9291
  usermods.add(new MY9291Usermod());
#endif

#ifdef USERMOD_SI7021_MQTT_HA
  usermods.add(new Si7021_MQTT_HA());
#endif

#ifdef USERMOD_SMARTNEST
  usermods.add(new Smartnest());
#endif

#ifdef USERMOD_AUDIOREACTIVE
  usermods.add(new AudioReactive());
#endif

#ifdef USERMOD_ANALOG_CLOCK
  usermods.add(new AnalogClockUsermod());
#endif

#ifdef USERMOD_PING_PONG_CLOCK
  usermods.add(new PingPongClockUsermod());
#endif

#ifdef USERMOD_ADS1115
  usermods.add(new ADS1115Usermod());
#endif

#ifdef USERMOD_KLIPPER_PERCENTAGE
  usermods.add(new klipper_percentage());
#endif

#ifdef USERMOD_BOBLIGHT
  usermods.add(new BobLightUsermod());
#endif

#ifdef SD_ADAPTER
  usermods.add(new UsermodSdCard());
#endif

#ifdef USERMOD_PWM_OUTPUTS
  usermods.add(new PwmOutputsUsermod());
#endif

#ifdef USERMOD_SHT
  usermods.add(new ShtUsermod("SHT-Sensor", false));
#endif

#ifdef USERMOD_MCUTEMP
  usermods.add(new mcuTemp("MCUTemp", false));
#endif

//#ifdef USERMOD_INTERNAL_TEMPERATURE
//  usermods.add(new InternalTemperatureUsermod());
//#endif

#ifdef USERMOD_LDR_DUSK_DAWN
  usermods.add(new LDR_Dusk_Dawn_v2());
#endif


// WLEDMM ARTIFX
#ifdef USERMOD_ARTIFX
  usermods.add(new ARTIFXUserMod());
#endif

#ifdef USERMOD_WEATHER
  usermods.add(new WeatherUsermod("Weather", true));
#endif


#ifdef USERMOD_MPU6050_IMU
  usermods.add(new MPU6050Driver("mpu6050-IMU", true));
#endif

#ifdef USERMOD_GAMES
  usermods.add(new GamesUsermod());
#endif
#ifdef USERMOD_ANIMARTRIX
  usermods.add(new AnimartrixUsermod("Animartrix", false));
#endif

}
