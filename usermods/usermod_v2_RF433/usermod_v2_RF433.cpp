#include "wled.h"
#include "Arduino.h"
#include <RCSwitch.h>

#define RF433_BUSWAIT_TIMEOUT 24

class RF433Usermod : public Usermod
{
private:
  RCSwitch mySwitch = RCSwitch();
  unsigned long lastCommand = 0;
  unsigned long lastTime = 0;

  bool modEnabled = true;
  int8_t receivePin = -1;

  static const char _modName[];
  static const char _modEnabled[];
  static const char _receivePin[];

  bool initDone = false;

public:

  void setup()
  {
    mySwitch.disableReceive();
    if (modEnabled)
    {
      mySwitch.enableReceive(receivePin);
    }
    initDone = true;
  }

  /*
   * connected() is called every time the WiFi is (re)connected
   * Use it to initialize network interfaces
   */
  void connected()
  {
  }

  void loop()
  {
    if (!modEnabled || strip.isUpdating()) 
      return;

    if (mySwitch.available())
    {
      unsigned long receivedCommand = mySwitch.getReceivedValue();
      mySwitch.resetAvailable();

      // Discard duplicates, limit long press repeat
      if (lastCommand == receivedCommand && millis() - lastTime < 800)
        return;

      lastCommand = receivedCommand;
      lastTime = millis();

      DEBUG_PRINT(F("RF433 Receive: "));
      DEBUG_PRINTLN(receivedCommand);
      
      if(!remoteJson433(receivedCommand))
        DEBUG_PRINTLN(F("RF433: unknown button"));
    }
  }

  // Add last received button to info pane
  void addToJsonInfo(JsonObject &root)
  {
    if (!initDone)
      return; // prevent crash on boot applyPreset()
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    JsonArray switchArr = user.createNestedArray("RF433 Last Received"); // name
    switchArr.add(lastCommand);
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject(FPSTR(_modName)); // usermodname
    top[FPSTR(_modEnabled)] = modEnabled;
    JsonArray pinArray = top.createNestedArray("pin");
    pinArray.add(receivePin);

    DEBUG_PRINTLN(F(" config saved."));
  }

  bool readFromConfig(JsonObject &root)
  {
    JsonObject top = root[FPSTR(_modName)];
    if (top.isNull())
    {
      DEBUG_PRINT(FPSTR(_modName));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    getJsonValue(top[FPSTR(_modEnabled)], modEnabled);
    getJsonValue(top["pin"][0], receivePin);

    DEBUG_PRINTLN(F("config (re)loaded."));

    // Redo init on update
    if(initDone)
      setup();

    return true;
  }

  /*
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   * This could be used in the future for the system to determine whether your usermod is installed.
   */
  uint16_t getId()
  {
    return USERMOD_ID_RF433;
  }

  // this function follows the same principle as decodeIRJson() / remoteJson()
  bool remoteJson433(int button)
  {
    char objKey[14];
    bool parsed = false;

    if (!requestJSONBufferLock(22)) return false;

    sprintf_P(objKey, PSTR("\"%d\":"), button);

    unsigned long start = millis();
    while (strip.isUpdating() && millis()-start < RF433_BUSWAIT_TIMEOUT) yield(); // wait for strip to finish updating, accessing FS during sendout causes glitches

    // attempt to read command from remote.json
    readObjectFromFile(PSTR("/remote433.json"), objKey, pDoc);
    JsonObject fdo = pDoc->as<JsonObject>();
    if (fdo.isNull()) {
      // the received button does not exist
      releaseJSONBufferLock();
      return parsed;
    }

    String cmdStr = fdo["cmd"].as<String>();
    JsonObject jsonCmdObj = fdo["cmd"]; //object

    if (jsonCmdObj.isNull())  // we could also use: fdo["cmd"].is<String>()
    {
      // HTTP API command
      String apireq = "win"; apireq += '&';                        // reduce flash string usage
      if (!cmdStr.startsWith(apireq)) cmdStr = apireq + cmdStr;    // if no "win&" prefix
      if (!irApplyToAllSelected && cmdStr.indexOf(F("SS="))<0) {
        char tmp[10];
        sprintf_P(tmp, PSTR("&SS=%d"), strip.getMainSegmentId());
        cmdStr += tmp;
      }
      fdo.clear();                                                 // clear JSON buffer (it is no longer needed)
      handleSet(nullptr, cmdStr, false);                           // no stateUpdated() call here
      stateUpdated(CALL_MODE_BUTTON);
      parsed = true;
    } else {
    // command is JSON object
      if (jsonCmdObj[F("psave")].isNull())
        deserializeState(jsonCmdObj, CALL_MODE_BUTTON_PRESET);
      else {
        uint8_t psave = jsonCmdObj[F("psave")].as<int>();
        char pname[33];
        sprintf_P(pname, PSTR("IR Preset %d"), psave);
        fdo.clear();
        if (psave > 0 && psave < 251) savePreset(psave, pname, fdo);
      }
      parsed = true;
    }
    releaseJSONBufferLock();
    return parsed;
  }
};

const char RF433Usermod::_modName[]          PROGMEM = "RF433 Remote";
const char RF433Usermod::_modEnabled[]       PROGMEM = "Enabled";
const char RF433Usermod::_receivePin[]       PROGMEM = "RX Pin";

static RF433Usermod usermod_v2_RF433;
REGISTER_USERMOD(usermod_v2_RF433);
