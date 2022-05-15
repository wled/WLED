#include "wled.h"

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

//gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup()
{
  
}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected()
{

}

/*
 * Physical IO
 */
#define PIN_UP_RELAY 4
#define PIN_DN_RELAY 5
#define PIN_ON_TIME  500
bool upActive = false, upActiveBefore = false, downActive = false, downActiveBefore = false;
unsigned long upStartTime = 0, downStartTime = 0;

void handleRelay()
{
  //up and down relays
  if (userVar0) {
    upActive = true;
    if (userVar0 == 1) {
      upActive = false;
      downActive = true;
    }
    userVar0 = 0;
  }
  
  if (upActive)
  {
    if(!upActiveBefore)
    {
      pinMode(PIN_UP_RELAY, OUTPUT);
      digitalWrite(PIN_UP_RELAY, LOW);
      upActiveBefore = true;
      upStartTime = millis();
      DEBUG_PRINTLN("UPA");
    }
    if (millis()- upStartTime > PIN_ON_TIME)
    {
      upActive = false;
      DEBUG_PRINTLN("UPN");
    }
  } else if (upActiveBefore)
  {
    pinMode(PIN_UP_RELAY, INPUT);
    upActiveBefore = false;
  }

  if (downActive)
  {
    if(!downActiveBefore)
    {
      pinMode(PIN_DN_RELAY, OUTPUT);
      digitalWrite(PIN_DN_RELAY, LOW);
      downActiveBefore = true;
      downStartTime = millis();
    }
    if (millis()- downStartTime > PIN_ON_TIME)
    {
      downActive = false;
    }
  } else if (downActiveBefore)
  {
    pinMode(PIN_DN_RELAY, INPUT);
    downActiveBefore = false;
  }
}

//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop()
{
  handleRelay();
}