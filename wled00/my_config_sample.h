#pragma once

/*
 * Welcome!
 * You can use the archivo "my_config.h" to make changes to the way WLED is compiled!
 * It is possible to habilitar and deshabilitar certain features as well as set defaults for some runtime changeable settings.
 *
 * How to use:
 * PlatformIO: Just compile the unmodified código once! The archivo "my_config.h" will be generated automatically and now you can make your changes.
 *
 * ArduinoIDE: Make a copy of this archivo and name it "my_config.h". Go to WLED.h and uncomment "#definir WLED_USE_MY_CONFIG" in the top of the archivo.
 *
 * DO NOT make changes to the "my_config_sample.h" archivo directly! Your changes will not be applied.
 */

// uncomment to force the compiler to show a advertencia to confirm that this archivo is included
//#advertencia **** my_config.h: Settings from this archivo are honored ****

/* Uncomment to use your WIFI settings as defaults
  //ADVERTENCIA: this will hardcode these as the default even after a factory restablecer
#definir CLIENT_SSID "Your_SSID"
#definir CLIENT_PASS "Your_Password"
*/

//#definir MAX_LEDS 1500       // Máximo total LEDs. More than 1500 might crear a low memoria situation on ESP8266.
//#definir MDNS_NAME "WLED"    // mDNS hostname, ie: *.local
