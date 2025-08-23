#pragma once

#include "wled.h"

//v2 usermod that allows to change brightness and color using a rotary encoder, 
/**
 * Initialize encoder and optional button pins and set initial timing.
 *
 * Configures encoder pins as INPUT_PULLUP and the optional button pin if provided.
 * Also initializes internal timing variables used by the loop().
 */
 
/**
 * Poll the rotary encoder and optional button (called frequently).
 *
 * Runs at ~500 Hz (every 2 ms) to:
 * - Toggle between brightness and color-adjust modes when the optional button transitions to LOW.
 * - Detect encoder rotation (A falling edge + B state) to increment/decrement:
 *   - Global brightness (bri) when in brightness mode (select_state == 0), clamped to [0,255].
 *   - Primary color hue when in color mode (select_state == 1), using HSV conversion with 0–255 wrap-around.
 * After any rotation-driven change, notifies the system via colorUpdated(CALL_MODE_BUTTON) and updateInterfaces(CALL_MODE_BUTTON).
 */
 
/**
 * Write usermod settings into the provided JSON root object.
 *
 * Creates a nested object "rotEncBrightness" containing:
 * - "fadeAmount" : amount to change per encoder step
 * - "pin" : array of three pin values (DT, CLK, optional button)
 *
 * @param root JSON object to which the configuration will be added.
 */
 
/**
 * Read usermod settings from the provided JSON root object.
 *
 * Loads settings with robust defaults and missing-value handling:
 * - Defaults: fadeAmount = 5; pins = [-1, -1, -1]
 * - Attempts to read the "rotEncBrightness" block and its "fadeAmount" and "pin" entries.
 * If any expected value is missing, defaults are used.
 *
 * @param root JSON object to read configuration from.
 * @return true if the "rotEncBrightness" block and all expected values were present; false if any values were missing (defaults applied).
 */
class RotaryEncoderBrightnessColor : public Usermod
{
private:
  //Private class members. You can declare variables and functions only accessible to your usermod here
  unsigned long lastTime = 0;
  unsigned long currentTime;
  unsigned long loopTime;

  unsigned char select_state = 0; // 0 = brightness 1 = color
  unsigned char button_state = HIGH;
  unsigned char prev_button_state = HIGH;
  CRGB fastled_col;
  CHSV prim_hsv;
  int16_t new_val;

  unsigned char Enc_A;
  unsigned char Enc_B;
  unsigned char Enc_A_prev = 0;

  // private class members configurable by Usermod Settings (defaults set inside readFromConfig())
  int8_t pins[3]; // pins[0] = DT from encoder, pins[1] = CLK from encoder, pins[2] = CLK from encoder (optional)
  int fadeAmount; // how many points to fade the Neopixel with each step

public:
  //Functions called by WLED

  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup()
  {
    //Serial.println("Hello from my usermod!");
    pinMode(pins[0], INPUT_PULLUP);
    pinMode(pins[1], INPUT_PULLUP);
    if(pins[2] >= 0) pinMode(pins[2], INPUT_PULLUP);
    currentTime = millis();
    loopTime = currentTime;
  }

  /**
   * Main periodic handler that polls the rotary encoder and optional push-button and applies adjustments.
   *
   * Called continuously by the main loop; this routine samples inputs every ~2 ms (500 Hz) and:
   * - If a button pin (pins[2]) is configured, detects LOW transitions to toggle select_state between
   *   brightness mode (0) and color/hue mode (1) with simple edge-based debouncing.
   * - Detects encoder A falling edges to determine rotation direction using the B line:
   *   - Clockwise (B HIGH): increases brightness (bri) in brightness mode or increases primary hue
   *     when in color mode.
   *   - Counter-clockwise (B LOW): decreases brightness or decreases primary hue respectively.
   * - Hue adjustments are performed in 0–255 space with wrap-around, converting between RGB and HSV
   *   to update colPri[0..2].
   * - After any rotation event, notifies the system by calling colorUpdated(CALL_MODE_BUTTON)
   *   and updateInterfaces(CALL_MODE_BUTTON).
   *
   * Side effects:
   * - Reads digital pins pins[0] (encoder A), pins[1] (encoder B), and optionally pins[2] (button).
   * - Mutates global state: bri, select_state, colPri[], Enc_A_prev, and timing state used for polling.
   * - Invokes colorUpdated() and updateInterfaces() on changes.
   *
   * Timing: samples inputs only when at least 2 ms have elapsed since the last sample.
   */
  void loop()
  {
    currentTime = millis(); // get the current elapsed time

    if (currentTime >= (loopTime + 2)) // 2ms since last check of encoder = 500Hz
    {
      if(pins[2] >= 0) {
        button_state = digitalRead(pins[2]);
        if (prev_button_state != button_state)
        {
          if (button_state == LOW)
          {
            if (select_state == 1)
            {
              select_state = 0;
            }
            else
            {
              select_state = 1;
            }
            prev_button_state = button_state;
          }
          else
          {
            prev_button_state = button_state;
          }
        }
      }
      int Enc_A = digitalRead(pins[0]); // Read encoder pins
      int Enc_B = digitalRead(pins[1]);
      if ((!Enc_A) && (Enc_A_prev))
      { // A has gone from high to low
        if (Enc_B == HIGH)
        { // B is high so clockwise
          if (select_state == 0)
          {
            if (bri + fadeAmount <= 255)
              bri += fadeAmount; // increase the brightness, dont go over 255
          }
          else
          {
            fastled_col.red = colPri[0];
            fastled_col.green = colPri[1];
            fastled_col.blue = colPri[2];
            prim_hsv = rgb2hsv_approximate(fastled_col);
            new_val = (int16_t)prim_hsv.h + fadeAmount;
            if (new_val > 255)
              new_val -= 255; // roll-over if  bigger than 255
            if (new_val < 0)
              new_val += 255; // roll-over if smaller than 0
            prim_hsv.h = (byte)new_val;
            hsv2rgb_rainbow(prim_hsv, fastled_col);
            colPri[0] = fastled_col.red;
            colPri[1] = fastled_col.green;
            colPri[2] = fastled_col.blue;
          }
        }
        else if (Enc_B == LOW)
        { // B is low so counter-clockwise
          if (select_state == 0)
          {
            if (bri - fadeAmount >= 0)
              bri -= fadeAmount; // decrease the brightness, dont go below 0
          }
          else
          {
            fastled_col.red = colPri[0];
            fastled_col.green = colPri[1];
            fastled_col.blue = colPri[2];
            prim_hsv = rgb2hsv_approximate(fastled_col);
            new_val = (int16_t)prim_hsv.h - fadeAmount;
            if (new_val > 255)
              new_val -= 255; // roll-over if  bigger than 255
            if (new_val < 0)
              new_val += 255; // roll-over if smaller than 0
            prim_hsv.h = (byte)new_val;
            hsv2rgb_rainbow(prim_hsv, fastled_col);
            colPri[0] = fastled_col.red;
            colPri[1] = fastled_col.green;
            colPri[2] = fastled_col.blue;
          }
        }
        //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
        // 6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
        colorUpdated(CALL_MODE_BUTTON);
        updateInterfaces(CALL_MODE_BUTTON);
      }
      Enc_A_prev = Enc_A;     // Store value of A for next time
      loopTime = currentTime; // Updates loopTime
    }
  }

  void addToConfig(JsonObject& root)
  {
    JsonObject top = root.createNestedObject("rotEncBrightness");
    top["fadeAmount"] = fadeAmount;
    JsonArray pinArray = top.createNestedArray("pin");
    pinArray.add(pins[0]);
    pinArray.add(pins[1]); 
    pinArray.add(pins[2]); 
  }

  /* 
   * This example uses a more robust method of checking for missing values in the config, and setting back to defaults:
   * - The getJsonValue() function copies the value to the variable only if the key requested is present, returning false with no copy if the value isn't present
   * - configComplete is used to return false if any value is missing, not just if the main object is missing
   * - The defaults are loaded every time readFromConfig() is run, not just once after boot
   * 
   * This ensures that missing values are added to the config, with their default values, in the rare but plausible cases of:
   * - a single value being missing at boot, e.g. if the Usermod was upgraded and a new setting was added
   * - a single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)
   * 
   * If configComplete is false, the default values are already set, and by returning false, WLED now knows it needs to save the defaults by calling addToConfig()
   */
  bool readFromConfig(JsonObject& root)
  {
    // set defaults here, they will be set before setup() is called, and if any values parsed from ArduinoJson below are missing, the default will be used instead
    fadeAmount = 5;
    pins[0] = -1;
    pins[1] = -1;
    pins[2] = -1;

    JsonObject top = root["rotEncBrightness"];

    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top["fadeAmount"], fadeAmount);
    configComplete &= getJsonValue(top["pin"][0], pins[0]);
    configComplete &= getJsonValue(top["pin"][1], pins[1]);
    configComplete &= getJsonValue(top["pin"][2], pins[2]);

    return configComplete;
  }
};
