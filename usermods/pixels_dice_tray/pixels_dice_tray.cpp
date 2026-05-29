#include <pixels_dice_interface.h>  // https://github.com/axlan/arduino-pixels-dice
#include "wled.h"

#include "dice_state.h"
#include "led_effects.h"
#include "tft_menu.h"

// Set this parameter to rotate the display. 1-3 rotate by 90,180,270 degrees.
#ifndef USERMOD_PIXELS_DICE_TRAY_ROTATION
  #define USERMOD_PIXELS_DICE_TRAY_ROTATION 0
#endif

// How often we are redrawing screen
#ifndef USERMOD_PIXELS_DICE_TRAY_REFRESH_RATE_MS
  #define USERMOD_PIXELS_DICE_TRAY_REFRESH_RATE_MS 200
#endif

// Time with no updates before screen turns off (-1 to disable)
#ifndef USERMOD_PIXELS_DICE_TRAY_TIMEOUT_MS
  #define USERMOD_PIXELS_DICE_TRAY_TIMEOUT_MS 5 * 60 * 1000
#endif

// Duration of each search for BLE devices.
#ifndef BLE_SCAN_DURATION_SEC
  #define BLE_SCAN_DURATION_SEC 4
#endif

// Time between searches for BLE devices.
#ifndef BLE_TIME_BETWEEN_SCANS_SEC
  #define BLE_TIME_BETWEEN_SCANS_SEC 5
#endif

#define WLED_DEBOUNCE_THRESHOLD \
  50  // only consider button input of at least 50ms as valid (debouncing)
#define WLED_LONG_PRESS \
  600  // long press if button is released after held for at least 600ms
#define WLED_DOUBLE_PRESS \
  350  // double press if another press within 350ms after a short press

class PixelsDiceTrayUsermod : public Usermod {
 private:
  bool enabled = true;

  DiceUpdate dice_update;

  // Settings
  uint32_t ble_scan_duration_sec = BLE_SCAN_DURATION_SEC;
  unsigned rotation = USERMOD_PIXELS_DICE_TRAY_ROTATION;
  DiceSettings dice_settings;

#if USING_TFT_DISPLAY
  MenuController menu_ctrl;
#endif

  static void center(String& line, uint8_t width) {
    int len = line.length();
    if (len < width)
      for (byte i = (width - len) / 2; i > 0; i--)
        line = ' ' + line;
    for (byte i = line.length(); i < width; i++)
      line += ' ';
  }

  // NOTE: THIS MOD DOES NOT SUPPORT CHANGING THE SPI PINS FROM THE UI! The
  // TFT_eSPI library requires that they are compiled in.
  static void SetSPIPinsFromMacros() {
#if USING_TFT_DISPLAY
    spi_mosi = TFT_MOSI;
    // Done in TFT library.
    if (TFT_MISO == TFT_MOSI) {
      spi_miso = -1;
    }
    spi_sclk = TFT_SCLK;
#endif
  }

  void UpdateDieNames(
      const std::array<const std::string, MAX_NUM_DICE>& new_die_names) {
    for (size_t i = 0; i < MAX_NUM_DICE; i++) {
      // If the saved setting was a wildcard, and that connected to a die, use
      // the new name instead of the wildcard. Saving this "locks" the name in.
      bool overriden_wildcard =
          new_die_names[i] == "*" && dice_update.connected_die_ids[i] != 0;
      if (!overriden_wildcard &&
          new_die_names[i] != dice_settings.configured_die_names[i]) {
        dice_settings.configured_die_names[i] = new_die_names[i];
        dice_update.connected_die_ids[i] = 0;
        last_die_events[i] = pixels::RollEvent();
      }
    }
  }

 public:
  PixelsDiceTrayUsermod()
#if USING_TFT_DISPLAY
      : menu_ctrl(&dice_settings)
#endif
  {
  }

  // Functions called by WLED

  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup() override {
    DEBUG_PRINTLN(F("DiceTray: init"));
#if USING_TFT_DISPLAY
    SetSPIPinsFromMacros();
    PinManagerPinType spiPins[] = {
        {spi_mosi, true}, {spi_miso, false}, {spi_sclk, true}};
    if (!PinManager::allocateMultiplePins(spiPins, 3, PinOwner::HW_SPI)) {
      enabled = false;
    } else {
      PinManagerPinType displayPins[] = {
          {TFT_CS, true}, {TFT_DC, true}, {TFT_RST, true}, {TFT_BL, true}};
      if (!PinManager::allocateMultiplePins(
              displayPins, sizeof(displayPins) / sizeof(PinManagerPinType),
              PinOwner::UM_FourLineDisplay)) {
        PinManager::deallocateMultiplePins(spiPins, 3, PinOwner::HW_SPI);
        enabled = false;
      }
    }

    if (!enabled) {
      DEBUG_PRINTLN(F("DiceTray: TFT Display pin allocations failed."));
      return;
    }
#endif

    // Need to enable WiFi sleep:
    // "E (1513) wifi:Error! Should enable WiFi modem sleep when both WiFi and Bluetooth are enabled!!!!!!"
    noWifiSleep = false;

    // Get the mode indexes that the effects are registered to.
    FX_MODE_SIMPLE_D20 = strip.addEffect(255, &simple_roll, _data_FX_MODE_SIMPLE_DIE);
    FX_MODE_PULSE_D20 = strip.addEffect(255, &pulse_roll, _data_FX_MODE_PULSE_DIE);
    FX_MODE_CHECK_D20 = strip.addEffect(255, &check_roll, _data_FX_MODE_CHECK_DIE);
    DIE_LED_MODES = {FX_MODE_SIMPLE_D20, FX_MODE_PULSE_D20, FX_MODE_CHECK_D20};

    // Start a background task scanning for dice.
    // On completion the discovered dice are connected to.
    pixels::ScanForDice(ble_scan_duration_sec, BLE_TIME_BETWEEN_SCANS_SEC);

#if USING_TFT_DISPLAY
    menu_ctrl.Init(rotation);
#endif
  }

  /*
   * connected() is called every time the WiFi is (re)connected
   * Use it to initialize network interfaces
   */
  void connected() override {
    // Serial.println("Connected to WiFi!");
  }

  /*
   * loop() is called continuously. Here you can check for events, read sensors,
   * etc.
   *
   * Tips:
   * 1. You can use "if (WLED_CONNECTED)" to check for a successful network
   * connection. Additionally, "if (WLED_MQTT_CONNECTED)" is available to check
   * for a connection to an MQTT broker.
   *
   * 2. Try to avoid using the delay() function. NEVER use delays longer than 10
   * milliseconds. Instead, use a timer check as shown here.
   */
  void loop() override {
    static long last_loop_time = 0;
    static long last_die_connected_time = millis();

    char mqtt_topic_buffer[MQTT_MAX_TOPIC_LEN + 16];
    char mqtt_data_buffer[128];

    // Check if we time interval for redrawing passes.
    if (millis() - last_loop_time < USERMOD_PIXELS_DICE_TRAY_REFRESH_RATE_MS) {
      return;
    }
    last_loop_time = millis();

    // Update dice_list with the connected dice
    pixels::ListDice(dice_update.dice_list);
    // Get all the roll/battery updates since the last loop
    pixels::GetDieRollUpdates(dice_update.roll_updates);
    pixels::GetDieBatteryUpdates(dice_update.battery_updates);

    // Go through list of connected die.
    // TODO: Blacklist die that are connected to, but don't match the configured
    //       names.
    std::array<bool, MAX_NUM_DICE> die_connected = {false, false};
    for (auto die_id : dice_update.dice_list) {
      bool matched = false;
      // First check if we've already matched this ID to a connected die.
      for (size_t i = 0; i < MAX_NUM_DICE; i++) {
        if (die_id == dice_update.connected_die_ids[i]) {
          die_connected[i] = true;
          matched = true;
          break;
        }
      }

      // If this isn't already matched, check if its name matches an expected name.
      if (!matched) {
        auto die_name = pixels::GetDieDescription(die_id).name;
        for (size_t i = 0; i < MAX_NUM_DICE; i++) {
          if (0 == dice_update.connected_die_ids[i] &&
              die_name == dice_settings.configured_die_names[i]) {
            dice_update.connected_die_ids[i] = die_id;
            die_connected[i] = true;
            matched = true;
            DEBUG_PRINTF_P(PSTR("DiceTray: %u (%s) connected.\n"), i,
                           die_name.c_str());
            break;
          }
        }

        // If it doesn't match any expected names, check if there's any wildcards to match.
        if (!matched) {
          auto description = pixels::GetDieDescription(die_id);
          for (size_t i = 0; i < MAX_NUM_DICE; i++) {
            if (dice_settings.configured_die_names[i] == "*") {
              dice_update.connected_die_ids[i] = die_id;
              die_connected[i] = true;
              dice_settings.configured_die_names[i] = die_name;
              DEBUG_PRINTF_P(PSTR("DiceTray: %u (%s) connected as wildcard.\n"),
                             i, die_name.c_str());
              break;
            }
          }
        }
      }
    }

    // Clear connected die that aren't still present.
    bool all_found = true;
    bool none_found = true;
    for (size_t i = 0; i < MAX_NUM_DICE; i++) {
      if (!die_connected[i]) {
        if (dice_update.connected_die_ids[i] != 0) {
          dice_update.connected_die_ids[i] = 0;
          last_die_events[i] = pixels::RollEvent();
          DEBUG_PRINTF_P(PSTR("DiceTray: %u disconnected.\n"), i);
        }

        if (!dice_settings.configured_die_names[i].empty()) {
          all_found = false;
        }
      } else {
        none_found = false;
      }
    }

    // Update last_die_events
    for (const auto& roll : dice_update.roll_updates) {
      for (size_t i = 0; i < MAX_NUM_DICE; i++) {
        if (dice_update.connected_die_ids[i] == roll.first) {
          last_die_events[i] = roll.second;
        }
      }
      if (WLED_MQTT_CONNECTED) {
        snprintf(mqtt_topic_buffer, sizeof(mqtt_topic_buffer), PSTR("%s/%s"),
                 mqttDeviceTopic, "dice/roll");
        const char* name = pixels::GetDieDescription(roll.first).name.c_str();
        snprintf(mqtt_data_buffer, sizeof(mqtt_data_buffer),
                 "{\"name\":\"%s\",\"state\":%d,\"val\":%d,\"time\":%d}", name,
                 int(roll.second.state), roll.second.current_face + 1,
                 roll.second.timestamp);
        mqtt->publish(mqtt_topic_buffer, 0, false, mqtt_data_buffer);
      }
    }

#if USERMOD_PIXELS_DICE_TRAY_TIMEOUT_MS > 0 && USING_TFT_DISPLAY
    // If at least one die is configured, but none are found
    if (none_found) {
      if (millis() - last_die_connected_time >
          USERMOD_PIXELS_DICE_TRAY_TIMEOUT_MS) {
        // Turn off LEDs and backlight and go to sleep.
        // Since none of the wake up pins are wired up, expect to sleep
        // until power cycle or reset, so don't need to handle normal
        // wakeup.
        bri = 0;
        applyFinalBri();
        menu_ctrl.EnableBacklight(false);
        gpio_hold_en((gpio_num_t)TFT_BL);
        gpio_deep_sleep_hold_en();
        esp_deep_sleep_start();
      }
    } else {
      last_die_connected_time = millis();
    }
#endif

    if (pixels::IsScanning() && all_found) {
      DEBUG_PRINTF_P(PSTR("DiceTray: All dice found. Stopping search.\n"));
      pixels::StopScanning();
    } else if (!pixels::IsScanning() && !all_found) {
      DEBUG_PRINTF_P(PSTR("DiceTray: Resuming dice search.\n"));
      pixels::ScanForDice(ble_scan_duration_sec, BLE_TIME_BETWEEN_SCANS_SEC);
    }
#if USING_TFT_DISPLAY
    menu_ctrl.Update(dice_update);
#endif
  }

  /*
   * addToJsonInfo() can be used to add custom entries to the /json/info part of
   * the JSON API. Creating an "u" object allows you to add custom key/value
   * pairs to the Info section of the WLED web UI. Below it is shown how this
   * could be used for e.g. a light sensor
   */
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    JsonArray lightArr = user.createNestedArray("DiceTray");  // name
    lightArr.add(enabled ? F("installed") : F("disabled"));   // unit
  }

  /*
   * addToJsonState() can be used to add custom entries to the /json/state part
   * of the JSON API (state object). Values in the state object may be modified
   * by connected clients
   */
  void addToJsonState(JsonObject& root) override {
    // root["user0"] = userVar0;
  }

  /*
   * readFromJsonState() can be used to receive data clients send to the
   * /json/state part of the JSON API (state object). Values in the state object
   * may be modified by connected clients
   */
  void readFromJsonState(JsonObject& root) override {
    // userVar0 = root["user0"] | userVar0; //if "user0" key exists in JSON,
    // update, else keep old value if (root["bri"] == 255)
    // Serial.println(F("Don't burn down your garage!"));
  }

  /*
   * addToConfig() can be used to add custom persistent settings to the cfg.json
   * file in the "um" (usermod) object. It will be called by WLED when settings
   * are actually saved (for example, LED settings are saved) If you want to
   * force saving the current state, use serializeConfig() in your loop().
   *
   * CAUTION: serializeConfig() will initiate a filesystem write operation.
   * It might cause the LEDs to stutter and will cause flash wear if called too
   * often. Use it sparingly and always in the loop, never in network callbacks!
   *
   * addToConfig() will also not yet add your setting to one of the settings
   * pages automatically. To make that work you still have to add the setting to
   * the HTML, xml.cpp and set.cpp manually.
   *
   * I highly recommend checking out the basics of ArduinoJson serialization and
   * deserialization in order to use custom settings!
   */
  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject("DiceTray");
    top["ble_scan_duration"] = ble_scan_duration_sec;
    top["die_0"] = dice_settings.configured_die_names[0];
    top["die_1"] = dice_settings.configured_die_names[1];
#if USING_TFT_DISPLAY
    top["rotation"] = rotation;
    JsonArray pins = top.createNestedArray("pin");
    pins.add(TFT_CS);
    pins.add(TFT_DC);
    pins.add(TFT_RST);
    pins.add(TFT_BL);
#endif
  }

  void appendConfigData() override {
    // Slightly annoying that you can't put text before an element.
    // The an item on the usermod config page has the following HTML:
    // ```html
    // Die 0
    // <input type="hidden" name="DiceTray:die_0" value="text">
    // <input type="text" name="DiceTray:die_0" value="*" style="width:250px;" oninput="check(this,'DiceTray')">
    // ```
    // addInfo let's you add data before or after the two input fields.
    //
    // To work around this, add info text to the end of the preceding item.
    //
    // See addInfo in wled00/data/settings_um.htm for details on what this function does.
    oappend(F(
        "addInfo('DiceTray:ble_scan_duration',1,'<br><br><i>Set to \"*\" to "
        "connect to any die.<br>Leave Blank to disable.</i><br><i "
        "class=\"warn\">Saving will replace \"*\" with die names.</i>','');"));
#if USING_TFT_DISPLAY
    oappend(F("ddr=addDropdown('DiceTray','rotation');"));
    oappend(F("addOption(ddr,'0 deg',0);"));
    oappend(F("addOption(ddr,'90 deg',1);"));
    oappend(F("addOption(ddr,'180 deg',2);"));
    oappend(F("addOption(ddr,'270 deg',3);"));
    oappend(F(
        "addInfo('DiceTray:rotation',1,'<br><i class=\"warn\">DO NOT CHANGE "
        "SPI PINS.</i><br><i class=\"warn\">CHANGES ARE IGNORED.</i>','');"));
    oappend(F("addInfo('TFT:pin[]',0,'','SPI CS');"));
    oappend(F("addInfo('TFT:pin[]',1,'','SPI DC');"));
    oappend(F("addInfo('TFT:pin[]',2,'','SPI RST');"));
    oappend(F("addInfo('TFT:pin[]',3,'','SPI BL');"));
#endif
  }

  /*
   * readFromConfig() can be used to read back the custom settings you added
   * with addToConfig(). This is called by WLED when settings are loaded
   * (currently this only happens once immediately after boot)
   *
   * readFromConfig() is called BEFORE setup(). This means you can use your
   * persistent values in setup() (e.g. pin assignments, buffer sizes), but also
   * that if you want to write persistent values to a dynamic buffer, you'd need
   * to allocate it here instead of in setup. If you don't know what that is,
   * don't fret. It most likely doesn't affect your use case :)
   */
  bool readFromConfig(JsonObject& root) override {
    // we look for JSON object:
    // {"DiceTray":{"rotation":0,"font_size":1}}
    JsonObject top = root["DiceTray"];
    if (top.isNull()) {
      DEBUG_PRINTLN(F("DiceTray: No config found. (Using defaults.)"));
      return false;
    }

    if (top.containsKey("die_0") && top.containsKey("die_1")) {
      const std::array<const std::string, MAX_NUM_DICE> new_die_names{
          top["die_0"], top["die_1"]};
      UpdateDieNames(new_die_names);
    } else {
      DEBUG_PRINTLN(F("DiceTray: No die names found."));
    }

#if USING_TFT_DISPLAY
    unsigned new_rotation = min(top["rotation"] | rotation, 3u);

    // Restore the SPI pins to their compiled in defaults.
    SetSPIPinsFromMacros();

    if (new_rotation != rotation) {
      rotation = new_rotation;
      menu_ctrl.Init(rotation);
    }

    // Update with any modified settings.
    menu_ctrl.Redraw();
#endif

    // use "return !top["newestParameter"].isNull();" when updating Usermod with
    // new features
    return !top["DiceTray"].isNull();
  }

  /**
   * handleButton() can be used to override default button behaviour. Returning true
   * will prevent button working in a default way.
   * Replicating button.cpp
   */
#if USING_TFT_DISPLAY
  bool handleButton(uint8_t b) override {
    if (!enabled || b > 1  // buttons 0,1 only
        || buttons[b].type == BTN_TYPE_SWITCH || buttons[b].type == BTN_TYPE_NONE ||
        buttons[b].type == BTN_TYPE_RESERVED ||
        buttons[b].type == BTN_TYPE_PIR_SENSOR ||
        buttons[b].type == BTN_TYPE_ANALOG ||
        buttons[b].type == BTN_TYPE_ANALOG_INVERTED) {
      return false;
    }

    unsigned long now = millis();
    static bool buttonPressedBefore[2] = {false};
    static bool buttonLongPressed[2] = {false};
    static unsigned long buttonPressedTime[2] = {0};
    static unsigned long buttonWaitTime[2] = {0};

    //momentary button logic
    if (!buttons[b].longPressed && isButtonPressed(b)) {  //pressed
      if (!buttons[b].pressedBefore) {
        buttons[b].pressedTime = now;
      }
      buttons[b].pressedBefore = true;

      if (now - buttons[b].pressedTime > WLED_LONG_PRESS) {  //long press
        menu_ctrl.HandleButton(ButtonType::LONG, b);
        buttons[b].longPressed = true;
        return true;
      }
    } else if (!isButtonPressed(b) && buttons[b].pressedBefore) {  //released

      long dur = now - buttons[b].pressedTime;
      if (dur < WLED_DEBOUNCE_THRESHOLD) {
        buttons[b].pressedBefore = false;
        return true;
      }  //too short "press", debounce

      bool doublePress = buttons[b].waitTime;  //did we have short press before?
      buttons[b].waitTime = 0;

      if (!buttons[b].longPressed) {  //short press
        // if this is second release within 350ms it is a double press (buttonWaitTime!=0)
        if (doublePress) {
          menu_ctrl.HandleButton(ButtonType::DOUBLE, b);
        } else {
          buttons[b].waitTime = now;
        }
      }
      buttons[b].pressedBefore = false;
      buttons[b].longPressed = false;
    }
    // if 350ms elapsed since last press/release it is a short press
    if (buttons[b].waitTime && now - buttons[b].waitTime > WLED_DOUBLE_PRESS &&
        !buttons[b].pressedBefore) {
      buttons[b].waitTime = 0;
      menu_ctrl.HandleButton(ButtonType::SINGLE, b);
    }

    return true;
  }
#endif

  /*
   * getId() allows you to optionally give your V2 usermod an unique ID (please
   * define it in const.h!). This could be used in the future for the system to
   * determine whether your usermod is installed.
   */
  uint16_t getId() { return USERMOD_ID_PIXELS_DICE_TRAY; }

  // More methods can be added in the future, this example will then be
  // extended. Your usermod will remain compatible as it does not need to
  // implement all methods from the Usermod base class!
};


static PixelsDiceTrayUsermod pixels_dice_tray;
REGISTER_USERMOD(pixels_dice_tray);