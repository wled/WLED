#include "wled.h"

#if defined(WORD_CLOCK_LANGUAGE_NL)
  #include "lang/word_clock_language_nl.h"
  namespace WordClock = WordClockDutch;
#else
  #ifndef WORD_CLOCK_LANGUAGE_DE
    #define WORD_CLOCK_LANGUAGE_DE
  #endif
  #include "lang/word_clock_language_de.h"
  namespace WordClock = WordClockGerman;
#endif

/*
 * Word Clock Usermod
 * This usermod displays the time in words using a compile-time selected
 * language pack. It uses the WLED V2 usermod API to integrate with WLED and
 * apply an overlay on the LED strip to light up the characters representing
 * the current time in Dutch. This assumes that each LED of the LED strip is
 * arranged behind a matrix of characters, lighting up one character per LED,
 * and that the character matrix contains all words needed to display the time
 * in a language-specific sentence.
 * 
 * These settings are available in the Config > Usermods > Word Clock
 * Settings page:
 *
 *   - `Active`: turn the word clock on or off.
 *   - `Brightness Active`: brightness of the letters used for the current
 *     time. Use 0 for off and 255 for full brightness.
 *   - `Brightness Inactive`: brightness of the other letters. Use 0 for off
 *     and 255 for full brightness.
 *   - `Meander`: set to `false` when the LED strip runs left to right on
 *      every row. Set to `true` when each row alternates direction.
 *   - `Character Matrix`: the uppercase letters in your clock face. Include
 *      all the words needed to display the Dutch time sentences, with each
 *      row placed directly after the previous row.
 *   - `Character Matrix Width`: the number of letters in each row. It cannot
 *     be greater than the total number of letters or smaller than the longest
 *     word the clock needs to display.
 *   - `Led Offset`: the number of physical LEDs before the first word-clock
 *     letter.
 *   - `Test Hour`: the hour to display for testing, from 0 to 23. Set it to
 *     -1 to use the real time.
 *   - `Test Minute`: the minute to display for testing, from 0 to 59.
 */

class WordClockUsermod : public Usermod
{
private:
  // The matrix of characters that can be highlighted to display the time.
  // This matrix must contain all words needed to display the time in a
  // Dutch sentence, e.g. "HET IS KWART OVER TIEN".
  // The characters of each row are stored sequentially, and the rows are
  // stored sequentially as well, left to right, and top to bottom.
  String characterMatrix = FPSTR(WordClock::DEFAULT_CHARACTER_MATRIX);

  // The number of characters per row
  int characterMatrixWidth = WordClock::DEFAULT_CHARACTER_MATRIX_WIDTH;

  // Is the ledstrip always from left to right on each row, or does it
  // meander through the rows (i.e. go from left to right on the first row,
  // then continue right to left on the second row, and so on)?
  bool meander = true;

  // Keep track of the last time our loop executed.
  // Initialised to trigger an update on the very first loop() call.
  int lastRefreshMinute = -1;

  // ledMask[i] is true if the LED at index i should be on for the current time, and false if it should be off
  bool* ledMask = nullptr;
  bool* wordMask = nullptr;


  // Set your config variables to their boot default value (this can also be done in readFromConfig() or a constructor if you prefer)

  // Is this usermod active?
  bool usermodActive = false;

  bool displayItIs = false;
  bool nord = false;
  int ledOffset = 0;

  // Opacity (0=off, 255=full brightness) applied to LEDs that ARE part of the current time sentence.
  int opacityActive = 255;

  // Opacity (0=off, 255=full brightness) applied to LEDs that are NOT part of the current time sentence.
  int opacityInactive = 0;

  // Test time override: set testHour (0‥23) and testMinute (0‥59) to force a specific time to be
  // displayed instead of the real time. Set testHour to -1 to disable (use real time).
  int testHour = -1;
  int testMinute = 0;

  void allocateLedMask() {
    if (ledMask) {
      d_free(ledMask);
      ledMask = nullptr;
    }
    if (wordMask) {
      d_free(wordMask);
      wordMask = nullptr;
    }

    size_t matrixLength = characterMatrix.length();

    if (matrixLength == 0)
      return;

    ledMask = (bool*) d_malloc(matrixLength * sizeof(bool));
    wordMask = (bool*) d_malloc(matrixLength * sizeof(bool));

    if (ledMask && wordMask) {
      memset(ledMask, 0, matrixLength * sizeof(bool));
      memset(wordMask, 0, matrixLength * sizeof(bool));
    } else {
      if (ledMask) {
        d_free(ledMask);
        ledMask = nullptr;
      }
      if (wordMask) {
        d_free(wordMask);
        wordMask = nullptr;
      }
    }
  }

  String lastSentence = "";
  int lastPhraseKey = -1;
  bool phraseMaskValid = false;

  void updateMinuteDots(const WordClockCore::MinuteDotMarkers& markers, uint8_t minuteDotCount) {
    if (!markers.enabled() || !ledMask || !wordMask)
      return;

    memcpy(ledMask, wordMask, characterMatrix.length() * sizeof(bool));

    for (uint8_t dot = 0; dot < WordClockCore::MAX_MINUTE_DOTS; ++dot) {
      const int markerIndex = markers.positions[dot];

      if (markerIndex < 0 || static_cast<size_t>(markerIndex) >= characterMatrix.length())
        continue;

      int ledIndex = markerIndex;

      if (meander)
        ledIndex = WordClockCore::toMeanderIndex(ledIndex, characterMatrixWidth, characterMatrix.length());

      if (ledIndex >= 0 && static_cast<size_t>(ledIndex) < characterMatrix.length())
        ledMask[ledIndex] = dot < minuteDotCount;
    }
  }

  /*
   * Update the ledMask for the current time, by setting ledMask[i] to true if
   * the LED at index i should be on for the current time, and false if it
   * should be off.
   */
  // Clamp a value to the inclusive [lo, hi] range.
  int clampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  void updateLedMaskForCurrentTime() {
    const int currentMinutes = testHour >= 0
      ? (testHour * 60 + testMinute) % 1440
      : (hour(localTime) * 60 + minute(localTime)) % 1440;
    WordClockCore::MinuteDotMarkers markers;

    if (!WordClockCore::parseMinuteDotMarkers(characterMatrix.c_str(), characterMatrix.length(), markers))
      return;

    const WordClockCore::TimeContext time = WordClockCore::makeTimeContext(currentMinutes, markers.enabled());
    const int phraseKey = markers.enabled()
      ? currentMinutes - currentMinutes % 5
      : ((time.totalMinutes / 60) * 60) + time.displayedMinute;

    if (phraseMaskValid && phraseKey == lastPhraseKey) {
      updateMinuteDots(markers, time.minuteDotCount);
      return;
    }

    WordClockCore::DisplayPlan plan;

    bool placementOk = false;
  #if defined(WORD_CLOCK_LANGUAGE_NL)
    placementOk = WordClock::buildPlan(time, displayItIs, plan) &&
          WordClock::placePlan(time, plan, characterMatrix, characterMatrixWidth, meander, ledMask,
             static_cast<uint16_t>(phraseKey));
  #else
    placementOk = WordClock::buildPlan(time, displayItIs, nord, plan) &&
            WordClock::placePlan(time, plan, characterMatrix, characterMatrixWidth, meander, ledMask,
                                characterMatrix.length());
  #endif
    if (!placementOk)
      return;

    memcpy(wordMask, ledMask, characterMatrix.length() * sizeof(bool));
    if (markers.enabled()) {
      for (uint8_t dot = 0; dot < WordClockCore::MAX_MINUTE_DOTS; ++dot)
        wordMask[markers.positions[dot]] = false;
    }
    lastPhraseKey = phraseKey;
    phraseMaskValid = true;
    updateMinuteDots(markers, time.minuteDotCount);

    String sentence;

    for (uint8_t index = 0; index < plan.count; ++index) {
      if (index > 0) sentence += ' ';
      sentence += FPSTR(WordClock::wordText(static_cast<WordClock::WordId>(plan.units[index].id)));
    }

    lastSentence = sentence;
  };

public:
  // Functions called by WLED

  ~WordClockUsermod() {
    if (ledMask)
      d_free(ledMask);
    if (wordMask)
      d_free(wordMask);
  }

  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup() {
    allocateLedMask();
  }

  /*
   * connected() is called every time the WiFi is (re)connected
   * Use it to initialize network interfaces
   */
  void connected() {
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
  void loop() {
    if (testHour < 0 && localTime == 0)
      return;

    const int currentMinute = testHour >= 0
      ? (testHour * 60 + testMinute) % 1440
      : (hour(localTime) * 60 + minute(localTime)) % 1440;

    if (currentMinute == lastRefreshMinute)
      return;

    updateLedMaskForCurrentTime();
    lastRefreshMinute = currentMinute;
  }

  /*
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
   * Below it is shown how this could be used for e.g. a light sensor
   */
  void addToJsonInfo(JsonObject& root) {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    // Current localTime so you can verify NTP has synced
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
             hour(localTime), minute(localTime), second(localTime));
    user[F("WClock localTime")] = timeBuf;

    // The sentence is built from the words selected for the configured display options.
    user[F("WClock sentence")] = lastSentence.isEmpty() ? F("(not computed yet)") : lastSentence;

    // How many matrix LEDs are currently lit
    int litCount = 0;

    if (ledMask) {
      for (int i = 0; i < (int)characterMatrix.length(); i++)
        if (ledMask[i]) litCount++;
    }

    user[F("WClock lit LEDs")] = litCount;
  }

  /*
   * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  void addToJsonState(JsonObject &root) {
  }

  /*
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  void readFromJsonState(JsonObject &root) {
  }

  /*
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
  void addToConfig(JsonObject &root) {
    JsonObject top = root.createNestedObject(F("Word Clock"));
    top[F("active")] = usermodActive;
    top[F("Display It Is")] = displayItIs;
    top[F("Led Offset")] = ledOffset;
  #if defined(WORD_CLOCK_LANGUAGE_DE)
    top[F("Norddeutsch")] = nord;
  #endif
    top[F("Brightness_Active")] = opacityActive;
    top[F("Brightness_Inactive")] = opacityInactive;
    top[F("meander")] = meander;
    top[F("Character_Matrix")] = characterMatrix;
    top[F("Character_Matrix_Width")] = characterMatrixWidth;
    top[F("Test_Hour")] = testHour;
    top[F("Test_Minute")] = testMinute;
  }

  void appendConfigData() {
    // Add hints for the Usermod Settings page, so the user knows what the settings mean
    oappend(F("addInfo('Word Clock:Brightness_Active', 1, '(0-255)');"));
    oappend(F("addInfo('Word Clock:Brightness_Inactive', 1, '(0-255)');"));
    oappend(F("addInfo('Word Clock:Led Offset', 1, 'Number of LEDs before the letters');"));
    oappend(F("addInfo('Word Clock:Test_Hour', 1, '(0-23, -1 for real time)');"));
    oappend(F("addInfo('Word Clock:Test_Minute', 1, '(0-59)');"));
  #if defined(WORD_CLOCK_LANGUAGE_DE)
    oappend(F("addInfo('Word Clock:Norddeutsch', 1, 'Viertel vor instead of Dreiviertel');"));
  #endif
  }

  /*
   * readFromConfig() can be used to read back the custom settings you added with addToConfig().
   * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
   *
   * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
   * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
   * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
   *
   * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
   *
   * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
   * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
   *
   * This function is guaranteed to be called on boot, but could also be called every time settings are updated
   */
  bool readFromConfig(JsonObject &root) {
    // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
    // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

    JsonObject top = root[F("Word Clock")];
    bool legacyConfig = top.isNull();
    JsonObject legacyTop = root[F("WordClockUsermod")];

    if (legacyConfig && !legacyTop.isNull()) {
      top = root.createNestedObject(F("Word Clock"));
    }

    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[F("active")], usermodActive);
    bool prevDisplayItIs = displayItIs;

    if (!getJsonValue(top[F("Display It Is")], displayItIs) &&
        getJsonValue(legacyTop[F("displayItIs")], displayItIs)) {
      top[F("Display It Is")] = displayItIs;
    }

    if (!getJsonValue(top[F("Led Offset")], ledOffset) &&
        getJsonValue(legacyTop[F("ledOffset")], ledOffset)) {
      top[F("Led Offset")] = ledOffset;
    }

  #if defined(WORD_CLOCK_LANGUAGE_DE)
    bool prevNord = nord;
    getJsonValue(top[F("Norddeutsch")], nord);
  #endif
    getJsonValue(top[F("Brightness_Active")], opacityActive);
    getJsonValue(top[F("Brightness_Inactive")], opacityInactive);
    opacityActive = clampInt(opacityActive, 0, 255);
    opacityInactive = clampInt(opacityInactive, 0, 255);

    if (displayItIs != prevDisplayItIs
  #if defined(WORD_CLOCK_LANGUAGE_DE)
        || nord != prevNord
  #endif
    ) {
      lastSentence = "";
      phraseMaskValid = false;
      lastRefreshMinute = -1;
    }

    bool prevMeander = meander;
    getJsonValue(top[F("meander")], meander);

    if (meander != prevMeander) {
      lastSentence = "";             // force mask recompute
      phraseMaskValid = false;
      lastRefreshMinute = -1; // trigger recompute on very next loop() call
    }

    String prevCharacterMatrix = characterMatrix;
    getJsonValue(top[F("Character_Matrix")], characterMatrix);

    if (!characterMatrix.equals(prevCharacterMatrix)) {
      allocateLedMask();

      lastSentence = "";             // force mask recompute
      phraseMaskValid = false;
      lastRefreshMinute = -1; // trigger recompute on very next loop() call
    }

    int prevCharacterMatrixWidth = characterMatrixWidth;
    getJsonValue(top[F("Character_Matrix_Width")], characterMatrixWidth);
    characterMatrixWidth = clampInt(characterMatrixWidth, WordClock::maxWordLength(), characterMatrix.length());

    if (characterMatrixWidth != prevCharacterMatrixWidth) {
      lastSentence = "";             // force mask recompute
      phraseMaskValid = false;
      lastRefreshMinute = -1; // trigger recompute on very next loop() call
    }

    int prevTestHour = testHour;
    int prevTestMinute = testMinute;
    getJsonValue(top[F("Test_Hour")], testHour);
    getJsonValue(top[F("Test_Minute")], testMinute);
    testHour = clampInt(testHour, -1, 23);
    testMinute = clampInt(testMinute, 0, 59);

    if (testHour != prevTestHour || testMinute != prevTestMinute) {
      lastSentence = "";               // force mask recompute
      phraseMaskValid = false;
      lastRefreshMinute = -1;  // trigger recompute on very next loop() call
    }

    return configComplete;
  }

  /*
   * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
   * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
   * Commonly used for custom clocks (Cronixie, 7 segment)
   */
  void handleOverlayDraw() {
    // Check if usermod is active
    if (!usermodActive)
      return;

    if (!ledMask)
      return;

    int matrixLen = (int)characterMatrix.length();

    // Loop over all leds
    for (int i = 0; i < matrixLen; i++) {
      int physIndex = ledOffset + i;
      uint32_t color = strip.getPixelColor(physIndex);
      // Scale by opacityActive for lit LEDs, opacityInactive for dimmed LEDs.
      int scale = ledMask[i] ? opacityActive : opacityInactive;
      uint8_t r = ((color >> 16) & 0xFF) * scale / 255;
      uint8_t g = ((color >>  8) & 0xFF) * scale / 255;
      uint8_t b = ((color >>  0) & 0xFF) * scale / 255;
      strip.setPixelColor(physIndex, RGBW32(r, g, b, 0));
    }
  }

  /*
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   * This could be used in the future for the system to determine whether your usermod is installed.
   */
  uint16_t getId() {
    return USERMOD_ID_WORDCLOCK;
  }

  // More methods can be added in the future, this example will then be extended.
  // Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};

static WordClockUsermod usermod_v2_word_clock;
REGISTER_USERMOD(usermod_v2_word_clock);
