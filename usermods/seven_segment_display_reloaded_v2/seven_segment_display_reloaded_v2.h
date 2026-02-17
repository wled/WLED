#pragma once
#include "wled.h"

// logging macro:
#define _logUsermodSSDR(fmt, ...) \
	DEBUG_PRINTF("[SSDR] " fmt "\n", ##__VA_ARGS__)

#ifdef USERMOD_SN_PHOTORESISTOR
  #include "SN_Photoresistor.h"
#endif
#ifdef USERMOD_BH1750
  #include "BH1750_v2.h"
#endif

//#define REFRESHTIME 497

class UsermodSSDR : public Usermod {
  private:
    //Runtime variables.
    unsigned long umSSDRLastRefresh = 0;
    unsigned long umSSDRRefreshTime = 3000;
    uint16_t umSSDRLength = 0;
    
    // Configurable settings for the SSDR Usermod
    // Enabled usermod
    #ifndef umSSDR_ENABLED
      #define umSSDR_ENABLED false
    #endif

    #ifndef umSSDR_ENABLE_AUTO_BRIGHTNESS
      #define umSSDR_ENABLE_AUTO_BRIGHTNESS false
    #endif

    #ifndef umSSDR_BRIGHTNESS_MIN
      #define umSSDR_BRIGHTNESS_MIN 0
    #endif

    #ifndef umSSDR_BRIGHTNESS_MAX
      #define umSSDR_BRIGHTNESS_MAX 255
    #endif

    #ifndef umSSDR_INVERT_AUTO_BRIGHTNESS
      #define umSSDR_INVERT_AUTO_BRIGHTNESS false
    #endif

    #ifndef umSSDR_LUX_MIN
      #define umSSDR_LUX_MIN 0
    #endif

    #ifndef umSSDR_LUX_MAX
      #define umSSDR_LUX_MAX 1000
    #endif

    #ifndef umSSDR_INVERTED
      #define umSSDR_INVERTED false
    #endif

    #ifndef umSSDR_COLONBLINK
      #define umSSDR_COLONBLINK true
    #endif

    #ifndef umSSDR_LEADING_ZERO
      #define umSSDR_LEADING_ZERO false
    #endif

    // Display mask for physical layout
    /*//  H - 00-23 hours
    //  h - 01-12 hours
    //  k - 01-24 hours
    //  m - 00-59 minutes
    //  s - 00-59 seconds
    //  d - 01-31 day of month
    //  M - 01-12 month
    //  y - 21 last two positions of year
    //  Y - 2021 year
    //  L - Light LED
    //  This option defines a separate LED (or set of LEDs) that can be controlled independently 
    //  from the other clock display LEDs. It can be switched on or off to provide additional 
    //  lighting for the display or serve as an ambient light source, without affecting the 
    //  clock's visual display of time and date.
    //  : for a colon
    */

    #ifndef umSSDR_DISPLAY_MASK
      #define umSSDR_DISPLAY_MASK "H:m"
    #endif

    #ifndef umSSDR_HOURS
      #define umSSDR_HOURS ""
    #endif

    #ifndef umSSDR_MINUTES
      #define umSSDR_MINUTES ""
    #endif

    #ifndef umSSDR_SECONDS
      #define umSSDR_SECONDS ""
    #endif

    #ifndef umSSDR_COLONS
      #define umSSDR_COLONS ""
    #endif

    #ifndef umSSDR_LIGHT
      #define umSSDR_LIGHT ""
    #endif

    #ifndef umSSDR_DAYS
      #define umSSDR_DAYS ""
    #endif

    #ifndef umSSDR_MONTHS
      #define umSSDR_MONTHS ""
    #endif

    #ifndef umSSDR_YEARS
      #define umSSDR_YEARS ""
    #endif

    #ifndef umSSDR_NUMBERS
		/* Segment order, seen from the front:
		  <  A  >
		/\       /\
		F        B
		\/       \/
		  <  G  >
		/\       /\
		E        C
		\/       \/
		  <  D  >
		*/
      uint8_t umSSDRNumbers[11][7] = {
			//  A    B    C    D    E    F    G
			{   1,   1,   1,   1,   1,   1,   0 },  // 0
			{   0,   1,   1,   0,   0,   0,   0 },  // 1
			{   1,   1,   0,   1,   1,   0,   1 },  // 2
			{   1,   1,   1,   1,   0,   0,   1 },  // 3
			{   0,   1,   1,   0,   0,   1,   1 },  // 4
			{   1,   0,   1,   1,   0,   1,   1 },  // 5
			{   1,   0,   1,   1,   1,   1,   1 },  // 6
			{   1,   1,   1,   0,   0,   0,   0 },  // 7
			{   1,   1,   1,   1,   1,   1,   1 },  // 8
			{   1,   1,   1,   1,   0,   1,   1 },  // 9
			{   0,   0,   0,   0,   0,   0,   0 }   // blank
      };
    #else
      uint8_t umSSDRNumbers[11][10] = umSSDR_NUMBERS;
    #endif

    bool umSSDRDisplayTime = umSSDR_ENABLED;
    bool umSSDREnableLDR = umSSDR_ENABLE_AUTO_BRIGHTNESS;
    uint16_t umSSDRBrightnessMin = umSSDR_BRIGHTNESS_MIN;
    uint16_t umSSDRBrightnessMax = umSSDR_BRIGHTNESS_MAX;
    bool umSSDRInvertAutoBrightness = umSSDR_INVERT_AUTO_BRIGHTNESS;
    uint16_t umSSDRLuxMin = umSSDR_LUX_MIN;
    uint16_t umSSDRLuxMax = umSSDR_LUX_MAX;
    bool umSSDRInverted = umSSDR_INVERTED;
    bool umSSDRColonblink = umSSDR_COLONBLINK;
    bool umSSDRLeadingZero = umSSDR_LEADING_ZERO;
    String umSSDRDisplayMask = umSSDR_DISPLAY_MASK;
    String umSSDRHours = umSSDR_HOURS;
    String umSSDRMinutes = umSSDR_MINUTES;
    String umSSDRSeconds = umSSDR_SECONDS;
    String umSSDRColons = umSSDR_COLONS;
    String umSSDRLight = umSSDR_LIGHT;
    String umSSDRDays = umSSDR_DAYS;
    String umSSDRMonths = umSSDR_MONTHS;
    String umSSDRYears = umSSDR_YEARS;
    
    bool* umSSDRMask = nullptr;
    bool externalLedOutputDisabled = false;
    
    // Forward declarations of private methods
    void _overlaySevenSegmentDraw();
    void _setColons();
    void _showElements(String *map, int timevar, bool isColon, bool removeZero);
    void _setLeds(int lednr, int lastSeenLedNr, bool range, int countSegments, int number, bool colon);
    void _setMaskToLeds();
    void _setAllFalse();
    int _checkForNumber(int count, int index, String *map);
    void _publishMQTTint_P(const char *subTopic, int value);
    void _publishMQTTstr_P(const char *subTopic, String Value);

    template<typename T>
    bool _cmpIntSetting_P(char *topic, char *payload, const char *setting, T *value) {
	  char settingBuffer[30];
	  strlcpy(settingBuffer, setting, sizeof(settingBuffer));

	  _logUsermodSSDR("Checking topic='%s' payload='%s' against setting='%s'", topic, payload, settingBuffer);

	  if (strcmp(topic, settingBuffer) != 0) return false;

	  T oldValue = *value;
	  *value = static_cast<T>(strtol(payload, nullptr, 10));
	  _publishMQTTint_P(setting, static_cast<int>(*value));
	  _logUsermodSSDR("Setting '%s' updated from %d to %d", setting, static_cast<int>(oldValue), static_cast<int>(*value));
	  return true;
    }

    bool _handleSetting(char *topic, char *payload);
    void _updateMQTT();
    void _addJSONObject(JsonObject& root);
    
    // String constants
    static const char _str_name[];
    static const char _str_timeEnabled[];
    static const char _str_ldrEnabled[];
    static const char _str_minBrightness[];
    static const char _str_maxBrightness[];
    static const char _str_luxMin[];
    static const char _str_luxMax[];
    static const char _str_invertAutoBrightness[];
    static const char _str_inverted[];
    static const char _str_colonblink[];
    static const char _str_leadingZero[];
    static const char _str_displayMask[];
    static const char _str_hours[];
    static const char _str_minutes[];
    static const char _str_seconds[];
    static const char _str_colons[];
    static const char _str_light[];
    static const char _str_days[];
    static const char _str_months[];
    static const char _str_years[];
    
    // Optional usermod dependencies
    #ifdef USERMOD_SN_PHOTORESISTOR
      Usermod_SN_Photoresistor *photoResistor = nullptr;
    #else
      #define photoResistor nullptr
    #endif
    
    #ifdef USERMOD_BH1750
      Usermod_BH1750* bh1750 = nullptr;
    #else
      #define bh1750 nullptr
    #endif
    
  public:
    // Core usermod functions
    void setup() override;
    void loop() override;
    void handleOverlayDraw();
    void onStateChange(uint8_t mode) override;
    void addToJsonInfo(JsonObject& root) override;
    void addToJsonState(JsonObject& root) override;
    void readFromJsonState(JsonObject& root) override;
    void onMqttConnect(bool sessionPresent) override;
    bool onMqttMessage(char *topic, char *payload) override;
    void addToConfig(JsonObject &root) override;
    bool readFromConfig(JsonObject &root) override;
    uint16_t getId() override;
    
    // Public function that can be called from other usermods
    /**
    * Allows external usermods to temporarily disable the LED output of this usermod.
    * This is useful when multiple usermods might be trying to control the same LEDs.
    * @param state true to disable LED output, false to enable
    */
    void disableOutputFunction(bool state) {
	  externalLedOutputDisabled = state;
	  #ifdef DEBUG_PRINTF
	  	_logUsermodSSDR("disableOutputFunction was triggered by an external Usermod: %s", externalLedOutputDisabled ? "true" : "false");
	  #endif
	  // When being disabled, clear the mask immediately so nothing gets drawn
	  if (state && umSSDRMask != nullptr) {
		_setAllFalse();
		#ifdef DEBUG_PRINTF
			_logUsermodSSDR("Cleared mask due to external disable");
		#endif
	  }
    }
    
    // Destructor to clean up memory
    ~UsermodSSDR();
};
