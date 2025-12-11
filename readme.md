<p align="center">
  <img src="/images/wled_logo_akemi.png">
  <a href="https://github.com/wled-dev/WLED/releases"><img src="https://img.shields.io/github/release/wled-dev/WLED.svg?style=flat-square"></a>
  <a href="https://raw.githubusercontent.com/wled-dev/WLED/main/LICENSE"><img src="https://img.shields.io/github/license/wled-dev/wled?color=blue&style=flat-square"></a>
  <a href="https://wled.discourse.group"><img src="https://img.shields.io/discourse/topics?colorB=blue&label=forum&server=https%3A%2F%2Fwled.discourse.group%2F&style=flat-square"></a>
  <a href="https://discord.gg/QAh7wJHrRM"><img src="https://img.shields.io/discord/473448917040758787.svg?colorB=blue&label=discord&style=flat-square"></a>
  <a href="https://kno.wled.ge"><img src="https://img.shields.io/badge/quick_start-wiki-blue.svg?style=flat-square"></a>
  <a href="https://github.com/Aircoookie/WLED-App"><img src="https://img.shields.io/badge/app-wled-blue.svg?style=flat-square"></a>
  <a href="https://gitpod.io/#https://github.com/wled-dev/WLED"><img src="https://img.shields.io/badge/Gitpod-ready--to--code-blue?style=flat-square&logo=gitpod"></a>

  </p>

# ¡Bienvenido a WLED! ✨

Una implementación rápida y rica en características de un servidor web ESP32 y ESP8266 para controlar LEDs NeoPixel (WS2812B, WS2811, SK6812) o también chipsets basados en SPI como el WS2801 y APA102.

Creado originalmente por [Aircoookie](https://github.com/Aircoookie)

## ⚙️ Características
- Librería WS2812FX con más de 100 efectos especiales  
- Efectos de ruido de FastLED y 50 paletas  
- Interfaz moderna con controles de color, efecto y segmento  
- Segmentos para establecer diferentes efectos y colores en partes definidas por el usuario de la tira de LEDs  
- Página de configuración - configuración a través de la red  
- Modo Punto de Acceso y estación - AP de conmutación por error automática  
- [Hasta 10 salidas de LED](https://kno.wled.ge/features/multi-strip/#esp32) por instancia
- Soporte para tiras RGBW  
- Hasta 250 presets de usuario para guardar y cargar colores/efectos fácilmente, admite ciclar a través de ellos.  
- Los presets se pueden usar para ejecutar automáticamente llamadas de API  
- Función de luz nocturna (se atenúa gradualmente)  
- Actualizabilidad completa del software OTA (HTTP + ArduinoOTA), protegible por contraseña  
- Reloj analógico configurable (soporte de reloj Cronixie, pantalla de 7 segmentos y EleksTube IPS a través de usermods) 
- Límite de brillo automático configurable para operación segura  
- Configuración basada en sistema de archivos para copia de seguridad más fácil de presets y configuración  

## 💡 Interfaces de control de luz soportadas
- Aplicación WLED para [Android](https://play.google.com/store/apps/details?id=ca.cgagnier.wlednativeandroid) e [iOS](https://apps.apple.com/gb/app/wled-native/id6446207239)
- APIs JSON y solicitudes HTTP  
- MQTT   
- E1.31, Art-Net, DDP y TPM2.net
- [diyHue](https://github.com/diyhue/diyHue) (Wled es soportado por diyHue, incluido Hue Sync Entertainment bajo udp. Gracias a [Gregory Mallios](https://github.com/gmallios))
- [Hyperion](https://github.com/hyperion-project/hyperion.ng)
- UDP en tiempo real  
- Control de voz de Alexa (incluyendo atenuación y color)  
- Sincronizar con luces Philips hue  
- Adalight (ambilight de PC a través de puerto serie) y TPM2  
- Sincronizar color de múltiples dispositivos WLED (notificador UDP)  
- Controles remotos por infrarrojos (RGB de 24 teclas, receptor requerido)  
- Temporizadores/horarios simples (tiempo de NTP, zonas horarias/DST soportadas)  

## 📲 Guía de inicio rápido y documentación

¡Consulte la [documentación en nuestro sitio oficial](https://kno.wled.ge)!

[En esta página](https://kno.wled.ge/basics/tutorials/) puede encontrar excelentes tutoriales y herramientas para ayudarle a poner su nuevo proyecto en funcionamiento.

## 🖼️ Interfaz de usuario
<img src="/images/macbook-pro-space-gray-on-the-wooden-table.jpg" width="50%"><img src="/images/walking-with-iphone-x.jpg" width="50%">

## 💾 Hardware compatible

¡Vea [aquí](https://kno.wled.ge/basics/compatible-hardware)!

## ✌️ Otros

Licenciado bajo la licencia EUPL v1.2  
Créditos [aquí](https://kno.wled.ge/about/contributors/)!
Proxy CORS por [Corsfix](https://corsfix.com/)

¡Únase al servidor de Discord para discutir todo sobre WLED!

<a href="https://discord.gg/QAh7wJHrRM"><img src="https://discordapp.com/api/guilds/473448917040758787/widget.png?style=banner2" width="25%"></a>

¡Consulte el [foro de Discourse de WLED](https://wled.discourse.group)!  

También puede enviarme correos a [dev.aircoookie@gmail.com](mailto:dev.aircoookie@gmail.com), pero por favor, solo hágalo si desea hablar conmigo en privado.  

Si WLED realmente ilumina tu día, puedes [![](https://img.shields.io/badge/send%20me%20a%20small%20gift-paypal-blue.svg?style=flat-square)](https://paypal.me/aircoookie)

## 🌐 Documentación en Español

¡Bienvenidos usuarios hispanohablantes! Hemos preparado documentación completa en español:

- 📖 **[Documentación Completa](DOCUMENTACION_ES.md)** - Funcionamiento, compilación, configuración y personalización
- 📋 **[Instalación ESP8266 Paso a Paso](INSTALACION_ESP8266_ES.md)** - Guía completa desde cero hasta funcionamiento
- 🔄 **[Actualizar Componentes](ACTUALIZACIONES_COMPONENTES_ES.md)** - Mantener WLED y dependencias actualizadas
- ⚡ **[Guía Rápida](GUIA_RAPIDA_ES.md)** - Setup en 5 minutos y troubleshooting
- 🔌 **[Referencia de API](API_REFERENCIA_ES.md)** - Control programático con ejemplos
- 🛠️ **[Compilación Avanzada](COMPILACION_AVANZADA_ES.md)** - Para desarrolladores y usermods
- 📚 **[Índice General](INDICE_DOCUMENTACION_ES.md)** - Navegación por temas

¿Nuevo en WLED? Comienza con la [Guía Rápida](GUIA_RAPIDA_ES.md) o [Instalación Paso a Paso](INSTALACION_ESP8266_ES.md) 🚀

*Descargo de responsabilidad:*   

Si sufre de epilepsia fotosensible, le recomendamos que **no** use este software.  
Si aún desea intentarlo, no use modos de estrobo, iluminación o ruido o configuraciones de velocidad de efecto alto.

De conformidad con la licencia EUPL, no asumo responsabilidad alguna por daños a usted o cualquier otra persona o equipo.  

