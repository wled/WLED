#include "wled.h"

/*
 * Word Clock (Dutch)
 * This is a usermod for the WLED project that displays the time in words in
 * Dutch. It uses the WLED V2 usermod API to integrate with the WLED system and
 * apply an overlay on the LED strip to light up the characters representing
 * the current time in Dutch. This assumes that each LED of the LED strip is
 * arranged behind a matrix of characters, lighting up one character per LED,
 * and that the character matrix contains all words needed to display the time
 * in a Dutch sentence such as "HET IS KWART VOOR DRIE".
 * 
 * These settings are available in the Config > Usermods > Word Clock NL
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
 *   - `Matrix Char Offset`: the number of letters to skip at the beginning of
 *     the matrix when matching letters to LEDs. This is useful when the first
 *     physical LEDs do not correspond to the first letters.
 *   - `Test Hour`: the hour to display for testing, from 0 to 23. Set it to
 *     -1 to use the real time.
 *   - `Test Minute`: the minute to display for testing, from 0 to 59.
 */

class WordClockNlUsermod : public Usermod
{
private:
  // The matrix of characters that can be highlighted to display the time.
  // This matrix must contain all words needed to display the time in a
  // Dutch sentence, e.g. "HET IS KWART OVER TIEN".
  // The characters of each row are stored sequentially, and the rows are
  // stored sequentially as well, left to right, and top to bottom.
  String characterMatrix = "NEUEHETHETHTNFEYIEISISVTVIJFKWARTNAAAGBETIENOAEEAINOVERVOORATFUHALFIEENYOEHIBUZEVENVNBNMTWEEELFNDRIEVIERVIJFNEGENZESTIENTWAALFACHTNTBXNHWEUUROAD";

  // The number of characters per row
  int characterMatrixWidth = 12;

  // Is the ledstrip always from left to right on each row, or does it
  // meander through the rows (i.e. go from left to right on the first row,
  // then continue right to left on the second row, and so on)?
  bool meander = true;

  // Keep track of the last time our loop executed.
  // Initialised to trigger an update on the very first loop() call.
  unsigned long lastTime = ULONG_MAX - 60000UL;

  // ledMask[i] is true if the LED at index i should be on for the current time, and false if it should be off
  bool* ledMask = nullptr; 

  
  // The words for the hours in Dutch, used to construct the sentences
  // representing the time. 0 = twaalf uur, 1 = een uur, etc.
  // Use PROGMEM as per the coding guidelines for usermods.
  const char* sOne PROGMEM = "EEN";
  const char* sTwo PROGMEM = "TWEE";
  const char* sThree PROGMEM = "DRIE";
  const char* sFour PROGMEM = "VIER";
  const char* sFive PROGMEM = "VIJF";
  const char* sSix PROGMEM = "ZES";
  const char* sSeven PROGMEM = "ZEVEN";
  const char* sEight PROGMEM = "ACHT";
  const char* sNine PROGMEM = "NEGEN";
  const char* sTen PROGMEM = "TIEN";
  const char* sEleven PROGMEM = "ELF";
  const char* sTwelve PROGMEM = "TWAALF";

  // Other things needed to form complete sentences
  const char* sIt PROGMEM = "HET";
  const char* sIs PROGMEM = "IS";
  const char* sPast PROGMEM = "OVER";
  const char* sTo PROGMEM = "VOOR";
  const char* sHalf PROGMEM = "HALF";
  const char* sQuarter PROGMEM = "KWART";
  const char* sHour PROGMEM = "UUR";
  const char* sSpace PROGMEM = " ";

  const char* const HOUR_WORDS[12] PROGMEM = {sOne, sTwo, sThree, sFour, sFive, sSix, sSeven, sEight, sNine, sTen, sEleven, sTwelve};

  // Set your config variables to their boot default value (this can also be done in readFromConfig() or a constructor if you prefer)

  // Is this usermod active?
  bool usermodActive = false;

  // Number of leds before the first character
  int ledOffset = 0;

  // Opacity (0=off, 255=full brightness) applied to LEDs that ARE part of the current time sentence.
  int opacityActive = 255;

  // Opacity (0=off, 255=full brightness) applied to LEDs that are NOT part of the current time sentence.
  int opacityInactive = 0;

  // Number of characters to skip at the start of the matrix when mapping to physical LEDs.
  // E.g. set to 10 to skip the first row, so matrix char 10 maps to physical LED ledOffset+0.
  int matrixCharOffset = 0;

  // Test time override: set testHour (0‥23) and testMinute (0‥59) to force a specific time to be
  // displayed instead of the real time. Set testHour to -1 to disable (use real time).
  int testHour = -1;
  int testMinute = 0;

  /*
   * Get the Dutch sentence representing the given the time in minutes, e.g. "HET IS KWART OVER TIEN".
   * totalMinutes: 0.‥1439
   */
  String getSentenceForMinutes(int totalMinutes) {
    int h = (totalMinutes / 60) % 12;  // 0‥.11 (0 = twaalf uur)
    int m = totalMinutes % 60;

    // Round to nearest 5 minutes
    m = ((m + 2) / 5) * 5;

    // hourIndex: index of the current clock hour in HOUR_WORDS
    int hourIndex = (h - 1 + 12) % 12;

    // nextHourIndex: the hour after that (used for half/voor constructions)
    int nextHourIndex = h % 12;

    if (m ==  0) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[hourIndex]) + FPSTR(sSpace) + FPSTR(sHour);
    if (m ==  5) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sFive) + FPSTR(sSpace) + FPSTR(sPast) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[hourIndex]);
    if (m == 10) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sTen) + FPSTR(sSpace) + FPSTR(sPast) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[hourIndex]);
    if (m == 15) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sQuarter) + FPSTR(sSpace) + FPSTR(sPast) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[hourIndex]);
    if (m == 20) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sTen) + FPSTR(sSpace) + FPSTR(sTo) + FPSTR(sSpace) + FPSTR(sHalf) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);
    if (m == 25) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sFive) + FPSTR(sSpace) + FPSTR(sTo) + FPSTR(sSpace) + FPSTR(sHalf) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);
    if (m == 30) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sHalf) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);
    if (m == 35) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sFive) + FPSTR(sSpace) + FPSTR(sPast) + FPSTR(sSpace) + FPSTR(sHalf) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);
    if (m == 40) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sTen) + FPSTR(sSpace) + FPSTR(sPast) + FPSTR(sSpace) + FPSTR(sHalf) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);
    if (m == 45) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sQuarter) + FPSTR(sSpace) + FPSTR(sTo) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);
    if (m == 50) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sTen) + FPSTR(sSpace) + FPSTR(sTo) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);
    if (m == 55) return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(sFive) + FPSTR(sSpace) + FPSTR(sTo) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[nextHourIndex]);

    return (String) FPSTR(sIt) + FPSTR(sSpace) + FPSTR(sIs) + FPSTR(sSpace) + FPSTR(HOUR_WORDS[hourIndex]);
  }

  String lastSentence = "";

  /*
   * Update the ledMask for the current time, by setting ledMask[i] to true if
   * the LED at index i should be on for the current time, and false if it
   * should be off.
   */
  bool wordFitsInRow(int pos, int len) {
    return (pos / characterMatrixWidth) == ((pos + len - 1) / characterMatrixWidth);
  }

  // Clamp a value to the inclusive [lo, hi] range.
  int clampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  // Length of the longest word that can appear in a sentence; a row must be at least this wide to ever fit a word.
  int getMaxWordLength() {
    int maxLen = 0;

    for (int i = 0; i < 12; i++) {
      int len = strlen(HOUR_WORDS[i]);
      if (len > maxLen) maxLen = len;
    }

    const char* others[] = {sIt, sIs, sPast, sTo, sHalf, sQuarter, sHour};

    for (int i = 0; i < 7; i++) {
      int len = strlen(others[i]);
      if (len > maxLen) maxLen = len;
    }

    return maxLen;
  }

  void updateLedMaskForCurrentTime() {
    int nrOfLeds = characterMatrix.length();

    // Use test time if set, otherwise use the real local time
    int currentMinutes;

    if (testHour >= 0) {
      currentMinutes = (testHour * 60 + testMinute) % 1440;
    } else {
      currentMinutes = (hour(localTime) * 60 + minute(localTime)) % 1440;
    }

    // Get the sentence to display for the current time
    String sentence = getSentenceForMinutes(currentMinutes);

    if (sentence.equals(lastSentence))
      return; // No need to update the mask if the sentence hasn't changed since the last update

    // Remember the current sentence for the next update
    lastSentence = sentence;

    // Erase the ledMask before recomputing it
    if (ledMask)
      memset(ledMask, 0, nrOfLeds * sizeof(bool));

    // Split the sentence into words and for each word, find the next
    // occurrence of that word in the characterMatrix and update the
    // corresponding ledMask values.
    int searchFromIndex = 0;
    int sentenceLength = sentence.length();

    for (int i = 0; i < sentenceLength; ) {
      // Extract the next word from the sentence
      int nextSpaceIndex = sentence.indexOf(' ', i);

      if (nextSpaceIndex == -1)
        nextSpaceIndex = sentenceLength;

      String word = sentence.substring(i, nextSpaceIndex);
      i = nextSpaceIndex + 1;

      // Find the occurrence of the word to highlight.
      // "HET" and "IS" appear multiple times in the matrix for visual variety;
      // pick a random occurrence each update so the same physical LEDs are not
      // always lit for these two fixed words.
      // All other words are found sequentially (so e.g. "VIJF OVER VIJF" lights
      // the minute VIJF first, then the hour VIJF second).
      int wordIndex;
      bool advanceSearchFrom = true;

      if (word == FPSTR(sIt) || word == FPSTR(sIs)) {
        // Count how many times the word appears in the matrix, ignoring
        // occurrences that would be split across two rows
        int count = 0;
        int pos = 0;

        while ((pos = characterMatrix.indexOf(word, pos)) != -1) {
          if (wordFitsInRow(pos, word.length())) {
            count++;
            pos += word.length();
          } else {
            pos += 1;
          }
        }

        // Pick a random occurrence (stays at 0 when only one occurrence exists)
        int pick = (count > 1) ? (int)random(count) : 0;
        int seen = 0;
        pos = 0;
        wordIndex = -1;

        while ((pos = characterMatrix.indexOf(word, pos)) != -1) {
          if (wordFitsInRow(pos, word.length())) {
            if (seen == pick) {
              wordIndex = pos;
              break;
            }
            seen++;
            pos += word.length();
          } else {
            pos += 1;
          }
        }
        // Do not advance searchFromIndex: HET and IS are independent of word order
        advanceSearchFrom = false;
      } else {
        wordIndex = searchFromIndex;

        while ((wordIndex = characterMatrix.indexOf(word, wordIndex)) != -1) {
          if (wordFitsInRow(wordIndex, word.length()))
            break;
          wordIndex += 1;
        }
      }

      if (wordIndex == -1) {
        // This should never happen if the characterMatrix contains all words
        // needed to display the time in Dutch. Note that the words "VIJF" and
        // "TIEN" have to occur multiple times in the characterMatrix, e.g. to
        // display "VIJF OVER VIJF" or "TIEN VOOR TIEN".
        Serial.println("Error: word not found in characterMatrix: " + word);
        continue;
      }

      // Update the ledMask values for this word
      for (int j = 0; j < word.length(); j++) {
        int charIndex = wordIndex + j;

        if (charIndex >= (int)characterMatrix.length()) {
          Serial.println("Error: character index out of bounds: " + String(charIndex));
          continue;
        }

        if (!meander) {
          // If the ledstrip is always from left to right on each row, then the
          // character index is the same as the led index
          ledMask[charIndex] = true;
        } else {
          // If the ledstrip meanders through the rows, we need to convert the
          // character index to a led index
          int row = charIndex / characterMatrixWidth;
          int col = charIndex % characterMatrixWidth;
          int ledIndex;

          if (row % 2 == 0) {
            // Even row: left to right
            ledIndex = row * characterMatrixWidth + col;
          } else {
            // Odd row: right to left
            ledIndex = row * characterMatrixWidth + (characterMatrixWidth - 1 - col);
          }

          ledMask[ledIndex] = true;
        }
      }

      if (advanceSearchFrom)
        searchFromIndex = wordIndex + word.length();
    }
  };

public:
  // Functions called by WLED

  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup() {
    // Initialize the ledMask with false values
    ledMask = (bool*) d_malloc(characterMatrix.length() * sizeof(bool));

    if (ledMask)
      memset(ledMask, 0, characterMatrix.length() * sizeof(bool));
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
    // Execute only once per minute
    if (millis() - lastTime < 60 * 1000)
      return;

    updateLedMaskForCurrentTime();

    // Remember this update
    lastTime = millis();
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

    // The Dutch sentence currently displayed. Skip the constant "HET IS " prefix (7 chars)
    // so the two visible characters in the UI show the variable part (e.g. "TIEN OVER DRIE").
    user[F("WClock sentence")] = lastSentence.length() > 7 ? lastSentence.substring(7) : (lastSentence.isEmpty() ? F("(not computed yet)") : lastSentence);

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
    JsonObject top = root.createNestedObject(F("Word Clock NL"));
    top[F("active")] = usermodActive;
    top[F("Brightness_Active")] = opacityActive;
    top[F("Brightness_Inactive")] = opacityInactive;
    top[F("meander")] = meander;
    top[F("Character_Matrix")] = characterMatrix;
    top[F("Character_Matrix_Width")] = characterMatrixWidth;
    top[F("Matrix_Char_Offset")] = matrixCharOffset;
    top[F("Test_Hour")] = testHour;
    top[F("Test_Minute")] = testMinute;
  }

  void appendConfigData() {
    // Add hints for the Usermod Settings page, so the user knows what the settings mean
    oappend(F("addInfo('Word Clock NL:Brightness_Active', 1, '(0-255)');"));
    oappend(F("addInfo('Word Clock NL:Brightness_Inactive', 1, '(0-255)');"));
    oappend(F("addInfo('Word Clock NL:Test_Hour', 1, '(0-23, -1 for real time)');"));
    oappend(F("addInfo('Word Clock NL:Test_Minute', 1, '(0-59)');"));
    // oappend(F("addInfo('Word clock NL:ledOffset', 1, 'Number of LEDs before the letters');"));
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

    JsonObject top = root[F("Word Clock NL")];

    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[F("active")], usermodActive);
    getJsonValue(top[F("Brightness_Active")], opacityActive);
    getJsonValue(top[F("Brightness_Inactive")], opacityInactive);
    opacityActive = clampInt(opacityActive, 0, 255);
    opacityInactive = clampInt(opacityInactive, 0, 255);
    bool prevMeander = meander;
    getJsonValue(top[F("meander")], meander);

    if (meander != prevMeander) {
      lastSentence = "";             // force mask recompute
      lastTime = ULONG_MAX - 60000UL; // trigger recompute on very next loop() call
    }

    String prevCharacterMatrix = characterMatrix;
    getJsonValue(top[F("Character_Matrix")], characterMatrix);

    if (!characterMatrix.equals(prevCharacterMatrix)) {
      // Size may have changed, so the ledMask buffer must be reallocated
      if (ledMask) {
        free(ledMask);
        ledMask = nullptr;
      }

      ledMask = (bool*) d_malloc(characterMatrix.length() * sizeof(bool));

      if (ledMask)
        memset(ledMask, 0, characterMatrix.length() * sizeof(bool));

      lastSentence = "";             // force mask recompute
      lastTime = ULONG_MAX - 60000UL; // trigger recompute on very next loop() call
    }

    int prevCharacterMatrixWidth = characterMatrixWidth;
    getJsonValue(top[F("Character_Matrix_Width")], characterMatrixWidth);
    characterMatrixWidth = clampInt(characterMatrixWidth, getMaxWordLength(), characterMatrix.length());

    if (characterMatrixWidth != prevCharacterMatrixWidth) {
      lastSentence = "";             // force mask recompute
      lastTime = ULONG_MAX - 60000UL; // trigger recompute on very next loop() call
    }

    getJsonValue(top[F("Matrix_Char_Offset")], matrixCharOffset);
    matrixCharOffset = clampInt(matrixCharOffset, 0, characterMatrix.length());

    int prevTestHour = testHour;
    int prevTestMinute = testMinute;
    getJsonValue(top[F("Test_Hour")], testHour);
    getJsonValue(top[F("Test_Minute")], testMinute);
    testHour = clampInt(testHour, -1, 23);
    testMinute = clampInt(testMinute, 0, 59);

    if (testHour != prevTestHour || testMinute != prevTestMinute) {
      lastSentence = "";               // force mask recompute
      lastTime = ULONG_MAX - 60000UL;  // trigger recompute on very next loop() call
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
    for (int i = matrixCharOffset; i < matrixLen; i++) {
      int physIndex = ledOffset + i - matrixCharOffset;
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
    return USERMOD_ID_WORDCLOCK_NL;
  }

  // More methods can be added in the future, this example will then be extended.
  // Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};

static WordClockNlUsermod usermod_v2_word_clock_nl;
REGISTER_USERMOD(usermod_v2_word_clock_nl);
