//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

long lastTime = 0;
int delayMs = 10;
const int pinA = D6; //data
const int pinB = D7; //clk
int oldA = LOW;

//gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup() {
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected() {
}

//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop() {
  if (millis()-lastTime > delayMs) {
    int A = digitalRead(pinA);
    int B = digitalRead(pinB);

    if (oldA == LOW && A == HIGH) {
      if (oldB == HIGH) {
      // bri += 10;
      // if (bri > 250) bri = 10;
      effectCurrent += 1;
      if (effectCurrent >= MODE_COUNT) effectCurrent = 0;
    }
    else {
      // bri -= 10;
      // if (bri < 10) bri = 250;
      effectCurrent -= 1;
      if (effectCurrent < 0) effectCurrent = (MODE_COUNT-1);
    }
    oldA = A;

    //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
    // 6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
    colorUpdated(CALL_MODE_FX_CHANGED);
    lastTime = millis();
  }
}
