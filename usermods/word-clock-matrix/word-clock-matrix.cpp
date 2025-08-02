#include "wled.h"

/*
 * Things to do...
 * Turn on ntp clock 24h format
 * 64 LEDS
 */


class WordClockMatrix : public Usermod
{
private:
  unsigned long lastTime = 0;
  uint8_t minuteLast = 99;
  int dayBrightness = 128;
  int nightBrightness = 16;

public:
  void setup()
  {
    Serial.println("Hello from my usermod!");

    //saveMacro(14, "A=128", false);
    //saveMacro(15, "A=64", false);
    //saveMacro(16, "A=16", false);

    //saveMacro(1, "&FX=0&R=255&G=255&B=255", false);

    //strip.getSegment(1).setOption(SEG_OPTION_SELECTED, true);

    //select first two segments (background color + FX settable)
    Segment &seg = strip.getSegment(0);
    seg.colors[0] = ((0 << 24) | ((0 & 0xFF) << 16) | ((0 & 0xFF) << 8) | ((0 & 0xFF)));
    strip.getSegment(0).setOption(0, false);
    strip.getSegment(0).setOption(2, false);
    //other segments are text
    for (int i = 1; i < 10; i++)
    {
      Segment &text_seg = strip.getSegment(i);
      text_seg.colors[0] = ((0 << 24) | ((0 & 0xFF) << 16) | ((190 & 0xFF) << 8) | ((180 & 0xFF)));
      strip.getSegment(i).setOption(0, true);
      strip.setBrightness(64);
    }
  }

  void connected()
  {
    Serial.println("Connected to WiFi!");
  }

  void selectWordSegments(bool state)
  {
    for (int i = 1; i < 10; i++)
    {
      //WS2812FX::Segment &seg = strip.getSegment(i);
      strip.getSegment(i).setOption(0, state);
      // strip.getSegment(1).setOption(SEG_OPTION_SELECTED, true);
      //seg.mode = 12;
      //seg.palette = 1;
      //strip.setBrightness(255);
    }
    strip.getSegment(0).setOption(0, !state);
  }

  void hourChime()
  {
    //strip.resetSegments();
    selectWordSegments(true);
    colorUpdated(CALL_MODE_FX_CHANGED);
    savePreset(13);
    selectWordSegments(false);
    //strip.getSegment(0).setOption(0, true);
    strip.getSegment(0).setOption(2, true);
    applyPreset(12);
    colorUpdated(CALL_MODE_FX_CHANGED);
  }

  void displayTime(byte hour, byte minute)
  {
    bool isToHour = false;      //true if minute > 30
    strip.getSegment(0).setGeometry(0, 64); // background
    strip.getSegment(1).setGeometry(0, 2);  //It is

    strip.getSegment(2).setGeometry(0, 0);
    strip.getSegment(3).setGeometry(0, 0); //disable minutes
    strip.getSegment(4).setGeometry(0, 0); //past
    strip.getSegment(6).setGeometry(0, 0); //to
    strip.getSegment(8).setGeometry(0, 0); //disable o'clock

    if (hour < 24) //valid time, display
    {
      if (minute == 30)
      {
        strip.getSegment(2).setGeometry(3, 6); //half
        strip.getSegment(3).setGeometry(0, 0); //minutes
      }
      else if (minute == 15 || minute == 45)
      {
        strip.getSegment(3).setGeometry(0, 0); //minutes
      }
      else if (minute == 10)
      {
        //strip.getSegment(5).setGeometry(6, 8); //ten
      }
      else if (minute == 5)
      {
        //strip.getSegment(5).setGeometry(16, 18); //five
      }
      else if (minute == 0)
      {
        strip.getSegment(3).setGeometry(0, 0); //minutes
        //hourChime();
      }
      else
      {
        strip.getSegment(3).setGeometry(18, 22); //minutes
      }

      //past or to?
      if (minute == 0)
      {                              //full hour
        strip.getSegment(3).setGeometry(0, 0);   //disable minutes
        strip.getSegment(4).setGeometry(0, 0);   //disable past
        strip.getSegment(6).setGeometry(0, 0);   //disable to
        strip.getSegment(8).setGeometry(60, 64); //o'clock
      }
      else if (minute > 34)
      {
        //strip.getSegment(6).setGeometry(22, 24); //to
        //minute = 60 - minute;
        isToHour = true;
      }
      else
      {
        //strip.getSegment(4).setGeometry(24, 27); //past
        //isToHour = false;
      }
    }

    //byte minuteRem = minute %10;

    if (minute <= 4)
    {
      strip.getSegment(3).setGeometry(0, 0);   //nothing
      strip.getSegment(5).setGeometry(0, 0);   //nothing
      strip.getSegment(6).setGeometry(0, 0);   //nothing
      strip.getSegment(8).setGeometry(60, 64); //o'clock
    }
    else if (minute <= 9)
    {
      strip.getSegment(5).setGeometry(16, 18); // five past
      strip.getSegment(4).setGeometry(24, 27); //past
    }
    else if (minute <= 14)
    {
      strip.getSegment(5).setGeometry(6, 8);   // ten past
      strip.getSegment(4).setGeometry(24, 27); //past
    }
    else if (minute <= 19)
    {
      strip.getSegment(5).setGeometry(8, 12);  // quarter past
      strip.getSegment(3).setGeometry(0, 0);   //minutes
      strip.getSegment(4).setGeometry(24, 27); //past
    }
    else if (minute <= 24)
    {
      strip.getSegment(5).setGeometry(12, 16); // twenty past
      strip.getSegment(4).setGeometry(24, 27); //past
    }
    else if (minute <= 29)
    {
      strip.getSegment(5).setGeometry(12, 18); // twenty-five past
      strip.getSegment(4).setGeometry(24, 27); //past
    }
    else if (minute <= 34)
    {
      strip.getSegment(5).setGeometry(3, 6);   // half past
      strip.getSegment(3).setGeometry(0, 0);   //minutes
      strip.getSegment(4).setGeometry(24, 27); //past
    }
    else if (minute <= 39)
    {
      strip.getSegment(5).setGeometry(12, 18); // twenty-five to
      strip.getSegment(6).setGeometry(22, 24); //to
    }
    else if (minute <= 44)
    {
      strip.getSegment(5).setGeometry(12, 16); // twenty to
      strip.getSegment(6).setGeometry(22, 24); //to
    }
    else if (minute <= 49)
    {
      strip.getSegment(5).setGeometry(8, 12);  // quarter to
      strip.getSegment(3).setGeometry(0, 0);   //minutes
      strip.getSegment(6).setGeometry(22, 24); //to
    }
    else if (minute <= 54)
    {
      strip.getSegment(5).setGeometry(6, 8);   // ten to
      strip.getSegment(6).setGeometry(22, 24); //to
    }
    else if (minute <= 59)
    {
      strip.getSegment(5).setGeometry(16, 18); // five to
      strip.getSegment(6).setGeometry(22, 24); //to
    }

    //hours
    if (hour > 23)
      return;
    if (isToHour)
      hour++;
    if (hour > 12)
      hour -= 12;
    if (hour == 0)
      hour = 12;

    switch (hour)
    {
    case 1:
      strip.getSegment(7).setGeometry(27, 29);
      break; //one
    case 2:
      strip.getSegment(7).setGeometry(35, 37);
      break; //two
    case 3:
      strip.getSegment(7).setGeometry(29, 32);
      break; //three
    case 4:
      strip.getSegment(7).setGeometry(32, 35);
      break; //four
    case 5:
      strip.getSegment(7).setGeometry(37, 40);
      break; //five
    case 6:
      strip.getSegment(7).setGeometry(43, 45);
      break; //six
    case 7:
      strip.getSegment(7).setGeometry(40, 43);
      break; //seven
    case 8:
      strip.getSegment(7).setGeometry(45, 48);
      break; //eight
    case 9:
      strip.getSegment(7).setGeometry(48, 50);
      break; //nine
    case 10:
      strip.getSegment(7).setGeometry(54, 56);
      break; //ten
    case 11:
      strip.getSegment(7).setGeometry(50, 54);
      break; //eleven
    case 12:
      strip.getSegment(7).setGeometry(56, 60);
      break; //twelve
    }

    selectWordSegments(true);
    applyPreset(1);
  }

  void timeOfDay()
  {
    // NOT USED: use timed macros instead
    //Used to set brightness dependant of time of day - lights dimmed at night

    //monday to thursday and sunday

    if ((weekday(localTime) == 6) | (weekday(localTime) == 7))
    {
      if ((hour(localTime) > 0) | (hour(localTime) < 8))
      {
        strip.setBrightness(nightBrightness);
      }
      else
      {
        strip.setBrightness(dayBrightness);
      }
    }
    else
    {
      if ((hour(localTime) < 6) | (hour(localTime) >= 22))
      {
        strip.setBrightness(nightBrightness);
      }
      else
      {
        strip.setBrightness(dayBrightness);
      }
    }
  }

  //loop. You can use "if (WLED_CONNECTED)" to check for successful connection
  void loop()
  {

      if (millis() - lastTime > 1000) {
        //Serial.println("I'm alive!");
        Serial.println(hour(localTime));
        lastTime = millis();
      }


    if (minute(localTime) != minuteLast)
    {
      updateLocalTime();
      //timeOfDay();
      minuteLast = minute(localTime);
      displayTime(hour(localTime), minute(localTime));
      if (minute(localTime) == 0)
      {
        hourChime();
      }
      if (minute(localTime) == 1)
      {
        //turn off background segment;
        strip.getSegment(0).setOption(2, false);
        //applyPreset(13);
      }
    }
  }

    void addToConfig(JsonObject& root)
    {
      JsonObject modName = root.createNestedObject("id");
      modName[F("mdns")] = "wled-word-clock";
      modName[F("name")] = "WLED WORD CLOCK";
    }

    uint16_t getId()
    {
      return 500;
    }


};


static WordClockMatrix word_clock_matrix;
REGISTER_USERMOD(word_clock_matrix);