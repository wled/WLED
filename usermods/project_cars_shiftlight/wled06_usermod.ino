/*
 *          Car rev display and shift indicator for Project Cars
 *          
 * This works via the UDP telemetry function. You'll need to enable it in the settings of the game.
 * I've had good results with settings around 5 (20 fps).
 * 
 */
#include "wled.h"

const uint8_t PCARS_dimcolor = 20;
WiFiUDP UDP;
const unsigned int PCARS_localUdpPort = 5606; // local port to listen on
char PCARS_packet[2048];

char PCARS_tempChar[2]; // Temporary array for u16 conversion

u16 PCARS_RPM;
u16 PCARS_maxRPM;

long PCARS_lastRead = millis() - 2001;
float PCARS_rpmRatio;

void PCARS_readValues() {

  int PCARS_packetSize = UDP.parsePacket();
  if (PCARS_packetSize) {
    int len = UDP.read(PCARS_packet, PCARS_packetSize);
    if (len > 0) {
      PCARS_packet[len] = 0;
    }
    if (len == 1367) { // Telemetry packet. Ignoring everything else.
      PCARS_lastRead = millis();

      realtimeLock(realtimeTimeoutMs, REALTIME_MODE_GENERIC);
      // current RPM
      memcpy(&PCARS_tempChar, &PCARS_packet[124], 2);
      PCARS_RPM = (PCARS_tempChar[1] << 8) + PCARS_tempChar[0];

      // max RPM
      memcpy(&PCARS_tempChar, &PCARS_packet[126], 2);
      PCARS_maxRPM = (PCARS_tempChar[1] << 8) + PCARS_tempChar[0];

      if (PCARS_maxRPM) {
        PCARS_rpmRatio = constrain((float)PCARS_RPM / (float)PCARS_maxRPM, 0, 1);
      } else {
        PCARS_rpmRatio = 0.0;
      }
    }
  }
}
void PCARS_buildcolorbars() {
  boolean activated = false;
  float ledratio = 0;
  uint16_t totalLen = strip.getLengthTotal();

  for (uint16_t i = 0; i < totalLen; i++) {
    if (PCARS_rpmRatio < .95 || (millis() % 100 > 70 )) {

      ledratio = (float)i / (float)totalLen;
      if (ledratio < PCARS_rpmRatio) {
        activated = true;
      } else {
        activated = false;
      }
      if (ledratio > 0.66) {
        setRealtimePixel(i, 0, 0, PCARS_dimcolor + ((255 - PCARS_dimcolor)*activated), 0);
      } else if (ledratio > 0.33) {
        setRealtimePixel(i, PCARS_dimcolor + ((255 - PCARS_dimcolor)*activated), 0, 0, 0);
      } else {
        setRealtimePixel(i, 0, PCARS_dimcolor + ((255 - PCARS_dimcolor)*activated), 0, 0);
      }
    }
    else {
      setRealtimePixel(i, 0, 0, 0, 0);

    }
  }
  colorUpdated(5);
  strip.show();
}

void userSetup()
{
  UDP.begin(PCARS_localUdpPort);
}

void userConnected()
{
  // new wifi, who dis?
}

void userLoop()
{
  PCARS_readValues();
  if (PCARS_lastRead > millis() - 2000) {
    PCARS_buildcolorbars();
  }
}