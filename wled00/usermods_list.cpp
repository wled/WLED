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

#ifdef USERMOD_BH1750
  #include "../usermods/BH1750_v2/usermod_bh1750.h"
#endif

// BME280 v2 usermod. Define "USERMOD_BME280" in my_config.h
#ifdef USERMOD_BME280
  #include "../usermods/BME280_v2/usermod_bme280.h"
#endif

#ifdef USERMOD_BME68X
  #include "../usermods/BME68X_v2/usermod_bme68x.h"
#endif


#ifdef USERMOD_FOUR_LINE_DISPLAY
  #include "../usermods/usermod_v2_four_line_display_ALT/usermod_v2_four_line_display_ALT.h"
#endif

#ifdef USERMOD_ROTARY_ENCODER_UI
  #include "../usermods/usermod_v2_rotary_encoder_ui_ALT/usermod_v2_rotary_encoder_ui_ALT.h"
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

#ifdef USERMOD_PIXELS_DICE_TRAY
  #include "../usermods/pixels_dice_tray/pixels_dice_tray.h"
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

#ifdef USERMOD_WIREGUARD
  #include "../usermods/wireguard/wireguard.h"
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
  #include "../usermods/usermod_v2_klipper_percentage/usermod_v2_klipper_percentage.h"
#endif

#ifdef USERMOD_BOBLIGHT
  #include "../usermods/boblight/boblight.h"
#endif

#ifdef USERMOD_ANIMARTRIX
  #include "../usermods/usermod_v2_animartrix/usermod_v2_animartrix.h"
#endif

#ifdef USERMOD_INTERNAL_TEMPERATURE
  #include "../usermods/Internal_Temperature_v2/usermod_internal_temperature.h"
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

#ifdef USERMOD_HTTP_PULL_LIGHT_CONTROL
  #include "../usermods/usermod_v2_HttpPullLightControl/usermod_v2_HttpPullLightControl.h"
#endif

#ifdef USERMOD_MPU6050_IMU
  #include "../usermods/mpu6050_imu/usermod_mpu6050_imu.h"
#endif

#ifdef USERMOD_MPU6050_IMU
  #include "../usermods/mpu6050_imu/usermod_gyro_surge.h"
#endif

#ifdef USERMOD_LDR_DUSK_DAWN
  #include "../usermods/LDR_Dusk_Dawn_v2/usermod_LDR_Dusk_Dawn_v2.h"
#endif

#ifdef USERMOD_POV_DISPLAY
  #include "../usermods/pov_display/usermod_pov_display.h"
#endif

#ifdef USERMOD_STAIRCASE_WIPE
  #include "../usermods/stairway_wipe_basic/stairway-wipe-usermod-v2.h"
#endif

#ifdef USERMOD_MAX17048
  #include "../usermods/MAX17048_v2/usermod_max17048.h"
#endif

#ifdef USERMOD_TETRISAI
  #include "../usermods/TetrisAI_v2/usermod_v2_tetrisai.h"
#endif

#ifdef USERMOD_AHT10
  #include "../usermods/AHT10_v2/usermod_aht10.h"
#endif

#ifdef USERMOD_INA226
  #include "../usermods/INA226_v2/usermod_ina226.h"
#endif

#ifdef USERMOD_LD2410
#include "../usermods/LD2410_v2/usermod_ld2410.h"
#endif


#ifdef USERMOD_DEEP_SLEEP
  #include "../usermods/deep_sleep/usermod_deep_sleep.h"
#endif

void registerUsermods()
{
/*
   * Add your usermod class name here
   * || || ||
   * \/ \/ \/
   */
  //UsermodManager::add(new MyExampleUsermod());

  #ifdef USERMOD_BATTERY
  UsermodManager::add(new UsermodBattery());
  #endif

  #ifdef USERMOD_DALLASTEMPERATURE
  UsermodManager::add(new UsermodTemperature());
  #endif

  #ifdef USERMOD_SN_PHOTORESISTOR
  UsermodManager::add(new Usermod_SN_Photoresistor());
  #endif

  #ifdef USERMOD_PWM_FAN
  UsermodManager::add(new PWMFanUsermod());
  #endif

  #ifdef USERMOD_BUZZER
  UsermodManager::add(new BuzzerUsermod());
  #endif

  #ifdef USERMOD_BH1750
  UsermodManager::add(new Usermod_BH1750());
  #endif

  #ifdef USERMOD_BME280
  UsermodManager::add(new UsermodBME280());
  #endif

  #ifdef USERMOD_BME68X
  UsermodManager::add(new UsermodBME68X());
  #endif

  #ifdef USERMOD_SENSORSTOMQTT
  UsermodManager::add(new UserMod_SensorsToMQTT());
  #endif

  #ifdef USERMOD_PIRSWITCH
  UsermodManager::add(new PIRsensorSwitch());
  #endif

  #ifdef USERMOD_FOUR_LINE_DISPLAY
  UsermodManager::add(new FourLineDisplayUsermod());
  #endif

  #ifdef USERMOD_ROTARY_ENCODER_UI
  UsermodManager::add(new RotaryEncoderUIUsermod()); // can use USERMOD_FOUR_LINE_DISPLAY
  #endif

  #ifdef USERMOD_AUTO_SAVE
  UsermodManager::add(new AutoSaveUsermod());  // can use USERMOD_FOUR_LINE_DISPLAY
  #endif

  #ifdef USERMOD_DHT
  UsermodManager::add(new UsermodDHT());
  #endif

  #ifdef USERMOD_VL53L0X_GESTURES
  UsermodManager::add(new UsermodVL53L0XGestures());
  #endif

  #ifdef USERMOD_ANIMATED_STAIRCASE
  UsermodManager::add(new Animated_Staircase());
  #endif

  #ifdef USERMOD_MULTI_RELAY
  UsermodManager::add(new MultiRelay());
  #endif

  #ifdef USERMOD_RTC
  UsermodManager::add(new RTCUsermod());
  #endif

  #ifdef USERMOD_ELEKSTUBE_IPS
  UsermodManager::add(new ElekstubeIPSUsermod());
  #endif

  #ifdef USERMOD_ROTARY_ENCODER_BRIGHTNESS_COLOR
  UsermodManager::add(new RotaryEncoderBrightnessColor());
  #endif

  #ifdef RGB_ROTARY_ENCODER
  UsermodManager::add(new RgbRotaryEncoderUsermod());
  #endif

  #ifdef USERMOD_ST7789_DISPLAY
  UsermodManager::add(new St7789DisplayUsermod());
  #endif

  #ifdef USERMOD_PIXELS_DICE_TRAY
    UsermodManager::add(new PixelsDiceTrayUsermod());
  #endif

  #ifdef USERMOD_SEVEN_SEGMENT
  UsermodManager::add(new SevenSegmentDisplay());
  #endif

  #ifdef USERMOD_SSDR
  UsermodManager::add(new UsermodSSDR());
  #endif

  #ifdef USERMOD_CRONIXIE
  UsermodManager::add(new UsermodCronixie());
  #endif

  #ifdef QUINLED_AN_PENTA
  UsermodManager::add(new QuinLEDAnPentaUsermod());
  #endif

  #ifdef USERMOD_WIZLIGHTS
  UsermodManager::add(new WizLightsUsermod());
  #endif

  #ifdef USERMOD_WIREGUARD
  UsermodManager::add(new WireguardUsermod());
  #endif

  #ifdef USERMOD_WORDCLOCK
  UsermodManager::add(new WordClockUsermod());
  #endif

  #ifdef USERMOD_MY9291
  UsermodManager::add(new MY9291Usermod());
  #endif

  #ifdef USERMOD_SI7021_MQTT_HA
  UsermodManager::add(new Si7021_MQTT_HA());
  #endif

  #ifdef USERMOD_SMARTNEST
  UsermodManager::add(new Smartnest());
  #endif

  #ifdef USERMOD_AUDIOREACTIVE
  UsermodManager::add(new AudioReactive());
  #endif

  #ifdef USERMOD_ANALOG_CLOCK
  UsermodManager::add(new AnalogClockUsermod());
  #endif

  #ifdef USERMOD_PING_PONG_CLOCK
  UsermodManager::add(new PingPongClockUsermod());
  #endif

  #ifdef USERMOD_ADS1115
  UsermodManager::add(new ADS1115Usermod());
  #endif

  #ifdef USERMOD_KLIPPER_PERCENTAGE
  UsermodManager::add(new klipper_percentage());
  #endif

  #ifdef USERMOD_BOBLIGHT
  UsermodManager::add(new BobLightUsermod());
  #endif

  #ifdef SD_ADAPTER
  UsermodManager::add(new UsermodSdCard());
  #endif

  #ifdef USERMOD_PWM_OUTPUTS
  UsermodManager::add(new PwmOutputsUsermod());
  #endif

  #ifdef USERMOD_SHT
  UsermodManager::add(new ShtUsermod());
  #endif

  #ifdef USERMOD_ANIMARTRIX
  UsermodManager::add(new AnimartrixUsermod("Animartrix", false));
  #endif

  #ifdef USERMOD_INTERNAL_TEMPERATURE
  UsermodManager::add(new InternalTemperatureUsermod());
  #endif

  #ifdef USERMOD_HTTP_PULL_LIGHT_CONTROL
  UsermodManager::add(new HttpPullLightControl());
  #endif

  #ifdef USERMOD_MPU6050_IMU
  static MPU6050Driver mpu6050; UsermodManager::add(&mpu6050);
  #endif

  #ifdef USERMOD_GYRO_SURGE
  static GyroSurge gyro_surge; UsermodManager::add(&gyro_surge);
  #endif

  #ifdef USERMOD_LDR_DUSK_DAWN
  UsermodManager::add(new LDR_Dusk_Dawn_v2());
  #endif

  #ifdef USERMOD_STAIRCASE_WIPE
  UsermodManager::add(new StairwayWipeUsermod());
  #endif

  #ifdef USERMOD_MAX17048
  UsermodManager::add(new Usermod_MAX17048());
  #endif

  #ifdef USERMOD_TETRISAI
  UsermodManager::add(new TetrisAIUsermod());
  #endif

  #ifdef USERMOD_AHT10
  UsermodManager::add(new UsermodAHT10());
  #endif

  #ifdef USERMOD_INA226
  UsermodManager::add(new UsermodINA226());
  #endif
  
  #ifdef USERMOD_LD2410
  UsermodManager::add(new LD2410Usermod());
  #endif

  #ifdef USERMOD_POV_DISPLAY
  UsermodManager::add(new PovDisplayUsermod());
  #endif

  #ifdef USERMOD_DEEP_SLEEP
   usermods.add(new DeepSleepUsermod());
  #endif
}
