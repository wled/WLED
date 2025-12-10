# Guía Paso a Paso: Instalar WLED en ESP8266

Esta guía te llevará a través de la instalación completa de WLED en una placa ESP8266 desde cero, incluyendo compilación, flasheo y configuración inicial.

## Tabla de Contenidos

1. [Requisitos](#requisitos)
2. [Paso 1: Preparar el Entorno](#paso-1-preparar-el-entorno)
3. [Paso 2: Descargar WLED](#paso-2-descargar-wled)
4. [Paso 3: Instalar Dependencias](#paso-3-instalar-dependencias)
5. [Paso 4: Configurar Hardware](#paso-4-configurar-hardware)
6. [Paso 5: Compilar el Firmware](#paso-5-compilar-el-firmware)
7. [Paso 6: Preparar la Placa ESP8266](#paso-6-preparar-la-placa-esp8266)
8. [Paso 7: Flashear el Firmware](#paso-7-flashear-el-firmware)
9. [Paso 8: Configuración Inicial](#paso-8-configuración-inicial)
10. [Troubleshooting](#troubleshooting)

---

## Requisitos

### Hardware
- **Placa ESP8266**: NodeMCU v2, Wemos D1 Mini, o similar
- **Cable USB**: Para conectar la placa al PC
- **Tira LED WS2812B (NeoPixel)**: Opcional para pruebas iniciales
- **Fuente de poder**: Para alimentar los LEDs (5V recomendado)
- **Resistor 470Ω**: Para proteger el pin de datos (recomendado)

### Software
- **Python 3.7+**: Descárgalo desde [python.org](https://www.python.org)
- **Git**: Descárgalo desde [git-scm.com](https://git-scm.com)
- **VS Code** (opcional pero recomendado): [code.visualstudio.com](https://code.visualstudio.com)
- **PlatformIO**: Se instala automáticamente en VS Code

### Conocimientos Básicos
- Familiaridad con terminal/línea de comandos
- Conceptos básicos de GPIO y USB serial

---

## Paso 1: Preparar el Entorno

### 1.1 Instalar Python
Verifica que Python 3.7+ está instalado:

```bash
python --version
# o en Linux/Mac:
python3 --version

sudo apt update
sudo apt install python3.7
sudo apt install python3

# powershell
winget search Python
winget install Python.Python.3.9

git config --global user.name "jlc"
git config --global user.email "jlc@jlc.es"

```

Si necesitas instalarlo, descárgalo desde [python.org](https://python.org) y sigue el instalador.

### 1.2 Instalar Git
Verifica que Git está instalado:

```bash
git --version
```

Si no está instalado, descárgalo desde [git-scm.com](https://git-scm.com).

### 1.3 Instalar VS Code y PlatformIO (Recomendado)

**Opción A: Usando VS Code + PlatformIO (Recomendado)**

1. Descarga [VS Code](https://code.visualstudio.com)
2. Abre VS Code
3. Ve a **Extensiones** (Ctrl+Shift+X o Cmd+Shift+X)
4. Busca "PlatformIO IDE"
5. Haz clic en **Instalar**
6. Reinicia VS Code

**Opción B: Instalación manual de PlatformIO CLI**

```bash
pip install platformio
```

---

## Paso 2: Descargar WLED

### 2.1 Clonar el Repositorio

Abre una terminal y ejecuta:

```bash
git clone https://github.com/Aircoookie/WLED.git
git clone https://github.com/erpepe2004/WLED.git
cd WLED
```

### 2.2 Verificar la Descarga

Comprueba que los archivos se descargaron correctamente:

```bash
ls -la
# Deberías ver: wled00/, platformio.ini, README.md, etc.
```

---

## Paso 3: Instalar Dependencias

### 3.1 Instalar Python packages

```bash
# Windows
pip install -r requirements.txt

# Linux/Mac
pip3 install -r requirements.txt
```

### 3.2 Instalar Node.js (para compilar la interfaz web)

Descarga Node.js 20+ desde [nodejs.org](https://nodejs.org) o usa tu gestor de paquetes:

**Linux/Mac:**
```bash
# Usando brew (Mac)
brew install node

# Usando apt (Ubuntu/Debian)
sudo apt update && sudo apt install nodejs npm
```

**Windows:**
Descarga el instalador desde [nodejs.org](https://nodejs.org) y ejecuta.

### 3.3 Instalar dependencias de Node.js

```bash
npm install
```

---

## Paso 4: Configurar Hardware

### 4.1 Conectar los LEDs

**Conexión básica:**
- **Din (Datos)** del LED → Pin GPIO4 (D2 en NodeMCU) + resistor 470Ω
- **GND** del LED → GND del ESP8266
- **+5V** del LED → +5V desde fuente de poder

**Diagrama de conexión (NodeMCU v2):**
```
ESP8266 NodeMCU      WS2812B LED
─────────────────────────────────
GND ─────────────────┼─ GND
D2 (GPIO4) ──470Ω───┤ Din
              +5V ───┼─ +5V (desde fuente separada)
```

### 4.2 Verificar conexión física

1. Conecta el ESP8266 al PC mediante el cable USB
2. Verifica que el LED de la placa se enciende
3. Verifica que el puerto USB es reconocido

---

## Paso 5: Compilar el Firmware

### 5.1 Compilar la interfaz web

**Importante: Siempre haz esto antes de compilar el firmware**

```bash
npm run build
```

**Salida esperada:**
```
> WLED@2506160 build
> node tools/cdata.js
...
✓ Successful build
```

### 5.2 Seleccionar la placa ESP8266

Edita el archivo `platformio.ini` y busca la línea `default_envs`:

**Para NodeMCU v2 o similar:**
```ini
default_envs = nodemcuv2
```

**Para Wemos D1 Mini (2MB FLASH):**
```ini
default_envs = esp8266_2m
```

**Para ESP-01 (1MB FLASH):**
```ini
default_envs = esp01_1m_full
```

### 5.3 Compilar el firmware

**Opción A: Usando VS Code**

1. Abre la paleta de comandos (Ctrl+Shift+P o Cmd+Shift+P)
2. Escribe "PlatformIO: Build"
3. Presiona Enter

**Opción B: Usando terminal**

```bash
pio run -e nodemcuv2
```

**Salida esperada (puede tomar 5-15 minutos):**
```
Building in release mode
Compiling .pio/build/nodemcuv2/src/wled.o
...
Linking .pio/build/nodemcuv2/firmware.elf
Building .pio/build/nodemcuv2/firmware.bin
=== [SUCCESS] Took 180 seconds ===
```

### 5.4 Verificar el firmware compilado

El firmware se encuentra en:
```
.pio/build/nodemcuv2/firmware.bin
```

---

## Paso 6: Preparar la Placa ESP8266

### 6.1 Identificar el puerto USB

**Windows:**
```
Abre Administrador de dispositivos → Puertos (COM y LPT)
Busca un puerto llamado "USB-SERIAL CH340" o similar
Anota el número de puerto (ejemplo: COM3)
```

**Linux/Mac:**
```bash
ls /dev/tty.*
# Busca algo como /dev/ttyUSB0, /dev/ttyACM0, o /dev/cu.wchusbserial*
```

### 6.2 Instalar drivers USB (si es necesario)

Si la placa no aparece en el administrador de dispositivos:

- **NodeMCU v2**: Necesita driver CH340
  - Descárgalo desde [ch340g.com](http://www.wch.cn/downloads/CH341SER_EXE.html)
  - Instala y reinicia

- **Wemos D1 Mini**: A menudo funciona sin driver

### 6.3 Limpiar la memoria de la placa (opcional pero recomendado)

```bash
# Borrar toda la memoria de la placa
esptool.py --port COM3 erase_flash

# En Linux/Mac:
esptool.py --port /dev/ttyUSB0 erase_flash
```

**Salida esperada:**
```
esptool.py v3.3.2
Serial port COM3
Connecting....
Chip is ESP8266
Features: WiFi
Erasing flash (this may take a while)...
Chip erase completed successfully in 8.5s
```

---

## Paso 7: Flashear el Firmware

### 7.1 Flashear usando VS Code (Recomendado)

1. Abre la paleta de comandos (Ctrl+Shift+P o Cmd+Shift+P)
2. Escribe "PlatformIO: Upload"
3. Selecciona el puerto USB correcto si te lo pregunta
4. Presiona Enter

**Salida esperada:**
```
Uploading .pio/build/nodemcuv2/firmware.bin
...
Writing at 0x00010000... (90 %)
Writing at 0x00040000... (100 %)
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
=== [SUCCESS] Took 25 seconds ===
```

### 7.2 Flashear usando terminal

```bash
# Windows
pio run -e nodemcuv2 --target upload -s

# Linux/Mac
pio run -e nodemcuv2 --target upload
```

### 7.3 Verificación

1. El LED de la placa debe parpadear durante el flasheo
2. La placa se reiniciará automáticamente al terminar
3. No desconectes la placa durante el proceso

---

## Paso 8: Configuración Inicial

### 8.1 Conectar a WiFi

1. Busca una red WiFi llamada "WLED-AP" (Access Point)
2. Conéctate a ella (sin contraseña o contraseña por defecto)
3. Abre un navegador y ve a `http://4.3.2.1` o `http://192.168.4.1`

### 8.2 Configurar red WiFi permanente

1. En la interfaz web, ve a **Configuración** (⚙️)
2. Selecciona **WiFi**
3. Busca y selecciona tu red WiFi
4. Ingresa la contraseña
5. Haz clic en **Guardar**

### 8.3 Encontrar la dirección IP

Después de conectarse a tu red WiFi:

**Opción A: Desde el router**
- Accede a la configuración del router
- Busca "clientes WiFi" o "dispositivos conectados"
- Busca un dispositivo llamado "WLED" o "esp8266"

**Opción B: Usar mDNS (Recomendado)**
- Ve a `http://wled.local` en tu navegador
- O busca `http://wled-[MAC_ADDRESS].local`

**Opción C: Usar puerto serial**
```bash
# Abre la consola serial en VS Code
# Ve a View → Terminal, luego abre la pestaña "PORTS"
# Busca mensajes que muestren la IP asignada
```

### 8.4 Acceder a la interfaz web

Abre tu navegador y ve a:
```
http://wled.local
# o
http://[IP_DEL_ESP8266]
# ejemplo: http://192.168.1.100
```

### 8.5 Configurar los LEDs

1. Ve a **Configuración** → **Configuración de LED**
2. Selecciona:
   - **Tipo de LED**: WS2812b (NeoPixel)
   - **Pin de datos**: GPIO4 (D2)
   - **Cantidad de LEDs**: El número de LEDs en tu tira
3. Haz clic en **Guardar y recargar**

### 8.6 Prueba básica

1. Vuelve a la página principal
2. Deberías ver el control de color
3. Cambia el color y verifica que los LEDs se encienden
4. Prueba algunos efectos desde el menú de efectos

---

## Troubleshooting

### El ESP8266 no aparece en el puerto USB

**Solución:**
1. Prueba un cable USB diferente (algunos son solo de carga)
2. Instala el driver CH340 si usas NodeMCU
3. Reinicia VS Code y el PC
4. Intenta con un puerto USB diferente

### Error: "Timed out waiting for packet header"

**Causa:** El puerto USB no está correctamente seleccionado o el driver no está instalado.

**Solución:**
```bash
# Lista los puertos disponibles
# Windows: mode COM3 (reemplaza COM3 con tu puerto)
# Linux: ls /dev/ttyUSB*

# Intenta seleccionar el puerto manualmente en VS Code:
# Paleta de comandos → "PlatformIO: Select Port"
```

### Error: "error: espcomm_open failed"

**Causa:** El ESP8266 no responde o está en modo de bajo consumo.

**Solución:**
1. Presiona el botón RESET de la placa
2. O desconecta y reconecta el cable USB
3. Intenta el flasheo nuevamente

### Los LEDs no encienden

**Verificar:**
1. ¿Está correctamente alimentado el LED?
2. ¿Está correctamente solicitado el GPIO4 en configuración?
3. ¿La dirección IP del LED es correcta?

**Soluciones:**
```bash
# Accede a la consola serial para ver errores:
# En VS Code: View → Terminal, pestaña "PORTS"
# Comprueba que ves mensajes de inicialización

# O flashea con loglevel DEBUG:
# Edita platformio.ini y añade al build_flags:
# -DWLED_DEBUG
```

### La placa se conecta a WiFi pero no responde por IP

**Solución:**
1. Verifica la IP del ESP8266 en tu router
2. Intenta acceder con `http://wled.local`
3. Abre el puerto 80 en el firewall si es necesario
4. Reinicia la placa desde la interfaz web: **Configuración** → **Sistema** → **Reiniciar**

### Compilación falla con "error: expected '}' before end of file"

**Causa:** Archivo dañado durante compilación previa.

**Solución:**
```bash
# Limpia los archivos compilados:
pio run --target clean

# Luego recompila:
pio run -e nodemcuv2
```

### El ESP8266 arranca pero se apaga constantemente

**Causa:** Probablemente falta de alimentación o brownout.

**Solución:**
1. Usa una fuente de poder más potente (mínimo 500mA)
2. Añade un capacitor de 470µF entre +5V y GND en los LEDs
3. Edita `wled00/wled.h` y busca `WLED_DISABLE_BROWNOUT_DET`
4. Recompila si es necesario

---

## Comandos Rápidos de Referencia

```bash
# Clonar WLED
git clone https://github.com/Aircoookie/WLED.git

# Instalar dependencias
npm install && pip install -r requirements.txt

# Compilar Web UI
npm run build

# Compilar firmware para NodeMCU v2
pio run -e nodemcuv2

# Flashear firmware
pio run -e nodemcuv2 --target upload

# Limpiar build cache
pio run --target clean

# Ver logs en tiempo real
pio device monitor --port COM3 --baud 115200
```

---

## Próximos Pasos

Ahora que tienes WLED funcionando en tu ESP8266:

1. **Explora efectos**: Prueba diferentes efectos en la pantalla principal
2. **Crea presets**: Guarda tus combinaciones favoritas
3. **Configura automatización**: Ve a **Configuración** → **Automatización**
4. **Integra con Home Assistant**: Consulta [INTEGRACION_HOMEASSISTANT_ES.md](INTEGRACION_HOMEASSISTANT_ES.md)
5. **Controla por API**: Consulta [API_REFERENCIA_ES.md](API_REFERENCIA_ES.md)

---

## Recursos Adicionales

- **Documentación oficial**: [WLED GitHub](https://github.com/Aircoookie/WLED)
- **Documentación WLED en español**: [DOCUMENTACION_ES.md](DOCUMENTACION_ES.md)
- **Foro WLED**: [GitHub Discussions](https://github.com/Aircoookie/WLED/discussions)
- **Discord WLED**: [Discord Server](https://discord.gg/wled)

---

**¿Necesitas ayuda?** Consulta la sección [Troubleshooting](#troubleshooting) o abre un issue en el repositorio.

Última actualización: Diciembre 2025
