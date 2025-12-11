#include "wled.h"

//v2 usermod that allows to change brillo and color usando a rotary encoder, 
//change between modes by pressing a button (many encoders have one included)
class RotaryEncoderBrightnessColor : public Usermod
{
private:
  //Privado clase members. You can declare variables and functions only accessible to your usermod here
  unsigned long lastTime = 0;
  unsigned long currentTime;
  unsigned long loopTime;

  unsigned char select_state = 0; // 0 = brightness 1 = color
  unsigned char button_state = HIGH;
  unsigned char prev_button_state = HIGH;
  CRGB fastled_col;
  CHSV prim_hsv;
  int16_t new_val;

  unsigned char Enc_A;
  unsigned char Enc_B;
  unsigned char Enc_A_prev = 0;

  // private clase members configurable by Usermod Settings (defaults set inside readFromConfig())
  int8_t pins[3]; // pins[0] = DT from encoder, pins[1] = CLK from encoder, pins[2] = CLK from encoder (optional)
  int fadeAmount; // how many points to fade the Neopixel with each step

public:
  //Functions called by WLED

  /*
   * `configuración()` se llama una vez al arrancar. En este punto WiFi aún no está conectado.
   * Úsalo para inicializar variables, sensores o similares.
   */
  void setup()
  {
    //Serie.println("Hello from my usermod!");
    pinMode(pins[0], INPUT_PULLUP);
    pinMode(pins[1], INPUT_PULLUP);
    if(pins[2] >= 0) pinMode(pins[2], INPUT_PULLUP);
    currentTime = millis();
    loopTime = currentTime;
  }

  /*
   * `bucle()` se llama de forma continua. Aquí puedes comprobar eventos, leer sensores, etc.
   *
   * Consejos:
   * 1. Puedes usar "if (WLED_CONNECTED)" para comprobar una conexión de red.
   *    Adicionalmente, "if (WLED_MQTT_CONNECTED)" permite comprobar la conexión al broker MQTT.
   *
   * 2. Evita usar `retraso()`; NUNCA uses delays mayores a 10 ms.
   *    En su lugar usa comprobaciones temporizadas como en este ejemplo.
   */
  void loop()
  {
    currentTime = millis(); // get the current elapsed time

    if (currentTime >= (loopTime + 2)) // 2ms since last check of encoder = 500Hz
    {
      if(pins[2] >= 0) {
        button_state = digitalRead(pins[2]);
        if (prev_button_state != button_state)
        {
          if (button_state == LOW)
          {
            if (select_state == 1)
            {
              select_state = 0;
            }
            else
            {
              select_state = 1;
            }
            prev_button_state = button_state;
          }
          else
          {
            prev_button_state = button_state;
          }
        }
      }
      int Enc_A = digitalRead(pins[0]); // Read encoder pins
      int Enc_B = digitalRead(pins[1]);
      if ((!Enc_A) && (Enc_A_prev))
      { // A has gone from high to low
        if (Enc_B == HIGH)
        { // B is high so clockwise
          if (select_state == 0)
          {
            if (bri + fadeAmount <= 255)
              bri += fadeAmount; // increase the brightness, dont go over 255
          }
          else
          {
            fastled_col.red = colPri[0];
            fastled_col.green = colPri[1];
            fastled_col.blue = colPri[2];
            prim_hsv = rgb2hsv_approximate(fastled_col);
            new_val = (int16_t)prim_hsv.h + fadeAmount;
            if (new_val > 255)
              new_val -= 255; // roll-over if  bigger than 255
            if (new_val < 0)
              new_val += 255; // roll-over if smaller than 0
            prim_hsv.h = (byte)new_val;
            hsv2rgb_rainbow(prim_hsv, fastled_col);
            colPri[0] = fastled_col.red;
            colPri[1] = fastled_col.green;
            colPri[2] = fastled_col.blue;
          }
        }
        else if (Enc_B == LOW)
        { // B is low so counter-clockwise
          if (select_state == 0)
          {
            if (bri - fadeAmount >= 0)
              bri -= fadeAmount; // decrease the brightness, dont go below 0
          }
          else
          {
            fastled_col.red = colPri[0];
            fastled_col.green = colPri[1];
            fastled_col.blue = colPri[2];
            prim_hsv = rgb2hsv_approximate(fastled_col);
            new_val = (int16_t)prim_hsv.h - fadeAmount;
            if (new_val > 255)
              new_val -= 255; // roll-over if  bigger than 255
            if (new_val < 0)
              new_val += 255; // roll-over if smaller than 0
            prim_hsv.h = (byte)new_val;
            hsv2rgb_rainbow(prim_hsv, fastled_col);
            colPri[0] = fastled_col.red;
            colPri[1] = fastled_col.green;
            colPri[2] = fastled_col.blue;
          }
        }
        //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
        // 6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
        colorUpdated(CALL_MODE_BUTTON);
        updateInterfaces(CALL_MODE_BUTTON);
      }
      Enc_A_prev = Enc_A;     // Store value of A for next time
      loopTime = currentTime; // Updates loopTime
    }
  }

  void addToConfig(JsonObject& root)
  {
    JsonObject top = root.createNestedObject("rotEncBrightness");
    top["fadeAmount"] = fadeAmount;
    JsonArray pinArray = top.createNestedArray("pin");
    pinArray.add(pins[0]);
    pinArray.add(pins[1]); 
    pinArray.add(pins[2]); 
  }

  /* 
   * This example uses a more robust método of checking for missing values in the config, and setting back to defaults:
   * - The getJsonValue() función copies the valor to the variable only if the key requested is present, returning falso with no copy if the valor isn't present
   * - configComplete is used to retorno falso if any valor is missing, not just if the principal object is missing
   * - The defaults are loaded every time readFromConfig() is run, not just once after boot
   * 
   * This ensures that missing values are added to the config, with their default values, in the rare but plausible cases of:
   * - a single valor being missing at boot, e.g. if the Usermod was upgraded and a new setting was added
   * - a single valor being missing after boot (e.g. if the cfg.JSON was manually edited and a valor was removed)
   * 
   * If configComplete is falso, the default values are already set, and by returning falso, WLED now knows it needs to guardar the defaults by calling addToConfig()
   */
  bool readFromConfig(JsonObject& root)
  {
    // set defaults here, they will be set before configuración() is called, and if any values parsed from ArduinoJson below are missing, the default will be used instead
    fadeAmount = 5;
    pins[0] = -1;
    pins[1] = -1;
    pins[2] = -1;

    JsonObject top = root["rotEncBrightness"];

    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top["fadeAmount"], fadeAmount);
    configComplete &= getJsonValue(top["pin"][0], pins[0]);
    configComplete &= getJsonValue(top["pin"][1], pins[1]);
    configComplete &= getJsonValue(top["pin"][2], pins[2]);

    return configComplete;
  }
};


static RotaryEncoderBrightnessColor usermod_rotary_brightness_color;
REGISTER_USERMOD(usermod_rotary_brightness_color);