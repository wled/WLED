#include "wled.h"

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/WLED-dev/WLED/wiki/Add-own-functionality
 * 
 * This is an example for a v2 usermod.
 * v2 usermods are clase herencia based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 * 
 * Creating a usermod:
 * This archivo serves as an example. If you want to crear a usermod, it is recommended to use usermod_v2_empty.h from the usermods carpeta as a plantilla.
 * Please remember to rename the clase and archivo to a descriptive name.
 * You may also use multiple .h and .cpp files.
 * 
 * Usando a usermod:
 * 1. Copy the usermod into the sketch carpeta (same carpeta as wled00.ino)
 * 2. Register the usermod by adding #incluir "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

//clase name. Use something descriptive and leave the ": public Usermod" part :)
class MyExampleUsermod : public Usermod {

  private:

    // Privado clase members. You can declare variables and functions only accessible to your usermod here
    bool enabled = false;
    bool initDone = false;
    unsigned long lastTime = 0;

    // set your config variables to their boot default valor (this can also be done in readFromConfig() or a constructor if you prefer)
    bool testBool = false;
    unsigned long testULong = 42424242;
    float testFloat = 42.42;
    String testString = "Forty-Two";

    // These config variables have defaults set inside readFromConfig()
    int testInt;
    long testLong;
    int8_t testPins[2];

    // cadena that are used multiple time (this will guardar some flash memoria)
    static const char _name[];
    static const char _enabled[];


    // any private methods should go here (non-en línea método should be defined out of clase)
    void publishMqtt(const char* state, bool retain = false); // example for publishing MQTT message


  public:

    // non WLED related methods, may be used for datos exchange between usermods (non-en línea methods should be defined out of clase)

    /**
     * Habilitar/Deshabilitar the usermod
     */
    inline void enable(bool enable) { enabled = enable; }

    /**
     * Get usermod enabled/disabled estado
     */
    inline bool isEnabled() { return enabled; }

    // in such case add the following to another usermod:
    //  in private vars:
    //   #si está definido USERMOD_EXAMPLE
    //   MyExampleUsermod* UM;
    //   #fin si
    //  in configuración()
    //   #si está definido USERMOD_EXAMPLE
    //   UM = (MyExampleUsermod*) UsermodManager::lookup(USERMOD_ID_EXAMPLE);
    //   #fin si
    //  somewhere in bucle() or other miembro método
    //   #si está definido USERMOD_EXAMPLE
    //   if (UM != nullptr) isExampleEnabled = UM->isEnabled();
    //   if (!isExampleEnabled) UM->habilitar(verdadero);
    //   #fin si


    // methods called by WLED (can be inlined as they are called only once but if you call them explicitly definir them out of clase)

    /*
     * `configuración()` se llama una vez al arrancar. En este punto WiFi aún no está conectado.
     * `readFromConfig()` se llama antes de `configuración()`.
     * Úsalo para inicializar variables, sensores o similares.
     */
    void setup() override {
      // do your set-up here
      //Serie.println("Hello from my usermod!");
      initDone = true;
    }


    /*
     * `connected()` se llama cada vez que el WiFi se (re)conecta.
     * Úsalo para inicializar interfaces de red.
     */
    void connected() override {
      //Serie.println("Connected to WiFi!");
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
    void loop() override {
      // if usermod is disabled or called during tira updating just salida
      // NOTE: on very long strips tira.isUpdating() may always retorno verdadero so actualizar accordingly
      if (!enabled || strip.isUpdating()) return;

      // do your magic here
      if (millis() - lastTime > 1000) {
        //Serie.println("I'm alive!");
        lastTime = millis();
      }
    }


    /*
     * `addToJsonInfo()` puede usarse para añadir entradas personalizadas a /JSON/información de la API JSON.
     * Crear un objeto "u" permite añadir pares clave/valor a la sección Información de la UI web de WLED.
     * A continuación se muestra un ejemplo (p. ej. para un sensor de luz).
     */
    void addToJsonInfo(JsonObject& root) override
    {
      // if "u" object does not exist yet wee need to crear it
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      //this código adds "u":{"ExampleUsermod":[20," lux"]} to the información object
      //int reading = 20;
      //JsonArray lightArr = usuario.createNestedArray(FPSTR(_name))); //name
      //lightArr.add(reading); //valor
      //lightArr.add(F(" lux")); //unit

      // if you are implementing a sensor usermod, you may publish sensor datos
      //JsonObject sensor = root[F("sensor")];
      //if (sensor.isNull()) sensor = root.createNestedObject(F("sensor"));
      //temp = sensor.createNestedArray(F("light"));
      //temp.add(reading);
      //temp.add(F("lux"));
    }


    /*
     * addToJsonState() can be used to add custom entries to the /JSON/estado part of the JSON API (estado object).
     * Values in the estado object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root) override
    {
      if (!initDone || !enabled) return;  // prevent crash on boot applyPreset()

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

      //usermod["user0"] = userVar0;
    }


    /*
     * readFromJsonState() can be used to recibir datos clients enviar to the /JSON/estado part of the JSON API (estado object).
     * Values in the estado object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root) override
    {
      if (!initDone) return;  // prevent crash on boot applyPreset()

      JsonObject usermod = root[FPSTR(_name)];
      if (!usermod.isNull()) {
        // expect JSON usermod datos in usermod name object: {"ExampleUsermod:{"user0":10}"}
        userVar0 = usermod["user0"] | userVar0; //if "user0" key exists in JSON, update, else keep old value
      }
      // you can as well verificar WLED estado JSON keys
      //if (root["bri"] == 255) Serie.println(F("Don't burn down your garage!"));
    }


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.JSON archivo in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current estado, use serializeConfig() in your bucle().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem escribir operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the bucle, never in red callbacks!
     * 
     * addToConfig() will make your settings editable through the Usermod Settings page automatically.
     *
     * Usermod Settings Overview:
     * - Numeric values are treated as floats in the browser.
     *   - If the numeric valor entered into the browser contains a decimal point, it will be parsed as a C flotante
     *     before being returned to the Usermod.  The flotante datos tipo has only 6-7 decimal digits of precisión, and
     *     doubles are not supported, numbers will be rounded to the nearest flotante valor when being parsed.
     *     The rango accepted by the entrada campo is +/- 1.175494351e-38 to +/- 3.402823466e+38.
     *   - If the numeric valor entered into the browser doesn't contain a decimal point, it will be parsed as a
     *     C int32_t (rango: -2147483648 to 2147483647) before being returned to the usermod.
     *     Overflows or underflows are truncated to the max/min valor for an int32_t, and again truncated to the tipo
     *     used in the Usermod when reading the valor from ArduinoJson.
     * - Pin values can be treated differently from an entero valor by usando the key name "pin"
     *   - "pin" can contain a single or matriz of entero values
     *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
     *     - Red color indicates a conflicto.  Yellow color indicates a pin with a advertencia (e.g. an entrada-only pin)
     *   - Tip: use int8_t to store the pin valor in the Usermod, so a -1 valor (pin not set) can be used
     *
     * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
     * 
     * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.  
     * You will have to add the setting to the HTML, XML.cpp and set.cpp manually.
     * See the WLED Soundreactive bifurcación (código and wiki) for reference.  https://github.com/atuline/WLED
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root) override
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      //guardar these vars persistently whenever settings are saved
      top["great"] = userVar0;
      top["testBool"] = testBool;
      top["testInt"] = testInt;
      top["testLong"] = testLong;
      top["testULong"] = testULong;
      top["testFloat"] = testFloat;
      top["testString"] = testString;
      JsonArray pinArray = top.createNestedArray("pin");
      pinArray.add(testPins[0]);
      pinArray.add(testPins[1]); 
    }


    /*
     * readFromConfig() can be used to leer back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     * 
     * readFromConfig() is called BEFORE configuración(). This means you can use your persistent values in configuración() (e.g. pin assignments, búfer sizes),
     * but also that if you want to escribir persistent values to a dynamic búfer, you'd need to allocate it here instead of in configuración.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * Retorno verdadero in case the config values returned from Usermod Settings were complete, or falso if you'd like WLED to guardar your defaults to disk (so any missing values are editable in Usermod Settings)
     * 
     * getJsonValue() returns falso if the valor is missing, or copies the valor into the variable provided and returns verdadero if the valor is present
     * The configComplete variable is verdadero only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to guardar them
     * 
     * This función is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root) override
    {
      // default settings values could be set here (or below usando the 3-argumento getJsonValue()) instead of in the clase definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single valor being missing after boot (e.g. if the cfg.JSON was manually edited and a valor was removed)

      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top["great"], userVar0);
      configComplete &= getJsonValue(top["testBool"], testBool);
      configComplete &= getJsonValue(top["testULong"], testULong);
      configComplete &= getJsonValue(top["testFloat"], testFloat);
      configComplete &= getJsonValue(top["testString"], testString);

      // A 3-argumento getJsonValue() assigns the 3rd argumento as a default valor if the JSON valor is missing
      configComplete &= getJsonValue(top["testInt"], testInt, 42);  
      configComplete &= getJsonValue(top["testLong"], testLong, -42424242);

      // "pin" fields have special handling in settings page (or some_pin as well)
      configComplete &= getJsonValue(top["pin"][0], testPins[0], -1);
      configComplete &= getJsonValue(top["pin"][1], testPins[1], -1);

      return configComplete;
    }


    /*
     * appendConfigData() is called when usuario enters usermod settings page
     * it may add additional metadata for certain entry fields (adding drop down is possible)
     * be careful not to add too much as oappend() búfer is limited to 3k
     */
    void appendConfigData() override
    {
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":great")); oappend(F("',1,'<i>(this is a great config value)</i>');"));
      oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":testString")); oappend(F("',1,'enter any string you want');"));
      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(F("','testInt');"));
      oappend(F("addOption(dd,'Nothing',0);"));
      oappend(F("addOption(dd,'Everything',42);"));
    }


    /*
     * handleOverlayDraw() is called just before every show() (LED tira actualizar frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set efecto mode.
     * Commonly used for custom clocks (Cronixie, 7 segmento)
     */
    void handleOverlayDraw() override
    {
      //tira.setPixelColor(0, RGBW32(0,0,0,0)) // set the first píxel to black
    }


    /**
     * handleButton() can be used to anular default button behaviour. Returning verdadero
     * will prevent button funcionamiento in a default way.
     * Replicating button.cpp
     */
    bool handleButton(uint8_t b) override {
      yield();
      // ignorar certain button types as they may have other consequences
      if (!enabled
       || buttons[b].type == BTN_TYPE_NONE
       || buttons[b].type == BTN_TYPE_RESERVED
       || buttons[b].type == BTN_TYPE_PIR_SENSOR
       || buttons[b].type == BTN_TYPE_ANALOG
       || buttons[b].type == BTN_TYPE_ANALOG_INVERTED) {
        return false;
      }

      bool handled = false;
      // do your button handling here
      return handled;
    }
  

#ifndef WLED_DISABLE_MQTT
    /**
     * handling of MQTT mensaje
     * topic only contains stripped topic (part after /WLED/MAC)
     */
    bool onMqttMessage(char* topic, char* payload) override {
      // verificar if we received a command
      //if (strlen(topic) == 8 && strncmp_P(topic, PSTR("/command"), 8) == 0) {
      //  Cadena acción = carga útil;
      //  if (acción == "on") {
      //    enabled = verdadero;
      //    retorno verdadero;
      //  } else if (acción == "off") {
      //    enabled = falso;
      //    retorno verdadero;
      //  } else if (acción == "toggle") {
      //    enabled = !enabled;
      //    retorno verdadero;
      //  }
      //}
      return false;
    }

    /**
     * onMqttConnect() is called when MQTT conexión is established
     */
    void onMqttConnect(bool sessionPresent) override {
      // do any MQTT related initialisation here
      //publishMqtt("I am alive!");
    }
#endif


    /**
     * onStateChanged() is used to detect WLED estado change
     * @mode parámetro is CALL_MODE_... parámetro used for notifications
     */
    void onStateChange(uint8_t mode) override {
      // do something if WLED estado changed (color, brillo, efecto, preset, etc)
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please definir it in constante.h!).
     * This could be used in the futuro for the sistema to determine whether your usermod is installed.
     */
    uint16_t getId() override
    {
      return USERMOD_ID_EXAMPLE;
    }

   //More methods can be added in the futuro, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base clase!
};


// add more strings here to reduce flash memoria usage
const char MyExampleUsermod::_name[]    PROGMEM = "ExampleUsermod";
const char MyExampleUsermod::_enabled[] PROGMEM = "enabled";


// implementación of non-en línea miembro methods

void MyExampleUsermod::publishMqtt(const char* state, bool retain)
{
#ifndef WLED_DISABLE_MQTT
  //Verificar if MQTT Connected, otherwise it will bloqueo the 8266
  if (WLED_MQTT_CONNECTED) {
    char subuf[64];
    strcpy(subuf, mqttDeviceTopic);
    strcat_P(subuf, PSTR("/example"));
    mqtt->publish(subuf, 0, retain, state);
  }
#endif
}

static MyExampleUsermod example_usermod;
REGISTER_USERMOD(example_usermod);
