/*
 * Usermod for detecting people entering/leaving a staircase and switching the
 * staircase on/off.
 *
 * Edit the Animated_Staircase_config.h file to compile this usermod for your
 * specific configuration.
 * 
 * See the accompanying README.md file for more info.
 */
#pragma once
#include "wled.h"

class MQTT_Animated_Staircase : public Usermod {
  private:

    /* configuration (available in API and stored in flash) */
    bool enabled = true;                   // Enable this usermod
    unsigned long segment_delay_ms = 150;   // Time between switching each segment
    unsigned long on_time_ms       = 30000; // The time for the light to stay on
    int8_t topPIRorTriggerPin      = -1;    // disabled //14 good
    int8_t bottomPIRorTriggerPin   = -1;    // disabled //12 good
    bool togglePower               = false; // toggle power on/off with staircase on/off
    bool mqttOnlyTrigger           = true;  // Set to true if triggering only through MQTT is desired
    bool swipeDirection = SWIPE_UP;  // Default to SWIPE_UP
    static constexpr bool SWIPE_UP = true;
    static constexpr bool SWIPE_DOWN = false;

    bool pendingSolidPresetAfterBlack = false; 
    unsigned long timeToApplySolidPreset = 0;  
    const unsigned long delayForSolidPresetMs = 250; 
    
    bool pendingMqttSwipe = false;
    unsigned long timeToApplyMqttSwipe = 0;
    char pendingMqttAction[16] = ""; // Safely holds the payload text for 250ms

    bool animationActive = false;
    bool autoPowerOffActive = false;  // Flag to indicate if auto power-off is active
    bool triggeredFromMQTT = false;  // Flag to indicate if the usermod was triggered from MQTT

    bool initDone = false;

    // Time between checking of the sensors
    const unsigned int scanDelay = 500;

    // Lights on or off.
    // Flipping this will start a transition.
    bool on = false;
    bool swipe = swipeDirection;

    // Indicates which Sensor was seen last (to determine the direction when swiping off)
    #define LOWER false
    #define UPPER true
    bool lastSensor = LOWER;

    // Time of the last transition action
    unsigned long lastTime = 0;

    // Time of the last sensor check
    unsigned long lastScanTime = 0;

    // Last time the lights were switched on or off
    unsigned long lastSwitchTime = 0;

    // For handling scheduled preset changes
    bool pendingPresetChange = false;
    uint8_t pendingPresetId = 0;
    unsigned long presetChangeTime = 0;

    // segment id between onIndex and offIndex are on.
    // controll the swipe by setting/moving these indices around.
    // onIndex must be less than or equal to offIndex
    byte onIndex = 0;
    byte offIndex = 0;

    // The maximum number of configured segments.
    // Dynamically updated based on user configuration.
    byte maxSegmentId = 1;
    byte minSegmentId = 0;

    // These values are used by the API to read the
    // last sensor state, or trigger a sensor
    // through the API
    bool topSensorRead     = false;
    bool topSensorWrite    = false;
    bool bottomSensorRead  = false;
    bool bottomSensorWrite = false;
    bool topSensorState    = false;
    bool bottomSensorState = false;

    bool haLightState = false; // false = off, true = on

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _segmentDelay[];
    static const char _onTime[];
    static const char _topPIRorTrigger_pin[];
    static const char _bottomPIRorTrigger_pin[];
    static const char _togglePower[];
    static const char _mqttOnly[];
    static const char _fadeTime_ms[];
    static const char _fadeTargetBrightness[];


void publishMqttToTopic(const char* topic, const char* message) 
{
#ifndef WLED_DISABLE_MQTT
  if (WLED_MQTT_CONNECTED) 
  {
    char fullTopic[128]; 
    snprintf(fullTopic, sizeof(fullTopic), "%s/%s", mqttDeviceTopic, topic);
    mqtt->publish(fullTopic, 0, false, message);
  }
#else
  (void)topic;
  (void)message;
#endif
}

void applyPresetWithDebug(uint8_t presetId)
{
    Serial.print(F("Applying preset: ")); Serial.println(presetId);
    applyPreset(presetId);
    currentPreset = presetId; // Reset to the solid preset
    yield(); // Give the system time to process
}


void publishMqtt(bool bottom, const char* state) 
{
#ifndef WLED_DISABLE_MQTT
  //Check if MQTT Connected, otherwise it will crash the 8266
  if (WLED_MQTT_CONNECTED)
  {
    char subuf[64];
    sprintf_P(subuf, PSTR("%s/motion/%d"), mqttDeviceTopic, (int)bottom);
    mqtt->publish(subuf, 0, false, state);
  }
#else
  (void)bottom;
  (void)state;
#endif
}


 void setIndex(String action) {
    minSegmentId = strip.getMainSegmentId();
    maxSegmentId = strip.getLastActiveSegmentId() + 1;

   if (action == "on-up") {
        // Start with only the top segment on
        onIndex = maxSegmentId - 1;  // Last segment (top)
        offIndex = maxSegmentId;     // Boundary after the last segment
    } else if (action == "on-down") {
        // Start with only the bottom segment on
        onIndex = minSegmentId;      // First segment (bottom)
        offIndex = minSegmentId + 1; // Boundary after the first segment
    } else if (action == "off-up" || action == "off-down") {
        // For both off animations, start with all segments (that were on) conceptually on.
        // The swipeDirection will determine how they turn off.
        onIndex = minSegmentId;
        offIndex = maxSegmentId;
        Serial.println(F("Set for OFF: onIndex=min, offIndex=max"));
    }
}
 


    
bool onMqttMessage(char* topic, char* payload) {
#ifndef WLED_DISABLE_MQTT
    if (strlen(topic) == 6 && strncmp_P(topic, PSTR("/swipe"), 6) == 0) {
        Serial.print(F("MQTT swipe detected: "));
        Serial.println(payload);

        // 1. Save the payload text so it isn't deleted from memory
        strncpy(pendingMqttAction, payload, sizeof(pendingMqttAction) - 1);
        pendingMqttAction[sizeof(pendingMqttAction) - 1] = '\0';

        // 2. Apply the Black preset instantly
        applyPresetWithDebug(3); 

        // 3. Start your non-blocking timer
        pendingMqttSwipe = true;
        timeToApplyMqttSwipe = millis(); 
        
        return true; // Message handled!
    }
#else
    (void)topic;
    (void)payload;
#endif
    return false; 
}

void onMqttConnect(bool sessionPresent) 
{
#ifndef WLED_DISABLE_MQTT

  (void)sessionPresent;

  // Check if the device topic is set
  if (mqttDeviceTopic[0] != 0) 
  {
    char subuf[128];
    
    // Subscribe to /swipe
    snprintf(subuf, sizeof(subuf), "%s/swipe", mqttDeviceTopic);
    mqtt->subscribe(subuf, 0);

    // Subscribe to /ha_light_state
    snprintf(subuf, sizeof(subuf), "%s/ha_light_state", mqttDeviceTopic);
    mqtt->subscribe(subuf, 0);
  }
#else
  (void)sessionPresent;
#endif
}



bool checkSensors() 
{
    bool sensorChanged = false;

    if ((millis() - lastScanTime) > scanDelay) {
        lastScanTime = millis();

        bottomSensorRead = bottomSensorWrite ||
            ((bottomPIRorTriggerPin < 0) ? false : digitalRead(bottomPIRorTriggerPin));

        topSensorRead = topSensorWrite ||
            ((topPIRorTriggerPin < 0) ? false : digitalRead(topPIRorTriggerPin));

        // Publish MQTT for bottom sensor if state changed
        if (bottomSensorRead != bottomSensorState) 
        {
            bottomSensorState = bottomSensorRead;
            sensorChanged = true;

            // Publish to "motion/bottom" with "on" or "off"
            publishMqttToTopic("motion/bottom", bottomSensorState ? "on" : "off");
        }

        // Publish MQTT for top sensor if state changed
        if (topSensorRead != topSensorState) 
        {
            topSensorState = topSensorRead;
            sensorChanged = true;

            publishMqttToTopic("motion/top", topSensorState ? "on" : "off");

        }

        // Reset API flags
        topSensorWrite = false;
        bottomSensorWrite = false;
    }

    return sensorChanged;
}





void autoPowerOff() {
        // This function is called when 'on' is true, and no animations are active.
    // It checks if it's time to start the OFF sequence.

    if (!on) return; // Should be true if called from loop as intended, but as a safeguard.

    if ((millis() - lastSwitchTime) > on_time_ms) {
        Serial.println(F("Auto power-off: Timer expired, initiating OFF sequence."));

        // --- Initiate the OFF sequence ---
        // 'on' is already true. Now set it to false as we start the turn-off process.
        // However, the global 'on' state should reflect that the system *was* on and is now *turning* off.
        // The 'on' variable in updateSwipe (if (on) else {}) will control ON vs OFF animation steps.
        // So, we set the global 'on' to false here to signal that the period of "being fully on" is over.
        on = false;

        animationActive = true;     // Enable animation for the OFF sequence
        autoPowerOffActive = true;  // Flag that the auto-off initiated animation is running
        lastTime = millis();        // Reset segment timer for the start of the OFF animation in updateSwipe

        // Determine OFF direction for FIFO (same logic as before)
        if (lastSensor == UPPER) { // Lights were turned ON top-to-bottom
            swipeDirection = SWIPE_DOWN; // For FIFO OFF top-to-bottom
            Serial.println(F("AutoPowerOff: lastSensor UPPER. Setting OFF swipe to SWIPE_DOWN."));
            setIndex("off-down"); // Initializes onIndex=min, offIndex=max
        } else { // lastSensor == LOWER (Lights were turned ON bottom-to-top)
            swipeDirection = SWIPE_UP;   // For FIFO OFF bottom-to-top
            Serial.println(F("AutoPowerOff: lastSensor LOWER. Setting OFF swipe to SWIPE_UP."));
            setIndex("off-up");   // Initializes onIndex=min, offIndex=max
        }
        // updateSwipe() will be called by loop() to perform the animation steps.
    }
}




void updateSwipe() {
    if (!animationActive) return; // If no ON or OFF animation is active, do nothing

    if ((millis() - lastTime) > segment_delay_ms) {
        lastTime = millis();
        bool full = false;

        if (on) { // Turning ON logic
            // ... (previous ON logic for SWIPE_UP and SWIPE_DOWN) ...
            // Example for SWIPE_UP ON:
            if (swipeDirection == SWIPE_UP) {
                Serial.print(F("SWIPE_UP ON: decreasing onIndex from ")); /* ... */ onIndex = MAX(minSegmentId, onIndex - 1); full = (onIndex == minSegmentId); /* ... */
            } else { // SWIPE_DOWN ON
                Serial.print(F("SWIPE_DOWN ON: increasing offIndex from ")); /* ... */ offIndex = MIN(maxSegmentId, offIndex + 1); full = (offIndex == maxSegmentId); /* ... */
            }

            if (full) {
                Serial.println(F("All segments ON. Waiting for auto power-off timer."));
                animationActive = false; // ON Animation complete. Loop will now check autoPowerOff condition.
                                         // 'on' is still true.
                                         // 'lastSwitchTime' was set at the start of the ON sequence.
            }
        } else { // Turning OFF logic (either by autoPowerOff or manual MQTT "off-*")
            // ... (previous OFF logic for SWIPE_UP and SWIPE_DOWN, which implements FIFO) ...
            // Example for SWIPE_UP OFF:
            if (swipeDirection == SWIPE_UP) { // OFF Bottom-to-Top
                 Serial.print(F("SWIPE_UP OFF (Bottom-to-Top): increasing onIndex from ")); /* ... */ if (onIndex < offIndex) { onIndex++; } full = (onIndex >= offIndex); /* ... */
            } else { // SWIPE_DOWN OFF (Top-to-Bottom)
                 Serial.print(F("SWIPE_DOWN OFF (Top-to-Bottom): decreasing offIndex from ")); /* ... */ if (offIndex > onIndex) { offIndex--; } full = (offIndex <= onIndex); /* ... */
            }

            if (full) {
                Serial.println(F("OFF Animation complete."));
                animationActive = false;
                if (autoPowerOffActive) { // Check if this was an auto power off
                    autoPowerOffActive = false; // Reset flag
                }
                
                onIndex = maxSegmentId; 
                offIndex = minSegmentId; 

                updateSegments(); // Ensure segments are visually off

                Serial.println(F("All segments turned OFF. Applying black preset."));
                strip.timebase = 0;
                applyPresetWithDebug(3); // Apply BLACK preset

                pendingSolidPresetAfterBlack = true;
                timeToApplySolidPreset = millis() + delayForSolidPresetMs;
                // ... (rest of your existing cleanup) ...
                return; 
            }
        }
        updateSegments();
    }
}

void updateSegments()
{
    // First, check if we need to turn everything off (final state)
    bool allOff = (onIndex >= offIndex);

    for (int i = minSegmentId; i < maxSegmentId; i++) 
    {
        Segment &seg = strip.getSegment(i);
        if (!seg.isActive()) continue; // Skip gaps

        bool segmentOn;
        if (allOff) {
            segmentOn = false; // Force all segments off
        } else {
            segmentOn = (i >= onIndex && i < offIndex);
        }

        seg.setOption(SEG_OPTION_ON, segmentOn);
    }
    //Serial.println();

    strip.trigger();
    colorUpdated(CALL_MODE_DIRECT_CHANGE);
    stateChanged = true;
}


void enable(bool enable)
    {
      if (enable) 
      {
        onIndex  = minSegmentId = strip.getMainSegmentId(); // it may not be the best idea to start with main segment as it may not be the first one
        offIndex = maxSegmentId = strip.getLastActiveSegmentId() + 1;

        // shorten the strip transition time to be equal or shorter than segment delay
        transitionDelay = segment_delay_ms;
        strip.setTransition(segment_delay_ms);
        strip.trigger();
      } else {
        if (togglePower && !on && offMode) toggleOnOff(); // toggle power on if off
        // Restore segment options
        for (int i = 0; i <= strip.getLastActiveSegmentId(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue; // skip vector gaps
          seg.setOption(SEG_OPTION_ON, true);
        }
        strip.trigger();  // force strip update
        stateChanged = true;  // inform external devices/UI of change
        colorUpdated(CALL_MODE_DIRECT_CHANGE);
      }
      enabled = enable;
    }

  public:
    void setup() 
    {
        Serial.println(F("Setting up Animated Staircase usermod..."));

        // Standardize invalid pin numbers to -1
        if (topPIRorTriggerPin    < 0) topPIRorTriggerPin    = -1;
        if (bottomPIRorTriggerPin < 0) bottomPIRorTriggerPin = -1;

        // Allocate pins using PinManager
        PinManagerPinType pins[2] = {
            { topPIRorTriggerPin },
            { bottomPIRorTriggerPin },
        };

        // Attempt to allocate pins
        if (!PinManager::allocateMultiplePins(pins, 2, PinOwner::UM_AnimatedStaircase)) 
        {  // Fix array size from 4 to 2
            Serial.println(F("Pin allocation failed! Disabling sensors."));
            topPIRorTriggerPin = -1;
            bottomPIRorTriggerPin = -1;
            enabled = false;  // Disable the usermod if pin allocation fails
        } 
        else 
        {
          enabled = true;
        }
          

        // Set up valid pins as INPUT
        if (topPIRorTriggerPin >= 0) {
            pinMode(topPIRorTriggerPin, INPUT);
        } else {
            Serial.println(F("Top PIR Sensor pin invalid, not initialized."));
        }

        if (bottomPIRorTriggerPin >= 0) {
            pinMode(bottomPIRorTriggerPin, INPUT);
        } else {
            Serial.println(F("Bottom PIR Sensor pin invalid, not initialized."));
        }



        applyPresetWithDebug(3);  // Apply the swipe preset
        delay(200);
        applyPresetWithDebug(1);
        delay(200);

                
        minSegmentId = strip.getMainSegmentId();  // Starting main segment ID
        maxSegmentId = strip.getLastActiveSegmentId() + 1;

        onIndex = minSegmentId;
        offIndex = maxSegmentId -1; // THIS IS IMPORTANT: Initialize offIndex to the end

        enable(enabled);
        initDone = true;
        Serial.println(F("Animated Staircase usermod setup complete."));

    }


    void loop() {
        checkSensors(); // Assuming this is your sensor checking logic
    
        // Handle pending preset change AFTER black preset
        if (pendingSolidPresetAfterBlack && millis() >= timeToApplySolidPreset) {
            if (!animationActive && !on) {
                Serial.println(F("Applying pending solid preset (2)."));
                applyPresetWithDebug(2);
            } else {
                Serial.println(F("New animation started or lights turned on; cancelling pending solid preset (2)."));
            }
            pendingSolidPresetAfterBlack = false;
        }

        if (pendingMqttSwipe && (millis() - timeToApplyMqttSwipe >= delayForSolidPresetMs)) {
        
            pendingMqttSwipe = false; // Turn the timer off
            
            applyPresetWithDebug(1);  // Apply the actual swipe preset
            
            // Now run the logic using our saved string!
            setIndex(pendingMqttAction);
            triggeredFromMQTT = true;

            if (!animationActive || !on) {
                if (strcmp(pendingMqttAction, "on-up") == 0) {
                    swipeDirection = SWIPE_UP;
                    lastSensor = UPPER;
                    on = true;
                    animationActive = true;
                    autoPowerOffActive = false;
                } else if (strcmp(pendingMqttAction, "on-down") == 0) {
                    swipeDirection = SWIPE_DOWN;
                    lastSensor = LOWER;
                    on = true;
                    animationActive = true;
                    autoPowerOffActive = false;
                } else if (strcmp(pendingMqttAction, "off-up") == 0) {
                    swipeDirection = SWIPE_UP;
                    on = false;
                    animationActive = true;
                } else if (strcmp(pendingMqttAction, "off-down") == 0) {
                    swipeDirection = SWIPE_DOWN;
                    on = false;
                    animationActive = true;
                }
                lastSwitchTime = millis();
                lastTime = millis();
            }
        }
    
        if (!enabled || strip.isUpdating()) return;
    
        // If lights are ON, and no ON animation is running, and no OFF animation has been started by autoPowerOff
        if (on && !animationActive && !autoPowerOffActive) {
            autoPowerOff(); // Renamed for clarity, or keep as autoPowerOff
        }
    
        updateSwipe();  // Runs continuously, handling active ON or OFF animations
    }

uint16_t getId() { return USERMOD_ID_MQTT_ANIMATED_STAIRCASE; }

void appendConfigData() 
{
    oappend(SET_F("addCheckbox('mqttOnlyTrigger',mqttOnlyTrigger,'MQTT Only Trigger','Enable triggering only through MQTT');"));
}


  /**
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     * 
     * addToConfig() will make your settings editable through the Usermod Settings page automatically.
     *
     * Usermod Settings Overview:
     * - Numeric values are treated as floats in the browser.
     *   - If the numeric value entered into the browser contains a decimal point, it will be parsed as a C float
     *     before being returned to the Usermod.  The float data type has only 6-7 decimal digits of precision, and
     *     doubles are not supported, numbers will be rounded to the nearest float value when being parsed.
     *     The range accepted by the input field is +/- 1.175494351e-38 to +/- 3.402823466e+38.
     *   - If the numeric value entered into the browser doesn't contain a decimal point, it will be parsed as a
     *     C int32_t (range: -2147483648 to 2147483647) before being returned to the usermod.
     *     Overflows or underflows are truncated to the max/min value for an int32_t, and again truncated to the type
     *     used in the Usermod when reading the value from ArduinoJson.
     * - Pin values can be treated differently from an integer value by using the key name "pin"
     *   - "pin" can contain a single or array of integer values
     *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
     *     - Red color indicates a conflict.  Yellow color indicates a pin with a warning (e.g. an input-only pin)
     *   - Tip: use int8_t to store the pin value in the Usermod, so a -1 value (pin not set) can be used
     *
     * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
     * 
     * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.  
     * You will have to add the setting to the HTML, xml.cpp and set.cpp manually.
     * See the WLED Soundreactive fork (code and wiki) for reference.  https://github.com/atuline/WLED
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    

void addStaircaseToJsonObject(JsonObject& staircase, bool forJsonState)
{
  
    staircase[F("enabled")] = enabled;
    staircase[F("mqtt-only-trigger")] = mqttOnlyTrigger;
    staircase[F("segment-delay")] = segment_delay_ms;
    staircase[F("on-time")] = on_time_ms / 1000; // Convert to seconds for storage
    staircase[F("topPIRorTrigger-pin")] = topPIRorTriggerPin;
    staircase[F("bottomPIRorTrigger-pin")] = bottomPIRorTriggerPin;
    staircase[F("toggle-power")] = togglePower;
    //staircase[F("fade-time_ms")] = fadeTime_ms;
    //staircase[F("fade-target-brightness")] = fadeTargetBrightness;

}   

void getUsermodConfigFromJsonObject(JsonObject& staircase)
{
    if (staircase.isNull()) {
        Serial.println(F("Error: JSON object is null. Skipping usermod config loading."));
        return;
    }

    // Read and assign values from JSON
    getJsonValue(staircase[F("enabled")], enabled);
    getJsonValue(staircase[F("mqtt-only-trigger")], mqttOnlyTrigger);
    getJsonValue(staircase[F("segment-delay")], segment_delay_ms);
    
    if (staircase.containsKey(F("on-time"))) {
        getJsonValue(staircase[F("on-time")], on_time_ms);
        on_time_ms *= 1000;  // Convert seconds back to milliseconds only if it was set
    }

    getJsonValue(staircase[F("topPIRorTrigger-pin")], topPIRorTriggerPin);
    getJsonValue(staircase[F("bottomPIRorTrigger-pin")], bottomPIRorTriggerPin);
    getJsonValue(staircase[F("toggle-power")], togglePower);

    //getJsonValue(staircase[F("fade-time_ms")], fadeTime_ms);
    //getJsonValue(staircase[F("fade-target-brightness")], fadeTargetBrightness);
}

void addToConfig(JsonObject& root)
 {
    JsonObject staircase = root.createNestedObject(FPSTR(_name));

    if (staircase.isNull()) {
        staircase = root.createNestedObject(FPSTR(_name));
    }

    addStaircaseToJsonObject(staircase, false);
}


    /*
    * Reads the configuration to internal flash memory before setup() is called.
    * 
    * The function should return true if configuration was successfully loaded or false if there was no configuration.
    */
  bool readFromConfig(JsonObject& root) 
{
    Serial.print(F("TopP Pin in readFromConfig: "));
    Serial.println(topPIRorTriggerPin);    
    
    JsonObject staircase = root[FPSTR(_name)];
    if (staircase.isNull()) {
        Serial.println(F("No config found. Using defaults."));
        return false;
    }

    int8_t oldTopPin = topPIRorTriggerPin;
    int8_t oldBottomPin = bottomPIRorTriggerPin;

    // Load the new config from the WLED UI
    // NOTE: This automatically overwrites topPIRorTriggerPin and bottomPIRorTriggerPin with the new UI values!
    getUsermodConfigFromJsonObject(staircase);

    int8_t newTopPin = topPIRorTriggerPin;
    int8_t newBottomPin = bottomPIRorTriggerPin;

    // Handle pin changes dynamically
    if (initDone) {
        if (newTopPin != oldTopPin || newBottomPin != oldBottomPin) { // <-- ADDED OPENING BRACKET
            Serial.println(F("Pins have changed, reallocating..."));

            // Deallocate OLD pins (Must use old variables here!)
            PinManager::deallocatePin(oldTopPin, PinOwner::UM_AnimatedStaircase);
            PinManager::deallocatePin(oldBottomPin, PinOwner::UM_AnimatedStaircase);

            // Attempt to reallocate NEW pins
            PinManagerPinType pins[2] = {
                { newTopPin },
                { newBottomPin },
            };

            if (!PinManager::allocateMultiplePins(pins, 2, PinOwner::UM_AnimatedStaircase)) {
                Serial.println(F("Pin reallocation failed! Disabling sensors."));
                topPIRorTriggerPin = -1;
                bottomPIRorTriggerPin = -1;
                enabled = false;  // Disable the usermod if pin allocation fails
                return true;  // Exit early, don't call setup()
            }

            setup();
        } 
    } else {
        // First run, just assign values
        topPIRorTriggerPin = newTopPin;
        bottomPIRorTriggerPin = newBottomPin;
    }

    return !staircase[FPSTR(_togglePower)].isNull();
}


    /*
    * Shows the delay between steps and power-off time in the "info"
    * tab of the web-UI.
    */
    void addToJsonInfo(JsonObject& root) 
    {
      JsonObject user = root["u"];
      if (user.isNull()) {
        user = root.createNestedObject("u");
      }

      JsonArray infoArr = user.createNestedArray(FPSTR(_name));  // name

      String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
      uiDomString += FPSTR(_name);
      uiDomString += F(":{");
      uiDomString += FPSTR(_enabled);
      uiDomString += enabled ? F(":false}});\">") : F(":true}});\">");
      uiDomString += F("<i class=\"icons ");
      uiDomString += enabled ? "on" : "off";
      uiDomString += F("\">&#xe08f;</i></button>");
      infoArr.add(uiDomString);
    }
};

// strings to reduce flash memory usage (used more than twice)
const char MQTT_Animated_Staircase::_name[]                      PROGMEM = "staircase";
const char MQTT_Animated_Staircase::_enabled[]                   PROGMEM = "enabled";
const char MQTT_Animated_Staircase::_segmentDelay[]              PROGMEM = "segment-delay-ms";
const char MQTT_Animated_Staircase::_onTime[]                    PROGMEM = "on-time-s";
const char MQTT_Animated_Staircase::_topPIRorTrigger_pin[]       PROGMEM = "topPIRorTrigger_pin";
const char MQTT_Animated_Staircase::_bottomPIRorTrigger_pin[]    PROGMEM = "bottomPIRorTrigger_pin";
const char MQTT_Animated_Staircase::_togglePower[]               PROGMEM = "toggle-on-off";
const char MQTT_Animated_Staircase::_mqttOnly[]                  PROGMEM = "mqtt-only-trigger";

// 1. Create a living instance of your class (you can name this variable whatever you want)
MQTT_Animated_Staircase myStaircaseMod;

REGISTER_USERMOD(myStaircaseMod);
