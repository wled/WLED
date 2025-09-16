#include "ESPRotary.h"
#include <math.h>
#include "wled.h"

class RgbRotaryEncoderUsermod : public Usermod
{
  private:
    bool enabled = false;
    bool initDone = false;
    bool isDirty = false;
    BusDigital *ledBus;
    /*
    * Green - eb - Q4 - 32
    * Red   - ea - Q1 - 15
    * Black - sw - Q2 - 12
    */
    ESPRotary *rotaryEncoder;
    int8_t ledIo = 3; // GPIO to control the LEDs
    int8_t eaIo = 15; // "ea" from RGB Encoder Board
    int8_t ebIo = 32; // "eb" from RGB Encoder Board
    byte stepsPerClick = 4; // How many "steps" your rotary encoder does per click. This varies per rotary encoder
    /* This could vary per rotary encoder: Usually rotary encoders have 20 "clicks".
      If yours has less/more, adjust this to: 100% = 20 LEDs * incrementPerClick */
    byte incrementPerClick = 5;
    byte ledMode = 3;
    byte ledBrightness = 64;

    // This is all needed to calculate the brightness, rotary position, etc.
    const byte minPos = 5; // minPos is not zero, because if we want to turn the LEDs off, we use the built-in button ;)
    const byte maxPos = 100; // maxPos=100, like 100%
    const byte numLeds = 20;
    byte lastKnownPos = 0;

    byte currentColors[3];
    byte lastKnownBri = 0;


    void initRotaryEncoder()
    {
      PinManagerPinType pins[2] = { { eaIo, false }, { ebIo, false } };
      if (!PinManager::allocateMultiplePins(pins, 2, PinOwner::UM_RGBRotaryEncoder)) {
        eaIo = -1;
        ebIo = -1;
        cleanup();
        return;
      }

      // I don't know why, but setting the upper bound here does not work. It results into 1717922932 O_o
      rotaryEncoder = new ESPRotary(eaIo, ebIo, stepsPerClick, incrementPerClick, maxPos, currentPos, incrementPerClick);
      rotaryEncoder->setUpperBound(maxPos); // I have to again set it here and then it works / is actually 100...

      rotaryEncoder->setChangedHandler(RgbRotaryEncoderUsermod::cbRotate);
    }

    void initLedBus()
    {
      byte _pins[5] = {(byte)ledIo, 255, 255, 255, 255};
      BusConfig busCfg = BusConfig(TYPE_WS2812_RGB, _pins, 0, numLeds, COL_ORDER_GRB, false, 0);

      ledBus = new BusDigital(busCfg, WLED_MAX_BUSSES - 1);
      if (!ledBus->isOk()) {
        cleanup();
        return;
      }

      ledBus->setBrightness(ledBrightness);
    }

    void updateLeds()
    {
      switch (ledMode) {
        case 2:
          {
            currentColors[0] = 255; currentColors[1] = 0; currentColors[2] = 0;
            for (int i = 0; i < currentPos / incrementPerClick - 1; i++) {
              ledBus->setPixelColor(i, 0);
            }
            ledBus->setPixelColor(currentPos / incrementPerClick - 1, colorFromRgbw(currentColors));
            for (int i = currentPos / incrementPerClick; i < numLeds; i++) {
              ledBus->setPixelColor(i, 0);
            }
          }
          break;

        default:
        case 1:
        case 3:
          // WLED orange (of course), which we will use in mode 1
          currentColors[0] = 255; currentColors[1] = 160; currentColors[2] = 0;
          for (int i = 0; i < currentPos / incrementPerClick; i++) {
            if (ledMode == 3) {
              hsv2rgb((i) / float(numLeds), 1, .25);
            }
            ledBus->setPixelColor(i, colorFromRgbw(currentColors));
          }
          for (int i = currentPos / incrementPerClick; i < numLeds; i++) {
            ledBus->setPixelColor(i, 0);
          }
          break;
      }

      isDirty = true;
    }

    void cleanup()
    {
      // Only deallocate pins if we allocated them ;)
      if (eaIo != -1) {
        PinManager::deallocatePin(eaIo, PinOwner::UM_RGBRotaryEncoder);
        eaIo = -1;
      }
      if (ebIo != -1) {
        PinManager::deallocatePin(ebIo, PinOwner::UM_RGBRotaryEncoder);
        ebIo = -1;
      }

      delete rotaryEncoder;
      delete ledBus;

      enabled = false;
    }

    int getPositionForBrightness()
    {
      return int(((float)bri / (float)255) * 100);
    }

    float fract(float x) { return x - int(x); }

    float mix(float a, float b, float t) { return a + (b - a) * t; }

    void hsv2rgb(float h, float s, float v) {
      currentColors[0] = int((v * mix(1.0, constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s)) * 255);
      currentColors[1] = int((v * mix(1.0, constrain(abs(fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s)) * 255);
      currentColors[2] = int((v * mix(1.0, constrain(abs(fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s)) * 255);
    }

  public:
    static byte currentPos;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _ledIo[];
    static const char _eaIo[];
    static const char _ebIo[];
    static const char _ledMode[];
    static const char _ledBrightness[];
    static const char _stepsPerClick[];
    static const char _incrementPerClick[];


    static void cbRotate(ESPRotary& r) {
      currentPos = r.getPosition();
    }

    /**
     * Enable/Disable the usermod
     */
    // inline void enable(bool enable) { enabled = enable; }
    /**
     * Get usermod enabled/disabled state
     */
    // inline bool isEnabled() { return enabled; }

    /*
      * setup() is called once at boot. WiFi is not yet connected at this point.
      * You can use it to initialize variables, sensors or similar.
      */
    void setup()
    {
      if (enabled) {
        currentPos = getPositionForBrightness();
        lastKnownBri = bri;

        initRotaryEncoder();
        initLedBus();

        // No updating of LEDs here, as that's sometimes not working; loop() will take care of that

        initDone = true;
      }
    }

    /*
      * loop() is called continuously. Here you can check for events, read sensors, etc.
      * 
      * Tips:
      * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
      *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
      * 
      * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
      *    Instead, use a timer check as shown here.
      */
    void loop()
    {
      if (!enabled || strip.isUpdating()) return;

      rotaryEncoder->loop();

      // If the rotary was changed
      if(lastKnownPos != currentPos) {
        lastKnownPos = currentPos;

        bri = min(int(round((2.55 * currentPos))), 255);
        lastKnownBri = bri;

        updateLeds();
        colorUpdated(CALL_MODE_DIRECT_CHANGE);
      }

      // If the brightness is changed not with the rotary, update the rotary
      if (bri != lastKnownBri) {
        currentPos = lastKnownPos = getPositionForBrightness();
        lastKnownBri = bri;
        rotaryEncoder->resetPosition(currentPos);
        updateLeds();
      }

      // Update LEDs here in loop to also validate that we can update/show
      if (isDirty && ledBus->canShow()) {
        isDirty = false;
        ledBus->show();
      }
    }

    void addToConfig(JsonObject &root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname

      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_ledIo)] = ledIo;
      top[FPSTR(_eaIo)] = eaIo;
      top[FPSTR(_ebIo)] = ebIo;
      top[FPSTR(_ledMode)] = ledMode;
      top[FPSTR(_ledBrightness)] = ledBrightness;
      top[FPSTR(_stepsPerClick)] = stepsPerClick;
      top[FPSTR(_incrementPerClick)] = incrementPerClick;
    }

    /**
     * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
     *
     * The function should return true if configuration was successfully loaded or false if there was no configuration.
     */
    bool readFromConfig(JsonObject &root)
    {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) {
        DEBUG_PRINTF("[%s] No config found. (Using defaults.)\n", _name);
        return false;
      }

      bool oldEnabled = enabled;
      int8_t oldLedIo = ledIo;
      int8_t oldEaIo =  eaIo;
      int8_t oldEbIo =  ebIo;
      byte oldLedMode = ledMode;
      byte oldStepsPerClick = stepsPerClick;
      byte oldIncrementPerClick = incrementPerClick;
      byte oldLedBrightness = ledBrightness;

      getJsonValue(top[FPSTR(_enabled)], enabled);
      getJsonValue(top[FPSTR(_ledIo)], ledIo);
      getJsonValue(top[FPSTR(_eaIo)], eaIo);
      getJsonValue(top[FPSTR(_ebIo)], ebIo);
      getJsonValue(top[FPSTR(_stepsPerClick)], stepsPerClick);
      getJsonValue(top[FPSTR(_incrementPerClick)], incrementPerClick);
      ledMode = top[FPSTR(_ledMode)] > 0 && top[FPSTR(_ledMode)] < 4 ? top[FPSTR(_ledMode)] : ledMode;
      ledBrightness = top[FPSTR(_ledBrightness)] > 0 && top[FPSTR(_ledBrightness)] <= 255 ? top[FPSTR(_ledBrightness)] : ledBrightness;

      if (!initDone) {
        // First run: reading from cfg.json
        // Nothing to do here, will be all done in setup() 
      }
      // Mod was disabled, so run setup()
      else if (enabled && enabled != oldEnabled) {
        DEBUG_PRINTF("[%s] Usermod has been re-enabled\n", _name);
        setup();
      }
      // Config has been changed, so adopt to changes
      else {
        if (!enabled) {
          DEBUG_PRINTF("[%s] Usermod has been disabled\n", _name);
          cleanup();
        }
        else {
          DEBUG_PRINTF("[%s] Usermod is enabled\n", _name);
          if (ledIo != oldLedIo) {
            delete ledBus;
            initLedBus();
          }

          if (ledBrightness != oldLedBrightness) {
            ledBus->setBrightness(ledBrightness);
            isDirty = true;
          }

          if (ledMode != oldLedMode) {
            updateLeds();
          }

          if (eaIo != oldEaIo || ebIo != oldEbIo || stepsPerClick != oldStepsPerClick || incrementPerClick != oldIncrementPerClick) {
            PinManager::deallocatePin(oldEaIo, PinOwner::UM_RGBRotaryEncoder);
            PinManager::deallocatePin(oldEbIo, PinOwner::UM_RGBRotaryEncoder);
            
            delete rotaryEncoder;
            initRotaryEncoder();
          }
        }

        DEBUG_PRINTF("[%s] Config (re)loaded\n", _name);
      }
      
      return true;
    }

    /*
      * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
      * This could be used in the future for the system to determine whether your usermod is installed.
      */
    uint16_t getId()
    {
      return USERMOD_RGB_ROTARY_ENCODER;
    }

    //More methods can be added in the future, this example will then be extended.
    //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};

byte RgbRotaryEncoderUsermod::currentPos = 5;
// strings to reduce flash memory usage (used more than twice)
const char RgbRotaryEncoderUsermod::_name[]              PROGMEM = "RGB-Rotary-Encoder";
const char RgbRotaryEncoderUsermod::_enabled[]           PROGMEM = "Enabled";
const char RgbRotaryEncoderUsermod::_ledIo[]             PROGMEM = "LED-pin";
const char RgbRotaryEncoderUsermod::_eaIo[]              PROGMEM = "ea-pin";
const char RgbRotaryEncoderUsermod::_ebIo[]              PROGMEM = "eb-pin";
const char RgbRotaryEncoderUsermod::_ledMode[]           PROGMEM = "LED-Mode";
const char RgbRotaryEncoderUsermod::_ledBrightness[]     PROGMEM = "LED-Brightness";
const char RgbRotaryEncoderUsermod::_stepsPerClick[]     PROGMEM = "Steps-per-Click";
const char RgbRotaryEncoderUsermod::_incrementPerClick[] PROGMEM = "Increment-per-Click";

static RgbRotaryEncoderUsermod rgb_rotary_encoder;
REGISTER_USERMOD(rgb_rotary_encoder);