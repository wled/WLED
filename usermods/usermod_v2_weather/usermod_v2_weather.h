// Weather usermod by ewowi. Next to the weather functionality, this usermod is also a proof of concept 
//      to work with data obtained from the internet.

#pragma once

#include "wled.h"

// #define WEATHER_DEBUG

//declare weathermod global variables (always precede with weather_ (pseudo class static variables)
static uint32_t usermods_pushLoop = 0; //effect pushes loop to execute. might be interesting for audioreactive too
static uint8_t  weather_units = 1; //config var metric (celsius) is default. (Standard=Kelvin, Imperial is Fahrenheit)
static float    weather_minTemp = 0; //config var
static float    weather_maxTemp = 40; //config var
static float    weather_temps[100]; //array of temperatures
static time_t   weather_times[100]; //array of corresponding times

//effect function
uint16_t mode_2DWeather(void) { 

  usermods_pushLoop = millis(); //will be reset to 0 in usermod loop

  SEGMENT.fadeToBlackBy(10);

  float currentTemp = 0;
  // time_t currentTime = 0;

  for (int x=0; x<SEGMENT.virtualWidth(); x++) {
    CRGB color;
    currentTemp = weather_temps[0];
    // currentTime = weather_times[0];
    if (weather_times[x%100] < localTime && weather_times[(x+1)%100] >= localTime) {
      color = RED;
      currentTemp = map(localTime, weather_times[x%100], weather_times[(x+1)%100], weather_temps[x%100] * 1000, weather_temps[(x+1)%100] * 1000) / 1000.0;
      // currentTime = localTime;
    }
    else
      color = ColorFromPalette(SEGPALETTE, map((uint8_t)weather_temps[x%100], 0, 40, 1, 255), 255, LINEARBLEND);

    for (int y=0; y<SEGMENT.virtualHeight() * (weather_temps[x%100]-weather_minTemp)/(weather_maxTemp - weather_minTemp); y++) {
      SEGMENT.setPixelColorXY(x, SEGMENT.virtualHeight() - y, color);
    }
    // Serial.print(" ");
    // Serial.print(weather_temps[x%16]);
  }
  if (localTime < weather_times[0])
    currentTemp = map(localTime, weather_times[0], weather_times[1], weather_temps[0] * 1000, weather_temps[1] * 1000) / 1000.0;

  // Serial.print(" time ");

  // char timeString[64];
  // epochToString(currentTime, timeString);
  // Serial.print(timeString);

  // Serial.print(" temp ");

  char tempString[6] = { '\0' };  // initialize string with zeros
  snprintf(tempString, 5, "%5.2f", currentTemp); // snprintf will prevent overflow
  // Serial.println();

  CRGB color = ColorFromPalette(SEGPALETTE, map((uint8_t)currentTemp, 0, 40, 1, 255), 255, LINEARBLEND);
  //really don't understand why this is not working if width < 16 (only works when Serial.println is uncommented ???)
  // uint16_t x = 0;
  // const uint16_t xSpace = (SEGMENT.virtualWidth()<16)?4:5;
  // SEGMENT.drawCharacter(tempString[0], x, -2, 5, 8, color);
  // // Serial.printf("%d %d", x, xSpace);
  // x += xSpace;
  // SEGMENT.drawCharacter(tempString[1], x, -2, 5, 8, color);
  // // Serial.printf("  %d %d", x, xSpace);
  // x += xSpace;
  // SEGMENT.setPixelColorXY(x, 4, color);
  // // Serial.printf("  %d %d", x, xSpace);
  // x += (SEGMENT.virtualWidth()<16)?1:2;
  // SEGMENT.drawCharacter(tempString[3], x, -2, 5, 8, color);
  // // Serial.printf("  %d %d", x, xSpace);
  // x += xSpace;
  // SEGMENT.drawCharacter(tempString[4], x, -2, 5, 8, color);
  // // Serial.printf("  %d %d\n", x, xSpace);

 if (SEGMENT.virtualWidth() < 16) {
    SEGMENT.drawCharacter(tempString[0], 0, -2, 5, 8, uint32_t(color), BLACK);
    SEGMENT.drawCharacter(tempString[1], 4, -2, 5, 8, uint32_t(color), BLACK);
    SEGMENT.setPixelColorXY(8, 4, color);
    SEGMENT.drawCharacter(tempString[3], 9, -2, 5, 8, uint32_t(color), BLACK);
  }
  else {
    SEGMENT.drawCharacter(tempString[0], 0, -2, 5, 8, uint32_t(color), BLACK);
    SEGMENT.drawCharacter(tempString[1], 5, -2, 5, 8, uint32_t(color), BLACK);
    SEGMENT.setPixelColorXY(10, 4, color);
    SEGMENT.drawCharacter(tempString[3], 12, -2, 5, 8, uint32_t(color), BLACK);
    SEGMENT.drawCharacter(tempString[4], 17, -2, 5, 8, uint32_t(color), BLACK);
  }

  return FRAMETIME;
}

static const char _data_FX_MODE_2DWEATHER[] PROGMEM = "🌦Weather ☾@;!;!;2;pal=54"; //temperature palette

//utility function, move somewhere else???
void epochToString(time_t time, char *timeString) {
  tmElements_t tm;
  breakTime(time, tm);
  sprintf(timeString, "%04d-%02d-%02d %02d:%02d:%02d", tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
}

//utility function, move somewhere else???
void httpGet(WiFiClient &client, const char *url, char *errorMessage) {
  //https://arduinojson.org/v6/example/http-client/
  //is this the most compact way to do http get and put it in arduinojson object???
  //would like async response ... ???
  client.setTimeout(10000);
  if (!client.connect("api.openweathermap.org", 80)) {
    strcat(errorMessage, PSTR("Connection failed"));
  }
  else {
    // Send HTTP request
    client.println(url);
    client.println(F("Host: api.openweathermap.org"));
    client.println(F("Connection: close"));
    if (client.println() == 0) {
      strcat(errorMessage, PSTR("Failed to send request"));
    }
    else {
      // Check HTTP status
      char status[32] = {0};
      client.readBytesUntil('\r', status, sizeof(status));
      if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
        strcat(errorMessage, PSTR("Unexpected response: "));
        strcat(errorMessage, status);
      }
      else {
        // Skip HTTP headers
        char endOfHeaders[] = "\r\n\r\n";
        if (!client.find(endOfHeaders)) {
          strcat(errorMessage, PSTR("Invalid response"));
        }
      }
    }
  }
}

class WeatherUsermod : public Usermod {
  private:
    // strings to reduce flash memory usage (used more than twice)
    String apiKey = ""; //config var

    char errorMessage[100] = "";
    bool isConnected = false; //only call openweathermap if connected

  public:
    WeatherUsermod(const char *name, bool enabled):Usermod(name, enabled) {} //WLEDMM: this shouldn't be necessary (passthrough of constructor), maybe because Usermod is an abstract class

    void setup() {
      strip.addEffect(255, &mode_2DWeather, _data_FX_MODE_2DWEATHER);
    }

    void connected() {
      isConnected = true;
    }

    void loop() {
      // return if no location  or no api key (reset lastTime to force loop)
      if (fabs(latitude) < 0.00001 && fabs(latitude) < 0.00001) {strcpy(errorMessage, PSTR("No location")); lastTime = 0; return;}
      if (strcmp(apiKey.c_str(), "") == 0) {strcpy(errorMessage, PSTR("No api key")); lastTime = 0; return;}

      //execute only if connected, effect pushes it or every hour
      if (isConnected && usermods_pushLoop > millis() - 1000 && (lastTime == 0 || millis() - lastTime > 3600 * 1000)) {
        lastTime = millis();
        strcpy(errorMessage, ""); //clear possible previous errors

        WiFiClient client;

        char url[180];
        sprintf(url, "GET /data/2.5/forecast?lat=%f&lon=%f&appid=%s&units=%s HTTP/1.0", latitude, longitude, apiKey.c_str(), weather_units==0?"standard":weather_units==1?"metric":"imperial");
        #ifdef WEATHER_DEBUG
          Serial.println(url);
        #endif

        httpGet(client, url, errorMessage);

        if (strcmp(errorMessage, "") == 0) {

          // https://arduinojson.org/v6/how-to/deserialize-a-very-large-document/
          StaticJsonDocument<256> filter; //in practice about 128
          filter["list"][0]["dt"] = true;
          filter["list"][0]["main"]["temp"] = true;
          filter["city"]["name"] = true;
          filter["city"]["country"] = true;
          PSRAMDynamicJsonDocument weatherDoc(4096); //in practice about 2673

          // Parse JSON object
          DeserializationError error = deserializeJson(weatherDoc, client, DeserializationOption::Filter(filter));
          #ifdef WEATHER_DEBUG
            Serial.printf("filter %u / %u%% (%u %u %u)\n", (unsigned int)filter.memoryUsage(), 100 * filter.memoryUsage() / filter.capacity(), (unsigned int)filter.size(), filter.overflowed(), (unsigned int)filter.nesting());
            Serial.printf("weatherDoc %u / %u%% (%u %u %u)\n", (unsigned int)weatherDoc.memoryUsage(), 100 * weatherDoc.memoryUsage() / weatherDoc.capacity(), (unsigned int)weatherDoc.size(), weatherDoc.overflowed(), (unsigned int)weatherDoc.nesting());
            serializeJson(filter, Serial);
            Serial.println();
            serializeJson(weatherDoc, Serial);
            Serial.println();
          #endif
          if (error) {
            strcat(errorMessage, PSTR("deserializeJson() failed: "));
            strcat(errorMessage, error.c_str());
          }
          else { //everything successful!!
            JsonObject weatherDocObject = weatherDoc.as<JsonObject>();
            JsonArray list = weatherDocObject[F("list")];
            JsonObject city = weatherDocObject["city"];
            strcat(errorMessage, city["name"]); //api successful
            strcat(errorMessage, city["country"]);

            uint8_t i = 0;
            for (JsonObject listElement: list) {
              weather_times[i%100] = listElement["dt"]; 

              JsonObject main = listElement["main"];
              weather_temps[i%100] = main["temp"]; 

              #ifdef WEATHER_DEBUG
                char timeString[64];
                epochToString(listElement["dt"], timeString);
                Serial.print(timeString);

                Serial.print(" temp ");
                Serial.print(weather_temps[i%100]);

                Serial.print(" city ");
                Serial.print(errorMessage);

                Serial.print(" sunrise ");
                char sunriseString[64];
                epochToString(city["sunrise"], sunriseString);
                Serial.print(sunriseString);

                Serial.print(" localtime ");
                char localTimeString[64];
                epochToString(localTime, localTimeString);
                Serial.print(localTimeString);

                Serial.println();
              #endif

              i++;
            }
          }
        }

        // Disconnect
        client.stop();

      }
      usermods_pushLoop = 0;
    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     */
    void addToJsonInfo(JsonObject& root)
    {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray infoArr = user.createNestedArray(FPSTR(_name));
      infoArr.add(errorMessage); //value
      // infoArr.add(""); //unit
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root)
    {
      //root["user0"] = userVar0;
    }


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root)
    {
      // userVar0 = root["user0"] | userVar0; //if "user0" key exists in JSON, update, else keep old value
      //if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
    }


    void addToConfig(JsonObject& root)
    {
      Usermod::addToConfig(root);
      JsonObject top = root[FPSTR(_name)];
      top[F("apiKey")] = apiKey;
      top[F("units")]   = weather_units;
      top[F("minTemp")] = weather_minTemp;
      top[F("maxTemp")] = weather_maxTemp;
    }


    bool readFromConfig(JsonObject& root)
    {
      bool configComplete = Usermod::readFromConfig(root);
      JsonObject top = root[FPSTR(_name)];

      configComplete &= getJsonValue(top[F("apiKey")], apiKey);
      configComplete &= getJsonValue(top[F("units")], weather_units);
      configComplete &= getJsonValue(top[F("minTemp")], weather_minTemp);
      configComplete &= getJsonValue(top[F("maxTemp")], weather_maxTemp);

      //  * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
      return configComplete;
    }

    void appendConfigData()
    {
      oappend(SET_F("addHB('Weather');")); // WLEDMM

      oappend(SET_F("dd=addDropdown('Weather','units');"));
      oappend(SET_F("addOption(dd,'Kelvin',0);"));
      oappend(SET_F("addOption(dd,'Celsius',1);"));
      oappend(SET_F("addOption(dd,'Fahrenheit',2);"));
      oappend(SET_F("addInfo('Weather:units',1,'<i>Set time and location in time settings</i>');"));
      oappend(SET_F("addInfo('Weather:apiKey',1,'<i>Create account on openweathermap.org and copy the key</i>');"));
      oappend(SET_F("addInfo('Weather:minTemp',1,'<i>Changing values: Reboot to (re)load forecast</i>');"));
    }

    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw()
    {
      //strip.setPixelColor(0, RGBW32(0,0,0,0)) // set the first pixel to black
    }

   
    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_WEATHER;
    }
};

// example openweathermap data
// {"cod":"200","message":0,"cnt":40,"list":[
// {"dt":1663945200,"main":{"temp":18.05,"feels_like":17.79,"temp_min":17.64,"temp_max":18.05,"pressure":1014,"sea_level":1014,"grnd_level":1013,"humidity":72,"temp_kf":0.41},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":4.32,"deg":238,"gust":5.6},"visibility":10000,"pop":0.48,"rain":{"3h":0.15},"sys":{"pod":"d"},"dt_txt":"2022-09-23 15:00:00"},
// {"dt":1663956000,"main":{"temp":16.86,"feels_like":16.59,"temp_min":16.16,"temp_max":16.86,"pressure":1014,"sea_level":1014,"grnd_level":1013,"humidity":76,"temp_kf":0.7},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":5.02,"deg":232,"gust":6.1},"visibility":10000,"pop":0.51,"rain":{"3h":0.33},"sys":{"pod":"n"},"dt_txt":"2022-09-23 18:00:00"},
// {"dt":1663966800,"main":{"temp":15.66,"feels_like":15.47,"temp_min":15.66,"temp_max":15.66,"pressure":1013,"sea_level":1013,"grnd_level":1013,"humidity":84,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":5.51,"deg":243,"gust":7.66},"visibility":10000,"pop":0.92,"rain":{"3h":0.86},"sys":{"pod":"n"},"dt_txt":"2022-09-23 21:00:00"},
// {"dt":1663977600,"main":{"temp":15.31,"feels_like":15.14,"temp_min":15.31,"temp_max":15.31,"pressure":1012,"sea_level":1012,"grnd_level":1012,"humidity":86,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":5.65,"deg":243,"gust":8.67},"visibility":10000,"pop":0.96,"rain":{"3h":2.48},"sys":{"pod":"n"},"dt_txt":"2022-09-24 00:00:00"},
// {"dt":1663988400,"main":{"temp":14.86,"feels_like":14.7,"temp_min":14.86,"temp_max":14.86,"pressure":1011,"sea_level":1011,"grnd_level":1011,"humidity":88,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":6.3,"deg":238,"gust":8.78},"visibility":10000,"pop":0.94,"rain":{"3h":1.63},"sys":{"pod":"n"},"dt_txt":"2022-09-24 03:00:00"},
// {"dt":1663999200,"main":{"temp":14.65,"feels_like":14.49,"temp_min":14.65,"temp_max":14.65,"pressure":1011,"sea_level":1011,"grnd_level":1011,"humidity":89,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":4.36,"deg":238,"gust":6.28},"visibility":10000,"pop":1,"rain":{"3h":2.17},"sys":{"pod":"d"},"dt_txt":"2022-09-24 06:00:00"},
// {"dt":1664010000,"main":{"temp":14.41,"feels_like":14.31,"temp_min":14.41,"temp_max":14.41,"pressure":1012,"sea_level":1012,"grnd_level":1011,"humidity":92,"temp_kf":0},"weather":[{"id":501,"main":"Rain","description":"moderate rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":3.03,"deg":198,"gust":5.32},"visibility":10000,"pop":0.88,"rain":{"3h":3.34},"sys":{"pod":"d"},"dt_txt":"2022-09-24 09:00:00"},
// {"dt":1664020800,"main":{"temp":14.72,"feels_like":14.57,"temp_min":14.72,"temp_max":14.72,"pressure":1013,"sea_level":1013,"grnd_level":1012,"humidity":89,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":1.66,"deg":157,"gust":2.76},"visibility":10000,"pop":0.87,"rain":{"3h":1.4},"sys":{"pod":"d"},"dt_txt":"2022-09-24 12:00:00"},
// {"dt":1664031600,"main":{"temp":14.61,"feels_like":14.45,"temp_min":14.61,"temp_max":14.61,"pressure":1013,"sea_level":1013,"grnd_level":1012,"humidity":89,"temp_kf":0},"weather":[{"id":501,"main":"Rain","description":"moderate rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":4.64,"deg":30,"gust":6.18},"visibility":10000,"pop":0.76,"rain":{"3h":3.51},"sys":{"pod":"d"},"dt_txt":"2022-09-24 15:00:00"},
// {"dt":1664042400,"main":{"temp":14.33,"feels_like":14.17,"temp_min":14.33,"temp_max":14.33,"pressure":1014,"sea_level":1014,"grnd_level":1013,"humidity":90,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":7.37,"deg":46,"gust":9.91},"visibility":10000,"pop":0.81,"rain":{"3h":2.46},"sys":{"pod":"n"},"dt_txt":"2022-09-24 18:00:00"},
// {"dt":1664053200,"main":{"temp":13.81,"feels_like":13.47,"temp_min":13.81,"temp_max":13.81,"pressure":1015,"sea_level":1015,"grnd_level":1015,"humidity":85,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":5.62,"deg":53,"gust":9.48},"visibility":10000,"pop":0.53,"rain":{"3h":2.09},"sys":{"pod":"n"},"dt_txt":"2022-09-24 21:00:00"},
// {"dt":1664064000,"main":{"temp":13.69,"feels_like":13.26,"temp_min":13.69,"temp_max":13.69,"pressure":1016,"sea_level":1016,"grnd_level":1015,"humidity":82,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":4.31,"deg":48,"gust":8.01},"visibility":10000,"pop":0.53,"rain":{"3h":0.23},"sys":{"pod":"n"},"dt_txt":"2022-09-25 00:00:00"},
// {"dt":1664074800,"main":{"temp":13.68,"feels_like":13.14,"temp_min":13.68,"temp_max":13.68,"pressure":1016,"sea_level":1016,"grnd_level":1015,"humidity":78,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":4.7,"deg":45,"gust":7.58},"visibility":10000,"pop":0.69,"rain":{"3h":0.45},"sys":{"pod":"n"},"dt_txt":"2022-09-25 03:00:00"},
// {"dt":1664085600,"main":{"temp":12.96,"feels_like":12.4,"temp_min":12.96,"temp_max":12.96,"pressure":1016,"sea_level":1016,"grnd_level":1015,"humidity":80,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":4.29,"deg":54,"gust":7.3},"visibility":10000,"pop":0.77,"rain":{"3h":2.4},"sys":{"pod":"d"},"dt_txt":"2022-09-25 06:00:00"},
// {"dt":1664096400,"main":{"temp":14.14,"feels_like":13.44,"temp_min":14.14,"temp_max":14.14,"pressure":1017,"sea_level":1017,"grnd_level":1016,"humidity":70,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":77},"wind":{"speed":3.09,"deg":72,"gust":4.35},"visibility":10000,"pop":0.54,"rain":{"3h":0.34},"sys":{"pod":"d"},"dt_txt":"2022-09-25 09:00:00"},
// {"dt":1664107200,"main":{"temp":16.15,"feels_like":15.15,"temp_min":16.15,"temp_max":16.15,"pressure":1016,"sea_level":1016,"grnd_level":1015,"humidity":51,"temp_kf":0},"weather":[{"id":802,"main":"Clouds","description":"scattered clouds","icon":"03d"}],"clouds":{"all":47},"wind":{"speed":3.47,"deg":4,"gust":3.49},"visibility":10000,"pop":0.36,"sys":{"pod":"d"},"dt_txt":"2022-09-25 12:00:00"},
// {"dt":1664118000,"main":{"temp":15.56,"feels_like":14.56,"temp_min":15.56,"temp_max":15.56,"pressure":1015,"sea_level":1015,"grnd_level":1014,"humidity":53,"temp_kf":0},"weather":[{"id":802,"main":"Clouds","description":"scattered clouds","icon":"03d"}],"clouds":{"all":35},"wind":{"speed":3.39,"deg":357,"gust":2.98},"visibility":10000,"pop":0,"sys":{"pod":"d"},"dt_txt":"2022-09-25 15:00:00"},
// {"dt":1664128800,"main":{"temp":13.95,"feels_like":12.94,"temp_min":13.95,"temp_max":13.95,"pressure":1014,"sea_level":1014,"grnd_level":1013,"humidity":59,"temp_kf":0},"weather":[{"id":801,"main":"Clouds","description":"few clouds","icon":"02n"}],"clouds":{"all":21},"wind":{"speed":1.8,"deg":346,"gust":2.3},"visibility":10000,"pop":0,"sys":{"pod":"n"},"dt_txt":"2022-09-25 18:00:00"},
// {"dt":1664139600,"main":{"temp":13.57,"feels_like":12.63,"temp_min":13.57,"temp_max":13.57,"pressure":1013,"sea_level":1013,"grnd_level":1012,"humidity":63,"temp_kf":0},"weather":[{"id":803,"main":"Clouds","description":"broken clouds","icon":"04n"}],"clouds":{"all":82},"wind":{"speed":2.6,"deg":267,"gust":3.43},"visibility":10000,"pop":0,"sys":{"pod":"n"},"dt_txt":"2022-09-25 21:00:00"},
// {"dt":1664150400,"main":{"temp":13.93,"feels_like":13.02,"temp_min":13.93,"temp_max":13.93,"pressure":1011,"sea_level":1011,"grnd_level":1010,"humidity":63,"temp_kf":0},"weather":[{"id":804,"main":"Clouds","description":"overcast clouds","icon":"04n"}],"clouds":{"all":91},"wind":{"speed":4.77,"deg":236,"gust":7.03},"visibility":10000,"pop":0,"sys":{"pod":"n"},"dt_txt":"2022-09-26 00:00:00"},
// {"dt":1664161200,"main":{"temp":14.53,"feels_like":13.79,"temp_min":14.53,"temp_max":14.53,"pressure":1007,"sea_level":1007,"grnd_level":1006,"humidity":67,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":7.51,"deg":234,"gust":10.73},"visibility":10000,"pop":0.2,"rain":{"3h":0.13},"sys":{"pod":"n"},"dt_txt":"2022-09-26 03:00:00"},
// {"dt":1664172000,"main":{"temp":14.62,"feels_like":13.73,"temp_min":14.62,"temp_max":14.62,"pressure":1004,"sea_level":1004,"grnd_level":1003,"humidity":61,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":10.16,"deg":236,"gust":13.08},"visibility":10000,"pop":0.2,"rain":{"3h":0.15},"sys":{"pod":"d"},"dt_txt":"2022-09-26 06:00:00"},
// {"dt":1664182800,"main":{"temp":14.24,"feels_like":13.68,"temp_min":14.24,"temp_max":14.24,"pressure":1002,"sea_level":1002,"grnd_level":1001,"humidity":75,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":10.43,"deg":232,"gust":13.98},"visibility":10000,"pop":0.41,"rain":{"3h":0.29},"sys":{"pod":"d"},"dt_txt":"2022-09-26 09:00:00"},
// {"dt":1664193600,"main":{"temp":13.58,"feels_like":13.29,"temp_min":13.58,"temp_max":13.58,"pressure":999,"sea_level":999,"grnd_level":998,"humidity":88,"temp_kf":0},"weather":[{"id":501,"main":"Rain","description":"moderate rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":7.48,"deg":252,"gust":10.89},"visibility":7736,"pop":0.9,"rain":{"3h":3.95},"sys":{"pod":"d"},"dt_txt":"2022-09-26 12:00:00"},
// {"dt":1664204400,"main":{"temp":13.65,"feels_like":13.11,"temp_min":13.65,"temp_max":13.65,"pressure":998,"sea_level":998,"grnd_level":998,"humidity":78,"temp_kf":0},"weather":[{"id":501,"main":"Rain","description":"moderate rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":4.63,"deg":288,"gust":6.33},"visibility":10000,"pop":0.93,"rain":{"3h":4.13},"sys":{"pod":"d"},"dt_txt":"2022-09-26 15:00:00"},
// {"dt":1664215200,"main":{"temp":12.79,"feels_like":12.11,"temp_min":12.79,"temp_max":12.79,"pressure":999,"sea_level":999,"grnd_level":998,"humidity":76,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":84},"wind":{"speed":7.52,"deg":334,"gust":9.93},"visibility":10000,"pop":1,"rain":{"3h":1.18},"sys":{"pod":"n"},"dt_txt":"2022-09-26 18:00:00"},
// {"dt":1664226000,"main":{"temp":12.75,"feels_like":11.88,"temp_min":12.75,"temp_max":12.75,"pressure":1000,"sea_level":1000,"grnd_level":999,"humidity":69,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":81},"wind":{"speed":8.6,"deg":330,"gust":10.65},"visibility":10000,"pop":0.95,"rain":{"3h":0.94},"sys":{"pod":"n"},"dt_txt":"2022-09-26 21:00:00"},
// {"dt":1664236800,"main":{"temp":12.4,"feels_like":11.45,"temp_min":12.4,"temp_max":12.4,"pressure":1000,"sea_level":1000,"grnd_level":999,"humidity":67,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":90},"wind":{"speed":7.24,"deg":313,"gust":9.58},"visibility":10000,"pop":0.95,"rain":{"3h":0.66},"sys":{"pod":"n"},"dt_txt":"2022-09-27 00:00:00"},
// {"dt":1664247600,"main":{"temp":12.05,"feels_like":11.19,"temp_min":12.05,"temp_max":12.05,"pressure":1000,"sea_level":1000,"grnd_level":999,"humidity":72,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":100},"wind":{"speed":7.56,"deg":310,"gust":9.89},"visibility":10000,"pop":1,"rain":{"3h":1.31},"sys":{"pod":"n"},"dt_txt":"2022-09-27 03:00:00"},
// {"dt":1664258400,"main":{"temp":12.03,"feels_like":11.01,"temp_min":12.03,"temp_max":12.03,"pressure":999,"sea_level":999,"grnd_level":999,"humidity":66,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":9.24,"deg":284,"gust":11.35},"visibility":10000,"pop":1,"rain":{"3h":1.58},"sys":{"pod":"d"},"dt_txt":"2022-09-27 06:00:00"},
// {"dt":1664269200,"main":{"temp":12.3,"feels_like":11.31,"temp_min":12.3,"temp_max":12.3,"pressure":999,"sea_level":999,"grnd_level":999,"humidity":66,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":70},"wind":{"speed":9.89,"deg":272,"gust":11.4},"visibility":10000,"pop":0.51,"rain":{"3h":0.38},"sys":{"pod":"d"},"dt_txt":"2022-09-27 09:00:00"},
// {"dt":1664280000,"main":{"temp":12.97,"feels_like":12.12,"temp_min":12.97,"temp_max":12.97,"pressure":998,"sea_level":998,"grnd_level":997,"humidity":69,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":84},"wind":{"speed":9.12,"deg":263,"gust":12.4},"visibility":10000,"pop":0.95,"rain":{"3h":1.08},"sys":{"pod":"d"},"dt_txt":"2022-09-27 12:00:00"},
// {"dt":1664290800,"main":{"temp":13.1,"feels_like":12.32,"temp_min":13.1,"temp_max":13.1,"pressure":996,"sea_level":996,"grnd_level":995,"humidity":71,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":10.39,"deg":274,"gust":12.96},"visibility":10000,"pop":1,"rain":{"3h":1.97},"sys":{"pod":"d"},"dt_txt":"2022-09-27 15:00:00"},
// {"dt":1664301600,"main":{"temp":12.72,"feels_like":11.82,"temp_min":12.72,"temp_max":12.72,"pressure":996,"sea_level":996,"grnd_level":995,"humidity":68,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":99},"wind":{"speed":8.59,"deg":291,"gust":11.52},"visibility":10000,"pop":1,"rain":{"3h":1.59},"sys":{"pod":"n"},"dt_txt":"2022-09-27 18:00:00"},
// {"dt":1664312400,"main":{"temp":12.27,"feels_like":11.41,"temp_min":12.27,"temp_max":12.27,"pressure":995,"sea_level":995,"grnd_level":995,"humidity":71,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":98},"wind":{"speed":8.22,"deg":279,"gust":10.74},"visibility":10000,"pop":0.89,"rain":{"3h":1.06},"sys":{"pod":"n"},"dt_txt":"2022-09-27 21:00:00"},
// {"dt":1664323200,"main":{"temp":12.22,"feels_like":11.4,"temp_min":12.22,"temp_max":12.22,"pressure":994,"sea_level":994,"grnd_level":993,"humidity":73,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":92},"wind":{"speed":8.28,"deg":277,"gust":11.01},"visibility":10000,"pop":1,"rain":{"3h":1.02},"sys":{"pod":"n"},"dt_txt":"2022-09-28 00:00:00"},
// {"dt":1664334000,"main":{"temp":12.36,"feels_like":11.48,"temp_min":12.36,"temp_max":12.36,"pressure":993,"sea_level":993,"grnd_level":992,"humidity":70,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10n"}],"clouds":{"all":87},"wind":{"speed":8.64,"deg":276,"gust":11.11},"visibility":10000,"pop":0.92,"rain":{"3h":0.64},"sys":{"pod":"n"},"dt_txt":"2022-09-28 03:00:00"},
// {"dt":1664344800,"main":{"temp":12.54,"feels_like":11.63,"temp_min":12.54,"temp_max":12.54,"pressure":993,"sea_level":993,"grnd_level":992,"humidity":68,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":93},"wind":{"speed":8.94,"deg":282,"gust":11.02},"visibility":10000,"pop":0.88,"rain":{"3h":0.62},"sys":{"pod":"d"},"dt_txt":"2022-09-28 06:00:00"},
// {"dt":1664355600,"main":{"temp":12.76,"feels_like":11.95,"temp_min":12.76,"temp_max":12.76,"pressure":993,"sea_level":993,"grnd_level":992,"humidity":71,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":10.07,"deg":281,"gust":12.33},"visibility":10000,"pop":0.75,"rain":{"3h":1.12},"sys":{"pod":"d"},"dt_txt":"2022-09-28 09:00:00"},
// {"dt":1664366400,"main":{"temp":13.41,"feels_like":12.66,"temp_min":13.41,"temp_max":13.41,"pressure":994,"sea_level":994,"grnd_level":993,"humidity":71,"temp_kf":0},"weather":[{"id":500,"main":"Rain","description":"light rain","icon":"10d"}],"clouds":{"all":100},"wind":{"speed":10.01,"deg":297,"gust":12.82},"visibility":9744,"pop":0.95,"rain":{"3h":2.77},"sys":{"pod":"d"},"dt_txt":"2022-09-28 12:00:00"}
// ],"city":{"id":2747373,"name":"The Hague","coord":{"lat":52.08,"lon":4.28},"country":"NL","population":474292,"timezone":7200,"sunrise":1663911010,"sunset":1663954830}}