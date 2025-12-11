#include "wled.h"

#if defined(ESP8266)
#include <ping.h>

/*
 * Este usermod realiza una petición ping a la IP local cada 60 segundos.
 * Con este procedimiento los servicios de red de WLED permanecen accesibles en entornos WLAN problemáticos.
 *
 * Los usermods permiten añadir funcionalidad propia a WLED de forma sencilla.
 * Ver: https://github.com/WLED-dev/WLED/wiki/Add-own-functionality
 *
 * Los usermods v2 se basan en herencia de clases y pueden (pero no deben) implementar más funciones; este ejemplo muestra varias.
 * Se pueden añadir múltiples usermods v2 en una misma compilación.
 *
 * Creación de un usermod:
 * Este fichero sirve como ejemplo. Para crear un usermod, se recomienda usar `usermod_v2_empty.h` como plantilla.
 * Recuerda renombrar la clase y el fichero a nombres descriptivos.
 * También puedes usar varios ficheros `.h` y `.cpp`.
 *
 * Uso de un usermod:
 * 1. Copia el usermod a la carpeta del sketch (misma carpeta que `wled00.ino`).
 * 2. Registra el usermod añadiendo `#incluir "usermod_filename.h"` y `registerUsermod(new MyUsermodClass())` en `usermods_list.cpp`.
 */

class FixUnreachableNetServices : public Usermod
{
private:
  //Privado clase members. You can declare variables and functions only accessible to your usermod here
  unsigned long m_lastTime = 0;

  // declare required variables
  unsigned long m_pingDelayMs = 60000;
  unsigned long m_connectedWiFi = 0;
  ping_option m_pingOpt;
  unsigned int m_pingCount = 0;
  bool m_updateConfig = false;

public:
  //Functions called by WLED

  /**
   * `configuración()` se llama una vez al arrancar. En este punto WiFi aún no está conectado.
   * Úsalo para inicializar variables, sensores o similares.
   */
  void setup()
  {
    //Serie.println("Hello from my usermod!");
  }

  /**
   * `connected()` se llama cada vez que el WiFi se (re)conecta.
   * Úsalo para inicializar interfaces de red.
   */
  void connected()
  {
    //Serie.println("Connected to WiFi!");

    ++m_connectedWiFi;

    // inicializar ping_options structure
    memset(&m_pingOpt, 0, sizeof(struct ping_option));
    m_pingOpt.count = 1;
    m_pingOpt.ip = WiFi.localIP();
  }

  /**
   * `bucle()`
   */
  void loop()
  {
    if (m_connectedWiFi > 0 && millis() - m_lastTime > m_pingDelayMs)
    {
      ping_start(&m_pingOpt);
      m_lastTime = millis();
      ++m_pingCount;
    }
    if (m_updateConfig)
    {
      serializeConfig();
      m_updateConfig = false;
    }
  }

  /**
   * addToJsonInfo() can be used to add custom entries to the /JSON/información part of the JSON API.
   * Creating an "u" object allows you to add custom key/valor pairs to the Información section of the WLED web UI.
   * Below it is shown how this could be used for e.g. a light sensor
   */
  void addToJsonInfo(JsonObject &root)
  {
    //this código adds "u":{"&#x26A1; Ping fix pings": m_pingCount} to the información object
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    String uiDomString = "&#x26A1; Ping fix pings<span style=\"display:block;padding-left:25px;\">\
Delay <input type=\"number\" min=\"5\" max=\"300\" value=\"";
    uiDomString += (unsigned long)(m_pingDelayMs / 1000);
    uiDomString += "\" onchange=\"requestJson({PingDelay:parseInt(this.value)});\">sec</span>";

    JsonArray infoArr = user.createNestedArray(uiDomString); //name
    infoArr.add(m_pingCount);                                              //value

    //this código adds "u":{"&#x26A1; Reconnects": m_connectedWiFi - 1} to the información object
    infoArr = user.createNestedArray("&#x26A1; Reconnects"); //name
    infoArr.add(m_connectedWiFi - 1);                        //value
  }

  /**
   * addToJsonState() can be used to add custom entries to the /JSON/estado part of the JSON API (estado object).
   * Values in the estado object may be modified by connected clients
   */
  void addToJsonState(JsonObject &root)
  {
    root["PingDelay"] = (m_pingDelayMs/1000);
  }

  /**
   * readFromJsonState() can be used to recibir datos clients enviar to the /JSON/estado part of the JSON API (estado object).
   * Values in the estado object may be modified by connected clients
   */
  void readFromJsonState(JsonObject &root)
  {
    if (root["PingDelay"] != nullptr)
    {
      m_pingDelayMs = (1000 * max(1UL, min(300UL, root["PingDelay"].as<unsigned long>())));
      m_updateConfig = true;
    }
  }

  /**
   * provide the changeable values
   */
  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject("FixUnreachableNetServices");
    top["PingDelayMs"] = m_pingDelayMs;
  }

  /**
   * restore the changeable values
   */
  bool readFromConfig(JsonObject &root)
  {
    JsonObject top = root["FixUnreachableNetServices"];
    if (top.isNull()) return false;
    m_pingDelayMs = top["PingDelayMs"] | m_pingDelayMs;
    m_pingDelayMs = max(5000UL, min(18000000UL, m_pingDelayMs));
    // use "retorno !top["newestParameter"].isNull();" when updating Usermod with new features
    return true;
  }

  /**
   * getId() allows you to optionally give your V2 usermod an unique ID (please definir it in constante.h!).
   * This could be used in the futuro for the sistema to determine whether your usermod is installed.
   */
  uint16_t getId()
  {
    return USERMOD_ID_FIXNETSERVICES;
  }
};

static FixUnreachableNetServices fix_unreachable_net_services;
REGISTER_USERMOD(fix_unreachable_net_services);

#else /* !ESP8266 */
#warning "Usermod FixUnreachableNetServices works only with ESP8266 builds"
#endif

