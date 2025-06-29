#pragma once
#ifndef WLED_CONST_H
#define WLED_CONST_H

/*
 * Readability defines and their associated numerical values + compile-time constants
 */

#define GRADIENT_PALETTE_COUNT 59

// You can define custom product info from build flags.
// This is useful to allow API consumer to identify what type of WLED version
// they are interacting with. Be aware that changing this might cause some third
// party API consumers to consider this as a non-WLED device since the values
// returned by the API and by MQTT will no longer be default. However, most
// third party only uses mDNS to validate, so this is generally fine to change.
// For example, Home Assistant will still work fine even with this value changed.
// Use like this:
// -D WLED_BRAND="\"Custom Brand\""
// -D WLED_PRODUCT_NAME="\"Custom Product\""
#ifndef WLED_BRAND
  #define WLED_BRAND "WLED"
#endif
#ifndef WLED_PRODUCT_NAME
  #define WLED_PRODUCT_NAME "FOSS"
#endif

//Defaults
#define DEFAULT_CLIENT_SSID "Your_Network"
#define DEFAULT_AP_SSID     WLED_BRAND "-AP"
#define DEFAULT_AP_PASS     "wled1234"
#define DEFAULT_OTA_PASS    "wledota"
#define DEFAULT_MDNS_NAME   "x"

//increase if you need more
#ifndef WLED_MAX_WIFI_COUNT
  #define WLED_MAX_WIFI_COUNT 3
#endif

#ifndef WLED_MAX_USERMODS
  #if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32S2)
    #define WLED_MAX_USERMODS 4
  #else
    #define WLED_MAX_USERMODS 6
  #endif
#endif

#ifdef ESP8266
  #define WLED_MAX_DIGITAL_CHANNELS 3
  #define WLED_MAX_ANALOG_CHANNELS 5
  #define WLED_MIN_VIRTUAL_BUSSES 3         // no longer used for bus creation but used to distinguish S2/S3 in UI
#else
  #if !defined(LEDC_CHANNEL_MAX) || !defined(LEDC_SPEED_MODE_MAX)
    #include "driver/ledc.h" // needed for analog/LEDC channel counts
  #endif
  #define WLED_MAX_ANALOG_CHANNELS (LEDC_CHANNEL_MAX*LEDC_SPEED_MODE_MAX)
  #if defined(CONFIG_IDF_TARGET_ESP32C3)    // 2 RMT, 6 LEDC, only has 1 I2S but NPB does not support it ATM
    #define WLED_MAX_DIGITAL_CHANNELS 2
    //#define WLED_MAX_ANALOG_CHANNELS 6
    #define WLED_MIN_VIRTUAL_BUSSES 4       // no longer used for bus creation but used to distinguish S2/S3 in UI
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)  // 4 RMT, 8 LEDC, only has 1 I2S bus, supported in NPB
    // the 5th bus (I2S) will prevent Audioreactive usermod from functioning (it is last used though)
    #define WLED_MAX_DIGITAL_CHANNELS 12    // x4 RMT + x1/x8 I2S0
    //#define WLED_MAX_ANALOG_CHANNELS 8
    #define WLED_MIN_VIRTUAL_BUSSES 4       // no longer used for bus creation but used to distinguish S2/S3 in UI
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)  // 4 RMT, 8 LEDC, has 2 I2S but NPB supports parallel x8 LCD on I2S1
    #define WLED_MAX_DIGITAL_CHANNELS 12    // x4 RMT + x8 I2S-LCD
    //#define WLED_MAX_ANALOG_CHANNELS 8
    #define WLED_MIN_VIRTUAL_BUSSES 6       // no longer used for bus creation but used to distinguish S2/S3 in UI
  #else
    // the last digital bus (I2S0) will prevent Audioreactive usermod from functioning
    #define WLED_MAX_DIGITAL_CHANNELS 16    // x1/x8 I2S1 + x8 RMT
    //#define WLED_MAX_ANALOG_CHANNELS 16
    #define WLED_MIN_VIRTUAL_BUSSES 6       // no longer used for bus creation but used to distinguish S2/S3 in UI
  #endif
#endif
// WLED_MAX_BUSSES was used to define the size of busses[] array which is no longer needed
// instead it will help determine max number of buses that can be defined at compile time
#ifdef WLED_MAX_BUSSES
  #undef WLED_MAX_BUSSES
#endif
#define WLED_MAX_BUSSES (WLED_MAX_DIGITAL_CHANNELS+WLED_MAX_ANALOG_CHANNELS)
static_assert(WLED_MAX_BUSSES <= 32, "WLED_MAX_BUSSES exceeds hard limit");

// Maximum number of pins per output. 5 for RGBCCT analog LEDs.
#define OUTPUT_MAX_PINS 5

// for pin manager
#ifdef ESP8266
#define WLED_NUM_PINS (GPIO_PIN_COUNT+1) // somehow they forgot GPIO 16 (0-16==17)
#else
#define WLED_NUM_PINS (GPIO_PIN_COUNT)
#endif

#ifndef WLED_MAX_BUTTONS
  #ifdef ESP8266
    #define WLED_MAX_BUTTONS 2
  #else
    #define WLED_MAX_BUTTONS 4
  #endif
#else
  #if WLED_MAX_BUTTONS < 2
    #undef WLED_MAX_BUTTONS
    #define WLED_MAX_BUTTONS 2
  #endif
#endif

#if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32S2)
#define WLED_MAX_COLOR_ORDER_MAPPINGS 5
#else
#define WLED_MAX_COLOR_ORDER_MAPPINGS 10
#endif

#if defined(WLED_MAX_LEDMAPS) && (WLED_MAX_LEDMAPS > 32 || WLED_MAX_LEDMAPS < 10)
  #undef WLED_MAX_LEDMAPS
#endif
#ifndef WLED_MAX_LEDMAPS
  #if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32S2)
    #define WLED_MAX_LEDMAPS 10
  #else
    #define WLED_MAX_LEDMAPS 16
  #endif
#endif

#ifndef WLED_MAX_SEGNAME_LEN
  #ifdef ESP8266
    #define WLED_MAX_SEGNAME_LEN 32
  #else
    #define WLED_MAX_SEGNAME_LEN 64
  #endif
#else
  #if WLED_MAX_SEGNAME_LEN<32
    #undef WLED_MAX_SEGNAME_LEN
    #define WLED_MAX_SEGNAME_LEN 32
  #else
    #warning WLED UI does not support modified maximum segment name length!
  #endif
#endif

#define WLED_MAX_PANELS 18                      // must not be more than 32

//Usermod IDs
#define USERMOD_ID_RESERVED               0     //Unused. Might indicate no usermod present
#define USERMOD_ID_UNSPECIFIED            1     //Default value for a general user mod that does not specify a custom ID
#define USERMOD_ID_EXAMPLE                2     //Usermod "usermod_v2_example.h"
#define USERMOD_ID_TEMPERATURE            3     //Usermod "usermod_temperature.h"
#define USERMOD_ID_FIXNETSERVICES         4     //Usermod "usermod_Fix_unreachable_netservices.h"
#define USERMOD_ID_PIRSWITCH              5     //Usermod "usermod_PIR_sensor_switch.h"
#define USERMOD_ID_IMU                    6     //Usermod "usermod_mpu6050_imu.h"
#define USERMOD_ID_FOUR_LINE_DISP         7     //Usermod "usermod_v2_four_line_display.h
#define USERMOD_ID_ROTARY_ENC_UI          8     //Usermod "usermod_v2_rotary_encoder_ui.h"
#define USERMOD_ID_AUTO_SAVE              9     //Usermod "usermod_v2_auto_save.h"
#define USERMOD_ID_DHT                   10     //Usermod "usermod_dht.h"
#define USERMOD_ID_MODE_SORT             11     //Usermod "usermod_v2_mode_sort.h"
#define USERMOD_ID_VL53L0X               12     //Usermod "usermod_vl53l0x_gestures.h"
#define USERMOD_ID_MULTI_RELAY           13     //Usermod "usermod_multi_relay.h"
#define USERMOD_ID_ANIMATED_STAIRCASE    14     //Usermod "Animated_Staircase.h"
#define USERMOD_ID_RTC                   15     //Usermod "usermod_rtc.h"
#define USERMOD_ID_ELEKSTUBE_IPS         16     //Usermod "usermod_elekstube_ips.h"
#define USERMOD_ID_SN_PHOTORESISTOR      17     //Usermod "usermod_sn_photoresistor.h"
#define USERMOD_ID_BATTERY               18     //Usermod "usermod_v2_battery.h"
#define USERMOD_ID_PWM_FAN               19     //Usermod "usermod_PWM_fan.h"
#define USERMOD_ID_BH1750                20     //Usermod "usermod_bh1750.h"
#define USERMOD_ID_SEVEN_SEGMENT_DISPLAY 21     //Usermod "usermod_v2_seven_segment_display.h"
#define USERMOD_RGB_ROTARY_ENCODER       22     //Usermod "rgb-rotary-encoder.h"
#define USERMOD_ID_QUINLED_AN_PENTA      23     //Usermod "quinled-an-penta.h"
#define USERMOD_ID_SSDR                  24     //Usermod "usermod_v2_seven_segment_display_reloaded.h"
#define USERMOD_ID_CRONIXIE              25     //Usermod "usermod_cronixie.h"
#define USERMOD_ID_WIZLIGHTS             26     //Usermod "wizlights.h"
#define USERMOD_ID_WORDCLOCK             27     //Usermod "usermod_v2_word_clock.h"
#define USERMOD_ID_MY9291                28     //Usermod "usermod_MY9291.h"
#define USERMOD_ID_SI7021_MQTT_HA        29     //Usermod "usermod_si7021_mqtt_ha.h"
#define USERMOD_ID_BME280                30     //Usermod "usermod_bme280.h
#define USERMOD_ID_SMARTNEST             31     //Usermod "usermod_smartnest.h"
#define USERMOD_ID_AUDIOREACTIVE         32     //Usermod "audioreactive.h"
#define USERMOD_ID_ANALOG_CLOCK          33     //Usermod "Analog_Clock.h"
#define USERMOD_ID_PING_PONG_CLOCK       34     //Usermod "usermod_v2_ping_pong_clock.h"
#define USERMOD_ID_ADS1115               35     //Usermod "usermod_ads1115.h"
#define USERMOD_ID_BOBLIGHT              36     //Usermod "boblight.h"
#define USERMOD_ID_SD_CARD               37     //Usermod "usermod_sd_card.h"
#define USERMOD_ID_PWM_OUTPUTS           38     //Usermod "usermod_pwm_outputs.h
#define USERMOD_ID_SHT                   39     //Usermod "usermod_sht.h
#define USERMOD_ID_KLIPPER               40     //Usermod Klipper percentage
#define USERMOD_ID_WIREGUARD             41     //Usermod "wireguard.h"
#define USERMOD_ID_INTERNAL_TEMPERATURE  42     //Usermod "usermod_internal_temperature.h"
#define USERMOD_ID_LDR_DUSK_DAWN         43     //Usermod "usermod_LDR_Dusk_Dawn_v2.h"
#define USERMOD_ID_STAIRWAY_WIPE         44     //Usermod "stairway-wipe-usermod-v2.h"
#define USERMOD_ID_ANIMARTRIX            45     //Usermod "usermod_v2_animartrix.h"
#define USERMOD_ID_HTTP_PULL_LIGHT_CONTROL 46   //usermod "usermod_v2_HttpPullLightControl.h"
#define USERMOD_ID_TETRISAI              47     //Usermod "usermod_v2_tetris.h"
#define USERMOD_ID_MAX17048              48     //Usermod "usermod_max17048.h"
#define USERMOD_ID_BME68X                49     //Usermod "usermod_bme68x.h
#define USERMOD_ID_INA226                50     //Usermod "usermod_ina226.h"
#define USERMOD_ID_AHT10                 51     //Usermod "usermod_aht10.h"
#define USERMOD_ID_LD2410                52     //Usermod "usermod_ld2410.h"
#define USERMOD_ID_POV_DISPLAY           53     //Usermod "usermod_pov_display.h"
#define USERMOD_ID_PIXELS_DICE_TRAY      54     //Usermod "pixels_dice_tray.h"
#define USERMOD_ID_DEEP_SLEEP            55     //Usermod "usermod_deep_sleep.h"
#define USERMOD_ID_RF433                 56     //Usermod "usermod_v2_RF433.h"
#define USERMOD_ID_BRIGHTNESS_FOLLOW_SUN 57     //Usermod "usermod_v2_brightness_follow_sun.h"
#define USERMOD_ID_USER_FX               58     //Usermod "user_fx"

//Access point behavior
#define AP_BEHAVIOR_BOOT_NO_CONN          0     //Open AP when no connection after boot
#define AP_BEHAVIOR_NO_CONN               1     //Open when no connection (either after boot or if connection is lost)
#define AP_BEHAVIOR_ALWAYS                2     //Always open
#define AP_BEHAVIOR_BUTTON_ONLY           3     //Only when button pressed for 6 sec
#define AP_BEHAVIOR_TEMPORARY             4     //Open AP when no connection after boot but only temporary
#ifndef WLED_AP_TIMEOUT
  #define WLED_AP_TIMEOUT            300000     //Temporary AP timeout
#endif

//Notifier callMode
#define CALL_MODE_INIT           0     //no updates on init, can be used to disable updates
#define CALL_MODE_DIRECT_CHANGE  1
#define CALL_MODE_BUTTON         2     //default button actions applied to selected segments
#define CALL_MODE_NOTIFICATION   3     //caused by incoming notification (UDP or DMX preset)
#define CALL_MODE_NIGHTLIGHT     4     //nightlight progress
#define CALL_MODE_NO_NOTIFY      5     //change state but do not send notifications (UDP)
#define CALL_MODE_FX_CHANGED     6     //no longer used
#define CALL_MODE_HUE            7
#define CALL_MODE_PRESET_CYCLE   8     //no longer used
#define CALL_MODE_BLYNK          9     //no longer used
#define CALL_MODE_ALEXA         10
#define CALL_MODE_WS_SEND       11     //special call mode, not for notifier, updates websocket only
#define CALL_MODE_BUTTON_PRESET 12     //button/IR JSON preset/macro

//RGB to RGBW conversion mode
#define RGBW_MODE_MANUAL_ONLY     0    // No automatic white channel calculation. Manual white channel slider
#define RGBW_MODE_AUTO_BRIGHTER   1    // New algorithm. Adds as much white as the darkest RGBW channel
#define RGBW_MODE_AUTO_ACCURATE   2    // New algorithm. Adds as much white as the darkest RGBW channel and subtracts this amount from each RGB channel
#define RGBW_MODE_DUAL            3    // Manual slider + auto calculation. Automatically calculates only if manual slider is set to off (0)
#define RGBW_MODE_MAX             4    // Sets white to the value of the brightest RGB channel (good for white-only LEDs without any RGB)
//#define RGBW_MODE_LEGACY        4    // Old floating algorithm. Too slow for realtime and palette support (unused)
#define AW_GLOBAL_DISABLED      255    // Global auto white mode override disabled. Per-bus setting is used

//realtime modes
#define REALTIME_MODE_INACTIVE    0
#define REALTIME_MODE_GENERIC     1
#define REALTIME_MODE_UDP         2
#define REALTIME_MODE_HYPERION    3
#define REALTIME_MODE_E131        4
#define REALTIME_MODE_ADALIGHT    5
#define REALTIME_MODE_ARTNET      6
#define REALTIME_MODE_TPM2NET     7
#define REALTIME_MODE_DDP         8
#define REALTIME_MODE_DMX         9

//realtime override modes
#define REALTIME_OVERRIDE_NONE    0
#define REALTIME_OVERRIDE_ONCE    1
#define REALTIME_OVERRIDE_ALWAYS  2

//E1.31 DMX modes
#define DMX_MODE_DISABLED         0            //not used
#define DMX_MODE_SINGLE_RGB       1            //all LEDs same RGB color (3 channels)
#define DMX_MODE_SINGLE_DRGB      2            //all LEDs same RGB color and master dimmer (4 channels)
#define DMX_MODE_EFFECT           3            //trigger standalone effects of WLED (15 channels)
#define DMX_MODE_EFFECT_W         7            //trigger standalone effects of WLED (18 channels)
#define DMX_MODE_MULTIPLE_RGB     4            //every LED is addressed with its own RGB (ledCount * 3 channels)
#define DMX_MODE_MULTIPLE_DRGB    5            //every LED is addressed with its own RGB and share a master dimmer (ledCount * 3 + 1 channels)
#define DMX_MODE_MULTIPLE_RGBW    6            //every LED is addressed with its own RGBW (ledCount * 4 channels)
#define DMX_MODE_EFFECT_SEGMENT   8            //trigger standalone effects of WLED (15 channels per segment)
#define DMX_MODE_EFFECT_SEGMENT_W 9            //trigger standalone effects of WLED (18 channels per segment)
#define DMX_MODE_PRESET           10           //apply presets (1 channel)

//Light capability byte (unused) 0bRCCCTTTT
//bits 0/1/2/3: specifies a type of LED driver. A single "driver" may have different chip models but must have the same protocol/behavior
//bits 4/5/6: specifies the class of LED driver - 0b000 (dec. 0-15)  unconfigured/reserved
//                                              - 0b001 (dec. 16-31) digital (data pin only)
//                                              - 0b010 (dec. 32-47) analog (PWM)
//                                              - 0b011 (dec. 48-63) digital (data + clock / SPI)
//                                              - 0b100 (dec. 64-79) unused/reserved
//                                              - 0b101 (dec. 80-95) virtual network busses
//                                              - 0b110 (dec. 96-111) unused/reserved
//                                              - 0b111 (dec. 112-127) unused/reserved
//bit 7 is reserved and set to 0

#define TYPE_NONE                 0            //light is not configured
#define TYPE_RESERVED             1            //unused. Might indicate a "virtual" light
//Digital types (data pin only) (16-39)
#define TYPE_DIGITAL_MIN         16            // first usable digital type
#define TYPE_WS2812_1CH          18            //white-only chips (1 channel per IC) (unused)
#define TYPE_WS2812_1CH_X3       19            //white-only chips (3 channels per IC)
#define TYPE_WS2812_2CH_X3       20            //CCT chips (1st IC controls WW + CW of 1st zone and CW of 2nd zone, 2nd IC controls WW of 2nd zone and WW + CW of 3rd zone)
#define TYPE_WS2812_WWA          21            //amber + warm + cold white
#define TYPE_WS2812_RGB          22
#define TYPE_GS8608              23            //same driver as WS2812, but will require signal 2x per second (else displays test pattern)
#define TYPE_WS2811_400KHZ       24            //half-speed WS2812 protocol, used by very old WS2811 units
#define TYPE_TM1829              25
#define TYPE_UCS8903             26
#define TYPE_APA106              27
#define TYPE_FW1906              28            //RGB + CW + WW + unused channel (6 channels per IC)
#define TYPE_UCS8904             29            //first RGBW digital type (hardcoded in busmanager.cpp, memUsage())
#define TYPE_SK6812_RGBW         30
#define TYPE_TM1814              31
#define TYPE_WS2805              32            //RGB + WW + CW
#define TYPE_TM1914              33            //RGB
#define TYPE_SM16825             34            //RGB + WW + CW
#define TYPE_DIGITAL_MAX         39            // last usable digital type
//"Analog" types (40-47)
#define TYPE_ONOFF               40            //binary output (relays etc.; NOT PWM)
#define TYPE_ANALOG_MIN          41            // first usable analog type
#define TYPE_ANALOG_1CH          41            //single channel PWM. Uses value of brightest RGBW channel
#define TYPE_ANALOG_2CH          42            //analog WW + CW
#define TYPE_ANALOG_3CH          43            //analog RGB
#define TYPE_ANALOG_4CH          44            //analog RGBW
#define TYPE_ANALOG_5CH          45            //analog RGB + WW + CW
#define TYPE_ANALOG_6CH          46            //analog RGB + A + WW + CW
#define TYPE_ANALOG_MAX          47            // last usable analog type
//Digital types (data + clock / SPI) (48-63)
#define TYPE_2PIN_MIN            48
#define TYPE_WS2801              50
#define TYPE_APA102              51
#define TYPE_LPD8806             52
#define TYPE_P9813               53
#define TYPE_LPD6803             54
#define TYPE_2PIN_MAX            63
//Network types (master broadcast) (80-95)
#define TYPE_VIRTUAL_MIN         80
#define TYPE_NET_DDP_RGB         80            //network DDP RGB bus (master broadcast bus)
#define TYPE_NET_E131_RGB        81            //network E131 RGB bus (master broadcast bus, unused)
#define TYPE_NET_ARTNET_RGB      82            //network ArtNet RGB bus (master broadcast bus, unused)
#define TYPE_NET_DDP_RGBW        88            //network DDP RGBW bus (master broadcast bus)
#define TYPE_NET_ARTNET_RGBW     89            //network ArtNet RGB bus (master broadcast bus, unused)
#define TYPE_VIRTUAL_MAX         95

//Color orders
#define COL_ORDER_GRB             0           //GRB(w),defaut
#define COL_ORDER_RGB             1           //common for WS2811
#define COL_ORDER_BRG             2
#define COL_ORDER_RBG             3
#define COL_ORDER_BGR             4
#define COL_ORDER_GBR             5
#define COL_ORDER_MAX             5

//ESP-NOW
#define ESP_NOW_STATE_UNINIT       0
#define ESP_NOW_STATE_ON           1
#define ESP_NOW_STATE_ERROR        2

//Button type
#define BTN_TYPE_NONE             0
#define BTN_TYPE_RESERVED         1
#define BTN_TYPE_PUSH             2
#define BTN_TYPE_PUSH_ACT_HIGH    3
#define BTN_TYPE_SWITCH           4
#define BTN_TYPE_PIR_SENSOR       5
#define BTN_TYPE_TOUCH            6
#define BTN_TYPE_ANALOG           7
#define BTN_TYPE_ANALOG_INVERTED  8
#define BTN_TYPE_TOUCH_SWITCH     9

//Ethernet board types
#define WLED_NUM_ETH_TYPES        13

#define WLED_ETH_NONE              0
#define WLED_ETH_WT32_ETH01        1
#define WLED_ETH_ESP32_POE         2
#define WLED_ETH_WESP32            3
#define WLED_ETH_QUINLED           4
#define WLED_ETH_TWILIGHTLORD      5
#define WLED_ETH_ESP32DEUX         6
#define WLED_ETH_ESP32ETHKITVE     7
#define WLED_ETH_QUINLED_OCTA      8
#define WLED_ETH_ABCWLEDV43ETH     9
#define WLED_ETH_SERG74           10
#define WLED_ETH_ESP32_POE_WROVER 11
#define WLED_ETH_LILYGO_T_POE_PRO 12

//Hue error codes
#define HUE_ERROR_INACTIVE        0
#define HUE_ERROR_UNAUTHORIZED    1
#define HUE_ERROR_LIGHTID         3
#define HUE_ERROR_PUSHLINK      101
#define HUE_ERROR_JSON_PARSING  250
#define HUE_ERROR_TIMEOUT       251
#define HUE_ERROR_ACTIVE        255

//Segment option byte bits
#define SEG_OPTION_SELECTED       0
#define SEG_OPTION_REVERSED       1
#define SEG_OPTION_ON             2
#define SEG_OPTION_MIRROR         3            //Indicates that the effect will be mirrored within the segment
#define SEG_OPTION_FREEZE         4            //Segment contents will not be refreshed
#define SEG_OPTION_RESET          5            //Segment runtime requires reset
#define SEG_OPTION_REVERSED_Y     6
#define SEG_OPTION_MIRROR_Y       7
#define SEG_OPTION_TRANSPOSED     8

//Segment differs return byte
#define SEG_DIFFERS_BRI        0x01 // opacity
#define SEG_DIFFERS_OPT        0x02 // all segment options except: selected, reset & transitional
#define SEG_DIFFERS_COL        0x04 // colors
#define SEG_DIFFERS_FX         0x08 // effect/mode parameters
#define SEG_DIFFERS_BOUNDS     0x10 // segment start/stop bounds
#define SEG_DIFFERS_GSO        0x20 // grouping, spacing & offset
#define SEG_DIFFERS_SEL        0x80 // selected

//Playlist option byte
#define PL_OPTION_SHUFFLE      0x01
#define PL_OPTION_RESTORE      0x02

// Segment capability byte
#define SEG_CAPABILITY_RGB     0x01
#define SEG_CAPABILITY_W       0x02
#define SEG_CAPABILITY_CCT     0x04

// WLED Error modes
#define ERR_NONE         0  // All good :)
#define ERR_DENIED       1  // Permission denied
#define ERR_CONCURRENCY  2  // Conurrency (client active)
#define ERR_NOBUF        3  // JSON buffer was not released in time, request cannot be handled at this time
#define ERR_NOT_IMPL     4  // Not implemented
#define ERR_NORAM_PX     7  // not enough RAM for pixels
#define ERR_NORAM        8  // effect RAM depleted
#define ERR_JSON         9  // JSON parsing failed (input too large?)
#define ERR_FS_BEGIN    10  // Could not init filesystem (no partition?)
#define ERR_FS_QUOTA    11  // The FS is full or the maximum file size is reached
#define ERR_FS_PLOAD    12  // It was attempted to load a preset that does not exist
#define ERR_FS_IRLOAD   13  // It was attempted to load an IR JSON cmd, but the "ir.json" file does not exist
#define ERR_FS_RMLOAD   14  // It was attempted to load an remote JSON cmd, but the "remote.json" file does not exist
#define ERR_FS_GENERAL  19  // A general unspecified filesystem error occurred
#define ERR_OVERTEMP    30  // An attached temperature sensor has measured above threshold temperature (not implemented)
#define ERR_OVERCURRENT 31  // An attached current sensor has measured a current above the threshold (not implemented)
#define ERR_UNDERVOLT   32  // An attached voltmeter has measured a voltage below the threshold (not implemented)

// Timer mode types
#define NL_MODE_SET               0            //After nightlight time elapsed, set to target brightness
#define NL_MODE_FADE              1            //Fade to target brightness gradually
#define NL_MODE_COLORFADE         2            //Fade to target brightness and secondary color gradually
#define NL_MODE_SUN               3            //Sunrise/sunset. Target brightness is set immediately, then Sunrise effect is started. Max 60 min.

// Settings sub page IDs
#define SUBPAGE_MENU              0
#define SUBPAGE_WIFI              1
#define SUBPAGE_LEDS              2
#define SUBPAGE_UI                3
#define SUBPAGE_SYNC              4
#define SUBPAGE_TIME              5
#define SUBPAGE_SEC               6
#define SUBPAGE_DMX               7
#define SUBPAGE_UM                8
#define SUBPAGE_UPDATE            9
#define SUBPAGE_2D               10
#define SUBPAGE_LOCK            251
#define SUBPAGE_PINREQ          252
#define SUBPAGE_CSS             253
#define SUBPAGE_JS              254
#define SUBPAGE_WELCOME         255

#define NTP_PACKET_SIZE 48       // size of NTP receive buffer
#define NTP_MIN_PACKET_SIZE 48   // min expected size - NTP v4 allows for "extended information" appended to the standard fields

//maximum number of rendered LEDs - this does not have to match max. physical LEDs, e.g. if there are virtual busses
#ifndef MAX_LEDS
  #ifdef ESP8266
    #define MAX_LEDS 1536 //can't rely on memory limit to limit this to 1536 LEDs
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    #define MAX_LEDS 2048 //due to memory constraints S2
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    #define MAX_LEDS 4096
  #else
    #define MAX_LEDS 16384
  #endif
#endif

#ifndef MAX_LED_MEMORY
  #ifdef ESP8266
    #define MAX_LED_MEMORY 4096
  #else
    #if defined(ARDUINO_ARCH_ESP32S2)
      #define MAX_LED_MEMORY 16384
    #elif defined(ARDUINO_ARCH_ESP32C3)
      #define MAX_LED_MEMORY 32768
    #else
      #define MAX_LED_MEMORY 65536
    #endif
  #endif
#endif

#ifndef MAX_LEDS_PER_BUS
#define MAX_LEDS_PER_BUS 2048   // may not be enough for fast LEDs (i.e. APA102)
#endif

// string temp buffer (now stored in stack locally)
#ifdef ESP8266
#define SETTINGS_STACK_BUF_SIZE 2560
#else
#define SETTINGS_STACK_BUF_SIZE 3840  // warning: quite a large value for stack (640 * WLED_MAX_USERMODS)
#endif

#ifdef WLED_USE_ETHERNET
  #define E131_MAX_UNIVERSE_COUNT 20
#else
  #ifdef ESP8266
    #define E131_MAX_UNIVERSE_COUNT 9
  #else
    #define E131_MAX_UNIVERSE_COUNT 12
  #endif
#endif

#ifndef ABL_MILLIAMPS_DEFAULT
  #define ABL_MILLIAMPS_DEFAULT 850   // auto lower brightness to stay close to milliampere limit
#else
  #if ABL_MILLIAMPS_DEFAULT == 0      // disable ABL
  #elif ABL_MILLIAMPS_DEFAULT < 250   // make sure value is at least 250
   #warning "make sure value is at least 250"
   #define ABL_MILLIAMPS_DEFAULT 250
  #endif
#endif

#ifndef LED_MILLIAMPS_DEFAULT
  #define LED_MILLIAMPS_DEFAULT 55    // common WS2812B
#else
  #if LED_MILLIAMPS_DEFAULT < 1 || LED_MILLIAMPS_DEFAULT > 100
   #warning "Unusual LED mA current, overriding with default value."
   #undef LED_MILLIAMPS_DEFAULT
   #define LED_MILLIAMPS_DEFAULT 55
  #endif
#endif

// PWM settings
#ifndef WLED_PWM_FREQ
#ifdef ESP8266
  #define WLED_PWM_FREQ    880 //PWM frequency proven as good for LEDs
#else
  #ifdef SOC_LEDC_SUPPORT_XTAL_CLOCK
    #define WLED_PWM_FREQ 9765    // XTAL clock is 40MHz (this will allow 12 bit resolution)
  #else
    #define WLED_PWM_FREQ  19531  // APB clock is 80MHz
  #endif
#endif
#endif

#define TOUCH_THRESHOLD 32 // limit to recognize a touch, higher value means more sensitive

// Size of buffer for API JSON object (increase for more segments)
#ifdef ESP8266
  #define JSON_BUFFER_SIZE 10240
#else
  #if defined(ARDUINO_ARCH_ESP32S2)
    #define JSON_BUFFER_SIZE 24576
  #else
    #define JSON_BUFFER_SIZE 32767
  #endif
#endif

//#define MIN_HEAP_SIZE
#define MIN_HEAP_SIZE 2048

// Web server limits
#ifdef ESP8266
// Minimum heap to consider handling a request
#define WLED_REQUEST_MIN_HEAP (8*1024)
// Estimated maximum heap required by any one request
#define WLED_REQUEST_HEAP_USAGE (6*1024)
#else
// ESP32 TCP stack needs much more RAM than ESP8266
// Minimum heap remaining before queuing a request
#define WLED_REQUEST_MIN_HEAP (12*1024)
// Estimated maximum heap required by any one request
#define WLED_REQUEST_HEAP_USAGE (12*1024)
#endif
// Maximum number of requests in queue; absolute cap on web server resource usage.
// Websockets do not count against this limit.
#define WLED_REQUEST_MAX_QUEUE 6

// Maximum size of node map (list of other WLED instances)
#ifdef ESP8266
  #define WLED_MAX_NODES 24
#else
  #define WLED_MAX_NODES 150
#endif

// Defaults pins, type and counts to configure LED output
#if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3)
  #ifdef WLED_ENABLE_DMX
    #define DEFAULT_LED_PIN 1
    #warning "Compiling with DMX. The default LED pin has been changed to pin 1."
  #else
    #define DEFAULT_LED_PIN 2    // GPIO2 (D4) on Wemos D1 mini compatible boards, safe to use on any board
  #endif
#else
  #define DEFAULT_LED_PIN 16   // aligns with GPIO2 (D4) on Wemos D1 mini32 compatible boards (if it is unusable it will be reassigned in WS2812FX::finalizeInit())
#endif
#define DEFAULT_LED_TYPE TYPE_WS2812_RGB
#define DEFAULT_LED_COUNT 30

#define INTERFACE_UPDATE_COOLDOWN 1000 // time in ms to wait between websockets, alexa, and MQTT updates

#define PIN_RETRY_COOLDOWN   3000 // time in ms after an incorrect attempt PIN and OTA pass will be rejected even if correct
#define PIN_TIMEOUT        900000 // time in ms after which the PIN will be required again, 15 minutes

// HW_PIN_SCL & HW_PIN_SDA are used for information in usermods settings page and usermods themselves
// which GPIO pins are actually used in a hardware layout (controller board)
#if defined(I2CSCLPIN) && !defined(HW_PIN_SCL)
  #define HW_PIN_SCL I2CSCLPIN
#endif
#if defined(I2CSDAPIN) && !defined(HW_PIN_SDA)
  #define HW_PIN_SDA I2CSDAPIN
#endif
// you cannot change HW I2C pins on 8266
#if defined(ESP8266) && defined(HW_PIN_SCL)
  #undef HW_PIN_SCL
#endif
#if defined(ESP8266) && defined(HW_PIN_SDA)
  #undef HW_PIN_SDA
#endif
// defaults for 1st I2C on ESP32 (Wire global)
#ifndef HW_PIN_SCL
  #define HW_PIN_SCL SCL
#endif
#ifndef HW_PIN_SDA
  #define HW_PIN_SDA SDA
#endif

// HW_PIN_SCLKSPI & HW_PIN_MOSISPI & HW_PIN_MISOSPI are used for information in usermods settings page and usermods themselves
// which GPIO pins are actually used in a hardware layout (controller board)
#if defined(SPISCLKPIN) && !defined(HW_PIN_CLOCKSPI)
  #define HW_PIN_CLOCKSPI SPISCLKPIN
#endif
#if defined(SPIMOSIPIN) && !defined(HW_PIN_MOSISPI)
  #define HW_PIN_MOSISPI SPIMOSIPIN
#endif
#if defined(SPIMISOPIN) && !defined(HW_PIN_MISOSPI)
  #define HW_PIN_MISOSPI SPIMISOPIN
#endif
// you cannot change HW SPI pins on 8266
#if defined(ESP8266) && defined(HW_PIN_CLOCKSPI)
  #undef HW_PIN_CLOCKSPI
#endif
#if defined(ESP8266) && defined(HW_PIN_DATASPI)
  #undef HW_PIN_DATASPI
#endif
#if defined(ESP8266) && defined(HW_PIN_MISOSPI)
  #undef HW_PIN_MISOSPI
#endif
// defaults for VSPI on ESP32 (SPI global, SPI.cpp) as HSPI is used by WLED (bus_wrapper.h)
#ifndef HW_PIN_CLOCKSPI
  #define HW_PIN_CLOCKSPI SCK
#endif
#ifndef HW_PIN_DATASPI
  #define HW_PIN_DATASPI MOSI
#endif
#ifndef HW_PIN_MISOSPI
  #define HW_PIN_MISOSPI MISO
#endif

// IRAM_ATTR for 8266 with 32Kb IRAM causes error: section `.text1' will not fit in region `iram1_0_seg'
// this hack removes the IRAM flag for some 1D/2D functions - somewhat slower, but it solves problems with some older 8266 chips
#ifdef WLED_SAVE_IRAM
  #define IRAM_ATTR_YN
#else
  #define IRAM_ATTR_YN IRAM_ATTR
#endif

#endif
