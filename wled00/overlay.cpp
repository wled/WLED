#include "wled.h"

/*
 * Used to draw clock overlays over the strip
 */

void _overlayAnalogClock()
{
  int overlaySize = overlayMax - overlayMin +1;
  if (countdownMode)
  {
    _overlayAnalogCountdown(); return;
  }
  float hourP = ((float)(hour(localTime)%12))/12.0f;
  float minuteP = ((float)minute(localTime))/60.0f;
  hourP = hourP + minuteP/12.0f;
  float secondP = ((float)second(localTime))/60.0f;
  unsigned hourPixel = floorf(analogClock12pixel + overlaySize*hourP);
  if (hourPixel > overlayMax) hourPixel = overlayMin -1 + hourPixel - overlayMax;
  unsigned minutePixel = floorf(analogClock12pixel + overlaySize*minuteP);
  if (minutePixel > overlayMax) minutePixel = overlayMin -1 + minutePixel - overlayMax;
  unsigned secondPixel = floorf(analogClock12pixel + overlaySize*secondP);
  if (secondPixel > overlayMax) secondPixel = overlayMin -1 + secondPixel - overlayMax;
  if (analogClockSecondsTrail)
  {
    if (secondPixel < analogClock12pixel)
    {
      strip.setRange(analogClock12pixel, overlayMax, color_fade(0xFF0000, bri));
      strip.setRange(overlayMin, secondPixel, color_fade(0xFF0000, bri));
    } else
    {
      strip.setRange(analogClock12pixel, secondPixel, color_fade(0xFF0000, bri));
    }
  }
  if (analogClock5MinuteMarks)
  {
    for (unsigned i = 0; i <= 12; i++)
    {
      unsigned pix = analogClock12pixel + roundf((overlaySize / 12.0f) *i);
      if (pix > overlayMax) pix -= overlaySize;
      strip.setPixelColor(pix, color_fade(0x00FFAA, bri));
    }
  }
  if (!analogClockSecondsTrail) strip.setPixelColor(secondPixel, color_fade(0xFF0000, bri));
  strip.setPixelColor(minutePixel, color_fade(0x00FF00, bri));
  strip.setPixelColor(hourPixel, color_fade(0x0000FF, bri));
}


void _overlayAnalogCountdown()
{
  if ((unsigned long)toki.second() < countdownTime)
  {
    long diff = countdownTime - toki.second();
    float pval = 60.0f;
    if (diff > 31557600L) //display in years if more than 365 days
    {
      pval = 315576000.0f; //10 years
    } else if (diff > 2592000L) //display in months if more than a month
    {
      pval = 31557600.0f; //1 year
    } else if (diff > 604800) //display in weeks if more than a week
    {
      pval = 2592000.0f; //1 month
    } else if (diff > 86400) //display in days if more than 24 hours
    {
      pval = 604800.0f; //1 week
    } else if (diff > 3600) //display in hours if more than 60 minutes
    {
      pval = 86400.0f; //1 day
    } else if (diff > 60) //display in minutes if more than 60 seconds
    {
      pval = 3600.0f; //1 hour
    }
    int overlaySize = overlayMax - overlayMin +1;
    float perc = (pval-(float)diff)/pval;
    if (perc > 1.0f) perc = 1.0f;
    byte pixelCnt = perc*overlaySize;
    if (analogClock12pixel + pixelCnt > overlayMax)
    {
      strip.setRange(analogClock12pixel, overlayMax, ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
      strip.setRange(overlayMin, overlayMin +pixelCnt -(1+ overlayMax -analogClock12pixel), ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
    } else
    {
      strip.setRange(analogClock12pixel, analogClock12pixel + pixelCnt, ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
    }
  }
}

void handleOverlayDraw() {
  UsermodManager::handleOverlayDraw();
  if (analogClockSolidBlack) {
    for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
      const Segment& segment = strip.getSegment(i);
      if (!segment.isActive()) continue;
      if (segment.mode > 0 || segment.colors[0] > 0) {
        return;
      }
    }
  }
  if (overlayCurrent == 1) _overlayAnalogClock();
}

/*
 * Support for the Cronixie clock has moved to a usermod, compile with "-D USERMOD_CRONIXIE" to enable
 */
