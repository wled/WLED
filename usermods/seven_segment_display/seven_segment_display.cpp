#include "wled.h"

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

class SevenSegmentDisplay : public Usermod
{

#define WLED_SS_BUFFLEN 6
#define REFRESHTIME 497
private:
  //Runtime variables.
  unsigned long lastRefresh = 0;
  unsigned long lastCharacterStep = 0;
  String ssDisplayBuffer = "";
  char ssCharacterMask[36] = {0x77, 0x11, 0x6B, 0x3B, 0x1D, 0x3E, 0x7E, 0x13, 0x7F, 0x1F, 0x5F, 0x7C, 0x66, 0x79, 0x6E, 0x4E, 0x76, 0x5D, 0x44, 0x71, 0x5E, 0x64, 0x27, 0x58, 0x77, 0x4F, 0x1F, 0x48, 0x3E, 0x6C, 0x75, 0x25, 0x7D, 0x2A, 0x3D, 0x6B};
  int ssDisplayMessageIdx = 0; //Position of the start of the message to be physically displayed.
  bool ssDoDisplayTime = true;
  int ssVirtualDisplayMessageIdxStart = 0;
  int ssVirtualDisplayMessageIdxEnd = 0;
  unsigned long resfreshTime = 497;

  // set your config variables to their boot default value (this can also be done in readFromConfig() or a constructor if you prefer)
  int ssLEDPerSegment = 1; //The number of LEDs in each segment of the 7 seg (total per digit is 7 * ssLedPerSegment)
  int ssLEDPerPeriod = 1;  //A Period will have 1x and a Colon will have 2x
  int ssStartLED = 0;      //The pixel that the display starts at.
  /*  HH - 0-23. hh - 1-12, kk - 1-24 hours
    //  MM or mm - 0-59 minutes
    //  SS or ss = 0-59 seconds
    //  : for a colon
    //  All others for alpha numeric, (will be blank when displaying time)
    */
  String ssDisplayMask = "HHMMSS"; //Physical Display Mask, this should reflect physical equipment.
  /* ssDisplayConfig
    //           -------
    //         /   A   /          0 - EDCGFAB
    //        / F     / B         1 - EDCBAFG
    //       /       /            2 - GCDEFAB
    //       -------              3 - GBAFEDC
    //     /   G   /              4 - FABGEDC
    //    / E     / C             5 - FABCDEG
    //   /       /
    //   -------
    //      D
    */
  int ssDisplayConfig = 5; //Physical configuration of the Seven segment display
  String ssDisplayMessage = "~";
  bool ssTimeEnabled = true;         //If not, display message.
  unsigned int ssScrollSpeed = 1000; //Time between advancement of extended message scrolling, in milliseconds.

  //String to reduce flash memory usage
  static const char _str_perSegment[];
  static const char _str_perPeriod[];
  static const char _str_startIdx[];
  static const char _str_displayCfg[];
  static const char _str_timeEnabled[];
  static const char _str_scrollSpd[];
  static const char _str_displayMask[];
  static const char _str_displayMsg[];
  static const char _str_sevenSeg[];
  static const char _str_subFormat[];
  static const char _str_topicFormat[];

  unsigned long _overlaySevenSegmentProcess()
  {
    //Do time for now.
    if (ssDoDisplayTime)
    {
      //Format the ssDisplayBuffer based on ssDisplayMask
      int displayMaskLen = static_cast<int>(ssDisplayMask.length());
      for (int index = 0; index < displayMaskLen; index++)
      {
        //Only look for time formatting if there are at least 2 characters left in the buffer.
        if ((index < displayMaskLen - 1) && (ssDisplayMask[index] == ssDisplayMask[index + 1]))
        {
          int timeVar = 0;
          switch (ssDisplayMask[index])
          {
          case 'h':
            timeVar = hourFormat12(localTime);
            break;
          case 'H':
            timeVar = hour(localTime);
            break;
          case 'k':
            timeVar = hour(localTime) + 1;
            break;
          case 'M':
          case 'm':
            timeVar = minute(localTime);
            break;
          case 'S':
          case 's':
            timeVar = second(localTime);
            break;
          }

          //Only want to leave a blank in the hour formatting.
          if ((ssDisplayMask[index] == 'h' || ssDisplayMask[index] == 'H' || ssDisplayMask[index] == 'k') && timeVar < 10)
            ssDisplayBuffer[index] = ' ';
          else
            ssDisplayBuffer[index] = 0x30 + (timeVar / 10);
          ssDisplayBuffer[index + 1] = 0x30 + (timeVar % 10);

          //Need to increment the index because of the second digit.
          index++;
        }
        else
        {
          ssDisplayBuffer[index] = (ssDisplayMask[index] == ':' ? ':' : ' ');
        }
      }
      return REFRESHTIME;
    }
    else
    {
      /* This will handle displaying a message and the scrolling of the message if its longer than the buffer length */

      //Check to see if the message has scrolled completely
      int len = static_cast<int>(ssDisplayMessage.length());
      if (ssDisplayMessageIdx > len)
      {
        //If it has scrolled the whole message, reset it.
        setSevenSegmentMessage(ssDisplayMessage);
        return REFRESHTIME;
      }
      //Display message
      int displayMaskLen = static_cast<int>(ssDisplayMask.length());
      for (int index = 0; index < displayMaskLen; index++)
      {
        if (ssDisplayMessageIdx + index < len && ssDisplayMessageIdx + index >= 0)
          ssDisplayBuffer[index] = ssDisplayMessage[ssDisplayMessageIdx + index];
        else
          ssDisplayBuffer[index] = ' ';
      }

      //Increase the displayed message index to progress it one character if the length exceeds the display length.
      if (len > displayMaskLen)
        ssDisplayMessageIdx++;

      return ssScrollSpeed;
    }
  }

  void _overlaySevenSegmentDraw()
  {

    //Start pixels at ssStartLED, Use ssLEDPerSegment, ssLEDPerPeriod, ssDisplayBuffer
    int indexLED = ssStartLED;
    int displayMaskLen = static_cast<int>(ssDisplayMask.length());
    for (int indexBuffer = 0; indexBuffer < displayMaskLen; indexBuffer++)
    {
      if (ssDisplayBuffer[indexBuffer] == 0)
        break;
      else if (ssDisplayBuffer[indexBuffer] == '.')
      {
        //Won't ever turn off LED lights for a period. (or will we?)
        indexLED += ssLEDPerPeriod;
        continue;
      }
      else if (ssDisplayBuffer[indexBuffer] == ':')
      {
        //Turn off colon if odd second?
        indexLED += ssLEDPerPeriod * 2;
      }
      else if (ssDisplayBuffer[indexBuffer] == ' ')
      {
        //Turn off all 7 segments.
        _overlaySevenSegmentLEDOutput(0, indexLED);
        indexLED += ssLEDPerSegment * 7;
      }
      else
      {
        //Turn off correct segments.
        _overlaySevenSegmentLEDOutput(_overlaySevenSegmentGetCharMask(ssDisplayBuffer[indexBuffer]), indexLED);
        indexLED += ssLEDPerSegment * 7;
      }
    }
  }

  void _overlaySevenSegmentLEDOutput(char mask, int indexLED)
  {
    for (char index = 0; index < 7; index++)
    {
      if ((mask & (0x40 >> index)) != (0x40 >> index))
      {
        for (int numPerSeg = 0; numPerSeg < ssLEDPerSegment; numPerSeg++)
        {
          strip.setPixelColor(indexLED + numPerSeg, 0x000000);
        }
      }
      indexLED += ssLEDPerSegment;
    }
  }

  char _overlaySevenSegmentGetCharMask(char var)
  {
    if (var >= 0x30 && var <= 0x39)
    { /*If its a number, shift to index 0.*/
      var -= 0x30;
    }
    else if (var >= 0x41 && var <= 0x5a)
    { /*If its an Upper case, shift to index 0xA.*/
      var -= 0x37;
    }
    else if (var >= 0x61 && var <= 0x7A)
    { /*If its a lower case, shift to index 0xA.*/
      var -= 0x57;
    }
    else
    { /* Else unsupported, return 0; */
      return 0;
    }
    char mask = ssCharacterMask[static_cast<int>(var)];
    /*
      0 - EDCGFAB
      1 - EDCBAFG
      2 - GCDEFAB
      3 - GBAFEDC
      4 - FABGEDC
      5 - FABCDEG
      */
    switch (ssDisplayConfig)
    {
    case 1:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 3, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 1, 2, 1);
      break;
    case 2:
      mask = _overlaySevenSegmentSwapBits(mask, 3, 6, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 4, 5, 1);
      break;
    case 3:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 4, 3);
      mask = _overlaySevenSegmentSwapBits(mask, 3, 6, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 4, 5, 1);
      break;
    case 4:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 4, 3);
      break;
    case 5:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 4, 3);
      mask = _overlaySevenSegmentSwapBits(mask, 0, 3, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 1, 2, 1);
      break;
    }
    return mask;
  }

  char _overlaySevenSegmentSwapBits(char x, char p1, char p2, char n)
  {
    /* Move all bits of first set to rightmost side */
    char set1 = (x >> p1) & ((1U << n) - 1);

    /* Move all bits of second set to rightmost side */
    char set2 = (x >> p2) & ((1U << n) - 1);

    /* Xor the two sets */
    char Xor = (set1 ^ set2);

    /* Put the Xor bits back to their original positions */
    Xor = (Xor << p1) | (Xor << p2);

    /* Xor the 'Xor' with the original number so that the 
        two sets are swapped */
    char result = x ^ Xor;

    return result;
  }

  void _publishMQTTint_P(const char *subTopic, int value)
  {
    if(mqtt == NULL) return;
      
    char buffer[64];
    char valBuffer[12];
    sprintf_P(buffer, PSTR("%s/%S/%S"), mqttDeviceTopic, _str_sevenSeg, subTopic);
    sprintf_P(valBuffer, PSTR("%d"), value);
    mqtt->publish(buffer, 2, true, valBuffer);
  }

  void _publishMQTTstr_P(const char *subTopic, String Value)
  {
    if(mqtt == NULL) return;
    char buffer[64];
    sprintf_P(buffer, PSTR("%s/%S/%S"), mqttDeviceTopic, _str_sevenSeg, subTopic);
    mqtt->publish(buffer, 2, true, Value.c_str(), Value.length());
  }

  void _updateMQTT()
  {
    _publishMQTTint_P(_str_perSegment, ssLEDPerSegment);
    _publishMQTTint_P(_str_perPeriod, ssLEDPerPeriod);
    _publishMQTTint_P(_str_startIdx, ssStartLED);
    _publishMQTTint_P(_str_displayCfg, ssDisplayConfig);
    _publishMQTTint_P(_str_timeEnabled, ssTimeEnabled);
    _publishMQTTint_P(_str_scrollSpd, ssScrollSpeed);

    _publishMQTTstr_P(_str_displayMask, ssDisplayMask);
    _publishMQTTstr_P(_str_displayMsg, ssDisplayMessage);
  }

  bool _cmpIntSetting_P(char *topic, char *payload, const char *setting, void *value)
  {
    if (strcmp_P(topic, setting) == 0)
    {
      *((int *)value) = strtol(payload, NULL, 10);
      _publishMQTTint_P(setting, *((int *)value));
      return true;
    }
    return false;
  }

  bool _handleSetting(char *topic, char *payload)
  {
    if (_cmpIntSetting_P(topic, payload, _str_perSegment, &ssLEDPerSegment))
      return true;
    if (_cmpIntSetting_P(topic, payload, _str_perPeriod, &ssLEDPerPeriod))
      return true;
    if (_cmpIntSetting_P(topic, payload, _str_startIdx, &ssStartLED))
      return true;
    if (_cmpIntSetting_P(topic, payload, _str_displayCfg, &ssDisplayConfig))
      return true;
    if (_cmpIntSetting_P(topic, payload, _str_timeEnabled, &ssTimeEnabled))
      return true;
    if (_cmpIntSetting_P(topic, payload, _str_scrollSpd, &ssScrollSpeed))
      return true;
    if (strcmp_P(topic, _str_displayMask) == 0)
    {
      ssDisplayMask = String(payload);
      ssDisplayBuffer = ssDisplayMask;
      _publishMQTTstr_P(_str_displayMask, ssDisplayMask);
      return true;
    }
    if (strcmp_P(topic, _str_displayMsg) == 0)
    {
      setSevenSegmentMessage(String(payload));
      return true;
    }
    return false;
  }

public:
  void setSevenSegmentMessage(String message)
  {
    //If the message isn't blank display it otherwise show time, if enabled.
    if (message.length() < 1 || message == "~")
      ssDoDisplayTime = ssTimeEnabled;
    else
      ssDoDisplayTime = false;

    //Determine is the message is longer than the display, if it is configure it to scroll the message.
    if (message.length() > ssDisplayMask.length())
      ssDisplayMessageIdx = -ssDisplayMask.length();
    else
      ssDisplayMessageIdx = 0;

    //If the message isn't the same, update runtime/mqtt (most calls will be resetting message scroll)
    if (!ssDisplayMessage.equals(message))
    {
      _publishMQTTstr_P(_str_displayMsg, message);
      ssDisplayMessage = message;
    }
  }
  //Functions called by WLED

  /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
  void setup()
  {
    ssDisplayBuffer = ssDisplayMask;
  }

  /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     */
  void loop()
  {
    if (millis() - lastRefresh > resfreshTime)
    {
      //In theory overlaySevenSegmentProcess should return the amount of time until it changes next.
      //So we should be okay to trigger the stripi on every process loop.
      resfreshTime = _overlaySevenSegmentProcess();
      lastRefresh = millis();
      strip.trigger();
    }
  }

  void handleOverlayDraw()
  {
    _overlaySevenSegmentDraw();
  }

  void onMqttConnect(bool sessionPresent)
  {
    char subBuffer[48];
    if (mqttDeviceTopic[0] != 0)
    {
      _updateMQTT();
      //subscribe for sevenseg messages on the device topic
      sprintf_P(subBuffer, PSTR("%s/%S/+/set"), mqttDeviceTopic, _str_sevenSeg);
      mqtt->subscribe(subBuffer, 2);
    }

    if (mqttGroupTopic[0] != 0)
    {
      //subscribe for sevenseg messages on the group topic
      sprintf_P(subBuffer, PSTR("%s/%S/+/set"), mqttGroupTopic, _str_sevenSeg);
      mqtt->subscribe(subBuffer, 2);
    }
  }

  bool onMqttMessage(char *topic, char *payload)
  {
    //If topic beings with sevenSeg cut it off, otherwise not our message.
    size_t topicPrefixLen = strlen_P(PSTR("/sevenSeg/"));
    if (strncmp_P(topic, PSTR("/sevenSeg/"), topicPrefixLen) == 0)
      topic += topicPrefixLen;
    else
      return false;
    //We only care if the topic ends with /set
    size_t topicLen = strlen(topic);
    if (topicLen > 4 &&
        topic[topicLen - 4] == '/' &&
        topic[topicLen - 3] == 's' &&
        topic[topicLen - 2] == 'e' &&
        topic[topicLen - 1] == 't')
    {
      //Trim /set and handle it
      topic[topicLen - 4] = '\0';
      _handleSetting(topic, payload);
    }
    return true;
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root[FPSTR(_str_sevenSeg)];
    if (top.isNull())
    {
      top = root.createNestedObject(FPSTR(_str_sevenSeg));
    }
    top[FPSTR(_str_perSegment)] = ssLEDPerSegment;
    top[FPSTR(_str_perPeriod)] = ssLEDPerPeriod;
    top[FPSTR(_str_startIdx)] = ssStartLED;
    top[FPSTR(_str_displayMask)] = ssDisplayMask;
    top[FPSTR(_str_displayCfg)] = ssDisplayConfig;
    top[FPSTR(_str_displayMsg)] = ssDisplayMessage;
    top[FPSTR(_str_timeEnabled)] = ssTimeEnabled;
    top[FPSTR(_str_scrollSpd)] = ssScrollSpeed;
  }

  bool readFromConfig(JsonObject &root)
  {
    JsonObject top = root[FPSTR(_str_sevenSeg)];

    bool configComplete = !top.isNull();

    //if sevenseg section doesn't exist return
    if (!configComplete)
      return configComplete;

    configComplete &= getJsonValue(top[FPSTR(_str_perSegment)], ssLEDPerSegment);
    configComplete &= getJsonValue(top[FPSTR(_str_perPeriod)], ssLEDPerPeriod);
    configComplete &= getJsonValue(top[FPSTR(_str_startIdx)], ssStartLED);
    configComplete &= getJsonValue(top[FPSTR(_str_displayMask)], ssDisplayMask);
    configComplete &= getJsonValue(top[FPSTR(_str_displayCfg)], ssDisplayConfig);

    String newDisplayMessage;
    configComplete &= getJsonValue(top[FPSTR(_str_displayMsg)], newDisplayMessage);
    setSevenSegmentMessage(newDisplayMessage);

    configComplete &= getJsonValue(top[FPSTR(_str_timeEnabled)], ssTimeEnabled);
    configComplete &= getJsonValue(top[FPSTR(_str_scrollSpd)], ssScrollSpeed);
    return configComplete;
  }

  /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
  uint16_t getId()
  {
    return USERMOD_ID_SEVEN_SEGMENT_DISPLAY;
  }
};

const char SevenSegmentDisplay::_str_perSegment[] PROGMEM = "perSegment";
const char SevenSegmentDisplay::_str_perPeriod[] PROGMEM = "perPeriod";
const char SevenSegmentDisplay::_str_startIdx[] PROGMEM = "startIdx";
const char SevenSegmentDisplay::_str_displayCfg[] PROGMEM = "displayCfg";
const char SevenSegmentDisplay::_str_timeEnabled[] PROGMEM = "timeEnabled";
const char SevenSegmentDisplay::_str_scrollSpd[] PROGMEM = "scrollSpd";
const char SevenSegmentDisplay::_str_displayMask[] PROGMEM = "displayMask";
const char SevenSegmentDisplay::_str_displayMsg[] PROGMEM = "displayMsg";
const char SevenSegmentDisplay::_str_sevenSeg[] PROGMEM = "sevenSeg";

static SevenSegmentDisplay seven_segment_display;
REGISTER_USERMOD(seven_segment_display);