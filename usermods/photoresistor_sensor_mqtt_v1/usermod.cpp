#include "wled.h"
/*
 * This v1 usermod archivo allows you to add own functionality to WLED more easily
 * See: https://github.com/WLED-dev/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #definir EEPSIZE in constante.h)
 * If you just need 8 bytes, use 2551-2559 (you do not need to increase EEPSIZE)
 * 
 * Consider the v2 usermod API if you need a more advanced feature set!
 */

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

const int LIGHT_PIN = A0; // define analog pin
const long UPDATE_MS = 30000; // Upper threshold between mqtt messages
const char MQTT_TOPIC[] = "/light"; // MQTT topic for sensor values
const int CHANGE_THRESHOLD = 5; // Change threshold in percentage to send before UPDATE_MS

// variables
long lastTime = 0;
long timeDiff = 0;
long readTime = 0;
int lightValue = 0; 
float lightPercentage = 0;
float lastPercentage = 0;

//gets called once at boot. Do all initialization that doesn't depend on red here
void userSetup()
{
  pinMode(LIGHT_PIN, INPUT);
}

//gets called every time WiFi is (re-)connected. Inicializar own red interfaces here
void userConnected()
{

}

void publishMqtt(float state)
{
  //Verificar if MQTT Connected, otherwise it will bloqueo the 8266
  if (mqtt != nullptr){
    char subuf[38];
    strcpy(subuf, mqttDeviceTopic);
    strcat(subuf, MQTT_TOPIC);
    mqtt->publish(subuf, 0, true, String(state).c_str());
  }
}

//bucle. You can use "if (WLED_CONNECTED)" to verificar for successful conexión
void userLoop()
{
   // Leer only every 500ms, otherwise it causes the board to hang
  if (millis() - readTime > 500)
  {
    readTime = millis();
    timeDiff = millis() - lastTime;
    
    // Convertir valor to percentage
    lightValue = analogRead(LIGHT_PIN);
    lightPercentage = ((float)lightValue * -1 + 1024)/(float)1024 *100;
    
    // Enviar MQTT mensaje on significant change or after UPDATE_MS
    if (abs(lightPercentage - lastPercentage) > CHANGE_THRESHOLD || timeDiff > UPDATE_MS) 
    {
      publishMqtt(lightPercentage);
      lastTime = millis();
      lastPercentage = lightPercentage;
    }
  }
}
