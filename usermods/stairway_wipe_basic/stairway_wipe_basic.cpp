#include "wled.h"

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/wled-dev/WLED/wiki/Add-own-functionality
 * 
 * This is Stairway-Wipe as a v2 usermod.
 * 
 * Using this usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "stairway-wipe-usermod-v2.h" in the top and registerUsermod(new StairwayWipeUsermod()) in the bottom of usermods_list.cpp
 */

class StairwayWipeUsermod : public Usermod {
  private:
    //Private class members. You can declare variables and functions only accessible to your usermod here
    unsigned long lastTime = 0;
    byte wipeState = 0; //0: inactive 1: wiping 2: solid
    unsigned long timeStaticStart = 0;
    uint16_t previousUserVar0 = 0;

//moved to buildflag
//comment this out if you want the turn off effect to be just fading out instead of reverse wipe
//#define STAIRCASE_WIPE_OFF
  public:
void setup() {
    }
    void loop() {
  //userVar0 (U0 in HTTP API):
  //has to be set to 1 if movement is detected on the PIR that is the same side of the staircase as the ESP8266
  //has to be set to 2 if movement is detected on the PIR that is the opposite side
  //can be set to 0 if no movement is detected. Otherwise LEDs will turn off after a configurable timeout (userVar1 seconds)

  if (userVar0 > 0)
  {
    if ((previousUserVar0 == 1 && userVar0 == 2) || (previousUserVar0 == 2 && userVar0 == 1)) wipeState = 3; //turn off if other PIR triggered
    previousUserVar0 = userVar0;
    
    if (wipeState == 0) {
      startWipe();
      wipeState = 1;
    } else if (wipeState == 1) { //wiping
      uint32_t cycleTime = 360 + (255 - effectSpeed)*75; //this is how long one wipe takes (minus 25 ms to make sure we switch in time)
      if (millis() + strip.timebase > (cycleTime - 25)) { //wipe complete
        effectCurrent = FX_MODE_STATIC;
        timeStaticStart = millis();
        colorUpdated(CALL_MODE_NOTIFICATION);
        wipeState = 2;
      }
    } else if (wipeState == 2) { //static
      if (userVar1 > 0) //if U1 is not set, the light will stay on until second PIR or external command is triggered
      {
        if (millis() - timeStaticStart > userVar1*1000) wipeState = 3;
      }
    } else if (wipeState == 3) { //switch to wipe off
      #ifdef STAIRCASE_WIPE_OFF
      effectCurrent = FX_MODE_COLOR_WIPE;
      strip.timebase = 360 + (255 - effectSpeed)*75 - millis(); //make sure wipe starts fully lit
      colorUpdated(CALL_MODE_NOTIFICATION);
      wipeState = 4;
      #else
      turnOff();
      #endif
    } else { //wiping off
      if (millis() + strip.timebase > (725 + (255 - effectSpeed)*150)) turnOff(); //wipe complete
    }
  } else {
    wipeState = 0; //reset for next time
    if (previousUserVar0) {
      #ifdef STAIRCASE_WIPE_OFF
      userVar0 = previousUserVar0;
      wipeState = 3;
      #else
      turnOff();
      #endif
    }
    previousUserVar0 = 0;
  }
}

    void readFromJsonState(JsonObject& root)
    {
      userVar0 = root["user0"] | userVar0; //if "user0" key exists in JSON, update, else keep old value
      //if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
    }

    uint16_t getId()
    {
      return USERMOD_ID_STAIRWAY_WIPE;
    }


    void startWipe()
    {
    bri = briLast; //turn on
    jsonTransitionOnce = true;
    strip.setTransition(0); //no transition
    effectCurrent = FX_MODE_COLOR_WIPE;
    strip.resetTimebase(); //make sure wipe starts from beginning

    //set wipe direction
    Segment& seg = strip.getSegment(0);
    bool doReverse = (userVar0 == 2);
    seg.setOption(1, doReverse);

    colorUpdated(CALL_MODE_NOTIFICATION);
    }

    void turnOff()
    {
    jsonTransitionOnce = true;
    #ifdef STAIRCASE_WIPE_OFF
    strip.setTransition(0); //turn off immediately after wipe completed
    #else
    strip.setTransition(4000); //fade out slowly
    #endif
    bri = 0;
    stateUpdated(CALL_MODE_NOTIFICATION);
    wipeState = 0;
    userVar0 = 0;
    previousUserVar0 = 0;
    }



   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};


static StairwayWipeUsermod stairway_wipe_basic;
REGISTER_USERMOD(stairway_wipe_basic);