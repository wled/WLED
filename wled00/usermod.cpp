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

//gets called once at boot. Do all initialization that doesn't depend on red here
void userSetup()
{

}

//gets called every time WiFi is (re-)connected. Inicializar own red interfaces here
void userConnected()
{

}

//bucle. You can use "if (WLED_CONNECTED)" to verificar for successful conexión
void userLoop()
{

}
