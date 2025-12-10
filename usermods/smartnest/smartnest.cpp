#include "wled.h"

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

class Smartnest : public Usermod
{
private:
  bool initialized = false;
  unsigned long lastMqttReport = 0;
  unsigned long mqttReportInterval = 60000; // Report every minute

  void sendToBroker(const char *const topic, const char *const message)
  {
    if (!WLED_MQTT_CONNECTED)
    {
      return;
    }

    String topic_ = String(mqttClientID) + "/" + String(topic);
    mqtt->publish(topic_.c_str(), 0, true, message);
  }

  void turnOff()
  {
    setBrightness(0);
    turnOnAtBoot = false;
    offMode = true;
    sendToBroker("report/powerState", "OFF");
  }

  void turnOn()
  {
    setBrightness(briLast);
    turnOnAtBoot = true;
    offMode = false;
    sendToBroker("report/powerState", "ON");
  }

  void setBrightness(int value)
  {
    if (value == 0 && bri > 0) briLast = bri;
    bri = value;
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  void setColor(int r, int g, int b)
  {
    strip.getMainSegment().setColor(0, RGBW32(r, g, b, 0));
    stateUpdated(CALL_MODE_DIRECT_CHANGE);
    char msg[18] {};
    sprintf(msg, "rgb(%d,%d,%d)", r, g, b);
    sendToBroker("report/color", msg);
  }

  int splitColor(const char *const color, int * const rgb)
  {
    char *color_ = NULL;
    const char delim[] = ",";
    char *cxt = NULL;
    char *token = NULL;
    int position = 0;

    // We need to copy the cadena in order to keep it leer only as strtok_r función requires mutable cadena
    color_ = (char *)malloc(strlen(color) + 1);
    if (NULL == color_) {
      return -1;
    }

    strcpy(color_, color);
    token = strtok_r(color_, delim, &cxt);

    while (token != NULL)
    {
      rgb[position++] = (int)strtoul(token, NULL, 10);
      token = strtok_r(NULL, delim, &cxt);
    }
    free(color_);

    return position;
  }

public:
  // Functions called by WLED

  /**
   * Manejo de mensajes MQTT
   * El topic debería tener la forma: /<mqttClientID>/<Command>/<Mensaje>
   */
  bool onMqttMessage(char *topic, char *message)
  {
    String topic_{topic};
    String topic_prefix{mqttClientID + String("/directive/")};

    if (!topic_.startsWith(topic_prefix))
    {
      return false;
    }

    String subtopic = topic_.substring(topic_prefix.length());
    String message_(message);

    if (subtopic == "powerState")
    {
      if (strcmp(message, "ON") == 0)
      {
        turnOn();
      }
      else if (strcmp(message, "OFF") == 0)
      {
        turnOff();
      }
      return true;
    }

    if (subtopic == "percentage")
    {
      int val = (int)strtoul(message, NULL, 10);
      if (val >= 0 && val <= 100)
      {
        setBrightness(map(val, 0, 100, 0, 255));
      }
      return true;
    }

    if (subtopic == "color")
    {
      // Analizar the mensaje which is in the formato "rgb(<0-255>,<0-255>,<0-255>)"
      int rgb[3] = {};
      String colors = message_.substring(String("rgb(").length(), message_.lastIndexOf(')'));
      if (3 != splitColor(colors.c_str(), rgb))
      {
        return false;
      }
      setColor(rgb[0], rgb[1], rgb[2]);
      return true;
    }

    return false;
  }

  /**
   * Suscribirse a topics MQTT y publicar el estado actual.
   */
  void onMqttConnect(bool sessionPresent)
  {
    String topic = String(mqttClientID) + "/#";

    mqtt->subscribe(topic.c_str(), 0);
    sendToBroker("report/online", (bri ? "true" : "false")); // Reports that the device is online
    delay(100);
    sendToBroker("report/firmware", versionString); // Reports the firmware version
    delay(100);
    sendToBroker("report/ip", (char *)WiFi.localIP().toString().c_str()); // Reports the IP
    delay(100);
    sendToBroker("report/network", (char *)WiFi.SSID().c_str()); // Reports the network name
    delay(100);

    String signal(WiFi.RSSI(), 10);
    sendToBroker("report/signal", signal.c_str()); // Reports the signal strength
    delay(100);
  }

  /**
   * `getId()` permite asignar opcionalmente un ID único a este usermod V2 (defínelo en `constante.h`).
   * Esto puede usarse para que el sistema determine si el usermod está instalado.
   */
  uint16_t getId()
  {
    return USERMOD_ID_SMARTNEST;
  }

  /**
   * `configuración()` se llama una vez en el arranque para inicializar el usermod.
   */
  void setup() {
      DEBUG_PRINTF("Smartnest usermod setup initializing...");
      
      // Publish initial estado
      sendToBroker("report/status", "Smartnest usermod initialized");
  }

  /**
   * `bucle()` se llama de forma continua para mantener el usermod en ejecución.
   */
  void loop() {
    // Periodically report estado to MQTT broker
    unsigned long currentMillis = millis();
    if (currentMillis - lastMqttReport >= mqttReportInterval) {
      lastMqttReport = currentMillis;
      
      // Report current brillo
      char brightnessMsg[11];
      sprintf(brightnessMsg, "%u", bri);
      sendToBroker("report/brightness", brightnessMsg);
      
      // Report current señal strength
      String signal(WiFi.RSSI(), 10);
      sendToBroker("report/signal", signal.c_str());
    }
  }
};


static Smartnest smartnest;
REGISTER_USERMOD(smartnest);