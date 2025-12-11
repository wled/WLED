# WLED - Documentación Completa en Español

## 📋 Tabla de Contenidos
1. [Funcionamiento](#funcionamiento)
2. [Compilación](#compilación)
3. [Configuración](#configuración)
4. [Personalización](#personalización)

---

## Funcionamiento

### ¿Qué es WLED?

WLED es un controlador de LED altamente optimizado basado en microcontroladores ESP32 y ESP8266. Proporciona una interfaz web moderna para controlar tiras de LEDs direccionables como:
- **NeoPixel**: WS2812B, WS2811, SK6812
- **SPI basados**: WS2801, APA102

### Características Principales

#### Efectos y Animaciones
- **100+ efectos especiales** basados en WS2812FX
- **50 paletas de color** personalizables
- **Efectos de ruido** de FastLED para variaciones naturales

#### Control de Segmentos
WLED permite dividir una tira de LEDs en múltiples "segmentos", donde cada uno puede tener:
- Color independiente
- Efecto diferente
- Velocidad y brillo propios
- Configuración única de paleta

Ejemplo: Una tira de 300 LEDs puede tener 3 segmentos:
- Segmento 1 (LEDs 0-99): Efecto Rainbow con paleta Cool
- Segmento 2 (LEDs 100-199): Color sólido rojo
- Segmento 3 (LEDs 200-299): Efecto Sparkle con paleta Fire

#### Interfaz de Usuario
- **Web UI responsive**: Funciona en computadoras, tablets y teléfonos
- **Controles intuitivos**: Selector de color (iro.js), deslizadores de brillo y velocidad
- **Página de configuración**: Acceso a todas las opciones del sistema

#### Soporte de Múltiples Salidas
- Hasta **10 salidas de LED simultáneas** en ESP32
- Control independiente de cada salida
- Soporte para diferentes tipos de LEDs en la misma placa

#### Presets de Usuario
- Guardar hasta **250 presets** de color y efecto
- Ciclo automático entre presets
- Ejecución automática de comandos API

### Interfaces de Control Soportadas

| Interfaz | Descripción |
|----------|-------------|
| **Aplicación WLED** | Apps nativas para Android e iOS |
| **API JSON/HTTP** | Control programático vía REST API |
| **MQTT** | Protocolo IoT para automatización |
| **E1.31/Art-Net** | Protocolos profesionales de iluminación |
| **UDP en tiempo real** | Sincronización de bajo latency |
| **Alexa** | Control de voz (requiere configuración) |
| **Philips Hue** | Sincronización con ecosistema Hue |
| **Controles IR** | Mandos de 24 teclas RGB |
| **Adalight** | Ambilight de PC vía puerto serie |

### Arquitectura Interna

```
┌─────────────────────────────────────────┐
│          INTERFAZ WEB (Web UI)          │
│  ┌───────────────────────────────────┐  │
│  │ - HTML (index.htm, settings.htm)  │  │
│  │ - CSS (estilos)                   │  │
│  │ - JavaScript (lógica del cliente) │  │
│  └───────────────────────────────────┘  │
└────────────────┬────────────────────────┘
                 │
        ┌────────▼──────────┐
        │   JSON API / WS   │
        │   Protocolo HTTP  │
        └────────┬──────────┘
                 │
┌────────────────▼──────────────────────────┐
│      FIRMWARE C++ (en ESP32/ESP8266)      │
│ ┌──────────────────────────────────────┐  │
│ │ - Sistema de Efectos (FX.cpp)        │  │
│ │ - Gestor de Bus LED (bus_manager.h)  │  │
│ │ - Protocolo MQTT, UDP, E1.31, etc    │  │
│ │ - Sistema de Presets (config)        │  │
│ │ - Sistema de Usermods (plugins)      │  │
│ └──────────────────────────────────────┘  │
└────────────────┬───────────────────────────┘
                 │
        ┌────────▼──────────┐
        │  GPIO del ESP32   │
        └────────┬──────────┘
                 │
        ┌────────▼──────────┐
        │   TIRAS DE LED    │
        │  (WS2812, etc)    │
        └───────────────────┘
```

### Flujo de Ejecución

1. **Inicio del dispositivo**: El ESP carga la configuración desde EEPROM
2. **Conexión de red**: Se conecta a WiFi (o activa modo AP)
3. **Servidor web**: Inicia el servidor HTTP en puerto 80
4. **Loop principal**: Continuamente:
   - Lee entrada de usuario (app, web, MQTT, etc)
   - Actualiza el estado de segmentos
   - Calcula los colores para cada efecto
   - Envía datos a los LEDs

### Consumo de Memoria

WLED está optimizado para dispositivos embebidos:
- **ESP8266**: Requiere ~2MB de flash (versión completa)
- **ESP32**: Puede usar toda la capacidad disponible

La memoria se usa para:
- Firmware C++ (~500KB-1MB)
- Interfaz web incrustada (~200-400KB)
- Almacenamiento de configuración
- Buffer de datos en tiempo real

---

## Compilación

### Requisitos Previos

#### 1. Software Necesario

**Node.js 20+** (para compilar la interfaz web)
```bash
# Verificar versión
node --version

# Si no está instalado, usar nvm (Node Version Manager)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
nvm install 20
nvm use 20
```

**Python 3.8+** (para PlatformIO)
```bash
python3 --version
```

**PlatformIO** (compilador para ESP)
```bash
# Instalar vía pip
pip install -r requirements.txt

# O instalarlo globalmente
pip install platformio
```

#### 2. Dependencias del Proyecto

```bash
# Instalar dependencias Node.js
npm ci

# Instalar dependencias Python
pip install -r requirements.txt
```

### Estructura de Compilación

WLED tiene un proceso de dos fases:

#### Fase 1: Compilación de Interfaz Web

**Comando**: `npm run build`

Procesa archivos en `wled00/data/`:
- Minifica HTML, CSS, JavaScript
- Comprime archivos con gzip
- Genera archivos de encabezado C++ (`html_*.h`)
- Incrusta todo en el firmware

**Tiempo**: ~3 segundos
**Obligatorio antes de compilar el firmware**

```bash
cd /workspaces/WLED
npm run build
```

Archivos generados:
- `wled00/html_ui.h` - Interfaz principal
- `wled00/html_settings.h` - Páginas de configuración
- `wled00/html_other.h` - Otros archivos

**⚠️ IMPORTANTE**: Nunca edites directamente los archivos `html_*.h`. Siempre modifica los archivos fuente en `wled00/data/` y reconstruye.

#### Fase 2: Compilación de Firmware

**Comando**: `pio run -e [entorno]`

Compila el código C++ para el ESP32/ESP8266

**Tiempo**: 15-20 minutos (primera compilación)
**Entornos disponibles**:

```
ESP8266 (WiFi 802.11b/g/n, 80-160 MHz):
  - nodemcuv2           → NodeMCU v2 (4MB flash)
  - esp01_1m_full       → ESP-01S (1MB flash)
  - esp8266_2m          → Genérico 2MB flash

ESP32 (WiFi dual-band, BLE, 240 MHz):
  - esp32dev            → DevKit ESP32 estándar
  - esp32_eth           → ESP32 con Ethernet
  - esp32_wrover        → ESP32-WROVER (PSRAM)

ESP32-S3 (Dual-core, mejor rendimiento):
  - esp32S3_wroom2      → ESP32-S3-WROOM
  - esp32s3dev_16MB_opi → ESP32-S3 DevKit 16MB
  
ESP32-C3 (RISC-V, bajo costo):
  - esp32c3dev          → ESP32-C3 DevKit

Custom:
  - usermods            → Compilación con usermods
```

**Compilar esp32dev**:
```bash
pio run -e esp32dev
```

**Listar todos los entornos**:
```bash
pio run --list-targets
```

### Proceso Completo de Compilación

```bash
# 1. Clonar/descargar WLED
git clone https://github.com/wled-dev/WLED.git
cd WLED

# 2. Instalar dependencias
npm ci                    # Node.js
pip install -r requirements.txt  # Python/PlatformIO

# 3. Compilar interfaz web (OBLIGATORIO)
npm run build

# 4. Ejecutar pruebas (opcional pero recomendado)
npm test

# 5. Compilar firmware para tu placa
pio run -e esp32dev       # Cambiar esp32dev por tu placa

# 6. Flashear a dispositivo (opcional)
pio run -e esp32dev --target upload
```

### Modo Desarrollo

Para desarrollo activo con cambios automáticos:

```bash
# Terminal 1: Monitorear cambios en interfaz web
npm run dev

# Terminal 2: Compilar firmware cuando sea necesario
pio run -e esp32dev
```

Cuando edites archivos en `wled00/data/`, `npm run dev`:
- Reconstruye automáticamente `html_*.h`
- No necesitas ejecutar `npm run build` manualmente

### Opciones de Compilación Avanzadas

#### Compilar con Usermods Personalizados

Crear archivo `platformio_override.ini`:

```ini
[env:custom_build]
extends = esp32dev
custom_usermods = my_usermod,another_usermod
build_flags =
  ${esp32dev.build_flags}
  -DWLED_ENABLE_CUSTOM_FEATURE
```

Ejecutar:
```bash
pio run -e custom_build
```

#### Deshabilitar Características

En `wled00/wled.h`:

```cpp
// Deshabilitar MQTT
#define WLED_DISABLE_MQTT

// Deshabilitar E1.31
#define WLED_DISABLE_E131

// Deshabilitar ALEXA
#define WLED_DISABLE_ALEXA
```

#### Compilar Versión Simplificada (ESP8266)

Para ESP8266 con espacio limitado:

```bash
pio run -e esp01_1m_full
```

Recuerda compilar la interfaz web primero:
```bash
npm run build
```

### Solución de Problemas de Compilación

| Problema | Solución |
|----------|----------|
| Error: `html_*.h` no encontrado | Ejecutar `npm run build` |
| PlatformIO no encontrado | `pip install platformio` |
| Node.js versión incorrecta | Usar `nvm use 20` |
| Falla en descarga de herramientas | Reintentar `pio run`, puede fallar por red |
| Memoria insuficiente | Deshabilitar features en `wled.h` |
| Puerto USB no detectado | Instalar drivers CH340/CP2102 |

---

## Configuración

### Acceso Inicial

#### 1. Conexión por Primera Vez

**Desde un ESP sin configurar**:
1. El dispositivo crea un punto de acceso (AP)
2. Nombre: `WLED-XXXXXX` (X = números aleatorios)
3. Sin contraseña
4. Abre http://192.168.4.1 en tu navegador

**Conectar a WiFi existente**:
1. En la interfaz web: Gear icon → Settings → WiFi
2. Selecciona tu red
3. Ingresa contraseña
4. Reinicia el dispositivo
5. Conéctate a la IP que asignó tu router

#### 2. Interfaz Web Principal

```
┌─────────────────────────────────────────────┐
│  WLED v2506160 | 192.168.1.100              │
├─────────────────────────────────────────────┤
│                                             │
│  🎨 Selector de Color  🔆 Brillo: [====]  │
│                                             │
│  📊 Efecto: Rainbow Cycle    ⚡ Velocidad  │
│                                             │
│  🎛️  Intensidad: [====]  💫 Paleta: Cool  │
│                                             │
│  ⏰ Temporizador    📝 Presets    ⚙️ Config │
│                                             │
└─────────────────────────────────────────────┘
```

**Controles principales**:
- **Color**: Selector interactivo para cambiar color
- **Brillo**: Volumen global de los LEDs (0-255)
- **Efecto**: Selecciona de 100+ efectos disponibles
- **Velocidad**: Qué tan rápido se ejecuta el efecto
- **Intensidad**: Densidad del efecto (depende del efecto)
- **Paleta**: Conjunto de colores para el efecto

### Configuración de Hardware

#### 1. Pines GPIO

**Ir a**: Settings → LED Preferences → Pin configuration

```
┌────────────────────────────────────────┐
│  LED Output 1                          │
│  GPIO Pin:  [_____]  (ej: 5, 16, etc) │
│  Type:      [Dropdown] (NeoPixel, etc) │
│  Start LED: [_____]   (0 para inicio) │
│  Count:     [_____]   (número de LEDs)│
│  Color Order: [Dropdown] (RGB, GRB)   │
│  Skip First LED: [checkbox]           │
└────────────────────────────────────────┘
```

**Pines recomendados por placa**:

**ESP32 DevKit**:
- GPIO 5: Pin D5 (salida recomendada 1)
- GPIO 16: Pin D16 (salida 2)
- GPIO 17: Pin D17 (salida 3)
- GPIO 4: Pin D4 (salida 4)

**ESP8266 (NodeMCU)**:
- GPIO 5 (D1): Salida recomendada
- GPIO 4 (D2): Alternativa

**ESP-01S**:
- GPIO 0 o 2: Única opción (limitaciones)

#### 2. Configuración de Segmentos

**Ir a**: Settings → LED Preferences → LED Layout

```
┌─────────────────────────────────────────┐
│  Segment 0 (Segmento 0)                 │
│  Start LED:    0                        │
│  End LED:      99   (100 LEDs)          │
│  Off: [checkbox]                        │
│  Reverse: [checkbox]                    │
│  Grouping: 1  (1 LED por efecto)       │
│  Spacing: 0   (sin espacios)           │
└─────────────────────────────────────────┘
│  
│  [+ Add Segment] [Save]
```

**Opciones por segmento**:
- **Start/End LED**: Rango de LEDs del segmento
- **Reverse**: Invierte la dirección de animación
- **Grouping**: Agrupa N LEDs como una unidad
- **Spacing**: Salta LEDs entre grupos

#### 3. Sincronización de Red

**Ir a**: Settings → Sync → Realtime

```
┌────────────────────────────────────────┐
│  UDP Realtime                          │
│  Status: [Enabled/Disabled]            │
│  IP Send To: [192.168.1.100]          │
│  UDP Port: 21324 (por defecto)        │
│                                        │
│  Sync Receive: [checkbox]              │
│  Generic UDP: [checkbox]               │
│  ArtNet: [checkbox]                    │
│  DDP: [checkbox]                       │
│  E1.31/sACN: [checkbox]                │
└────────────────────────────────────────┘
```

### Configuración de Red y WiFi

**Ir a**: Settings → WiFi Setup

```
┌────────────────────────────────────────┐
│  WiFi Network                          │
│  SSID: [_________________]             │
│  Password: [_________________]         │
│  Static IP: [checkbox]                 │
│  │ IP: [192.168.1.100]                │
│  │ Netmask: [255.255.255.0]           │
│  │ Gateway: [192.168.1.1]             │
│                                        │
│  Apply [Button] Reset [Button]        │
└────────────────────────────────────────┘
```

**Modos de conexión**:
1. **Modo Estación**: Conectado a tu WiFi
2. **Modo AP**: Dispositivo actúa como punto de acceso
3. **Fallback automático**: Si falla WiFi, crea AP

### Configuración de Seguridad

**Ir a**: Settings → Security

```
┌────────────────────────────────────────┐
│  OTA Password: [_________________]    │
│  (Necesario para actualizaciones OTA) │
│                                        │
│  API Security: [checkbox]              │
│  (Requiere API key para cambios)      │
│                                        │
│  Default for new presets:              │
│  □ Public  □ Protected  ☑ Private     │
└────────────────────────────────────────┘
```

### Configuración de Servicios

#### MQTT

**Ir a**: Settings → Sync → MQTT

```
┌────────────────────────────────────────┐
│  MQTT Broker Address: [_____________]  │
│  Port: [1883]                          │
│  User: [_________________]             │
│  Password: [_________________]         │
│  Client ID: WLED-[MAC]                │
│  Topic: wled/[MAC]/                   │
└────────────────────────────────────────┘
```

**Tópicos disponibles**:
- `wled/[MAC]/api` - Enviar comandos JSON
- `wled/[MAC]/status` - Recibir estado actual

#### Alexa

**Ir a**: Settings → Sync → Alexa

```
┌────────────────────────────────────────┐
│  ☑ Enable Alexa Integration            │
│  Device Name: Living Room Lights       │
│                                        │
│  Descubre dispositivo en Alexa App     │
└────────────────────────────────────────┘
```

**Comandos de ejemplo**:
- "Alexa, enciende las luces de la sala"
- "Alexa, sube el brillo de la sala"
- "Alexa, pon las luces rojas"

#### Sensor de Luz

**Ir a**: Settings → LED Preferences → Brightness Limiter

```
┌────────────────────────────────────────┐
│  Automatic Brightness Limit            │
│  ☑ Enabled                             │
│  Max Brightness: [████████░░] 85%      │
│  Mode: □ ESP internal □ Externo (pin) │
└────────────────────────────────────────┘
```

### Sincronización Entre Dispositivos

**Escenario**: Tienes 5 tiras de LED en diferentes habitaciones

**Opción 1: UDP Notifier**
- Dispositivo maestro envía estado
- Otros dispositivos lo reciben
- Todos se sincronizan automáticamente

**Opción 2: MQTT Broker**
- Todos se conectan a servidor central
- Mayor flexibilidad y control

**Configuración UDP**:
1. En dispositivo maestro: Settings → Sync → Realtime → UDP Send
2. Ingresa IP de dispositivo esclavo
3. Esclavo recibe automáticamente

---

## Personalización

### Sistema de Efectos

#### Efectos Disponibles

WLED incluye más de 100 efectos:

**Efectos Clásicos**:
- `Solid` - Color sólido
- `Blink` - Parpadeo
- `Strobe` - Estrobo
- `Color Wipe` - Relleno de color
- `Scan` - Barrido

**Efectos Dinámicos**:
- `Rainbow Cycle` - Arcoíris rotatorio
- `Fire` - Simulación de fuego
- `Colorful` - Patrones coloridos
- `Twinkle` - Centelleo aleatorio
- `Noise` - Ruido Perlin

**Efectos Avanzados**:
- `Matrix` - Efecto Matrix (lluvia código)
- `Ripple` - Ondas desde centro
- `Waves` - Ondas sinusoidales
- `Plasma` - Plasma dinámico

**Para ver la lista completa**: Abre el selector de efectos en la interfaz web

#### Crear Efecto Personalizado

Los efectos se definen en `wled00/FX.cpp`:

```cpp
// Estructura de efecto
uint16_t mode_custom_effect(void) {
  // SEGMENT es la estructura del segmento actual
  // SEGLEN = longitud del segmento
  // SEGMENT.speed = velocidad (0-255)
  // SEGMENT.intensity = intensidad (0-255)
  
  for(int i = 0; i < SEGLEN; i++) {
    // Calcular color del LED i
    uint32_t color = CHSV(i + SEGMENT.speed, 255, 255).rgb();
    setPixelColor(i, color);
  }
  
  return FRAMETIME; // Retorna ms hasta siguiente frame
}
```

Luego registrarlo en `FX.h`:
```cpp
_addMode(mode_custom_effect, "Mi Efecto");
```

### Paletas de Color

#### Paletas Incluidas

- **Cool** - Azules y verdes
- **Fire** - Rojo, naranja, amarillo
- **Ocean** - Tonos acuáticos
- **Rainbow** - Espectro completo
- **Party** - Colores vibrantes

#### Crear Paleta Personalizada

En `wled00/palettes.cpp`:

```cpp
DEFINE_GRADIENT_PALETTE(my_custom_palette) {
    0,    255,  0,  0,  // Rojo puro en 0%
  127,      0,255,  0,  // Verde en 50%
  255,      0,  0,255   // Azul en 100%
};
```

**Guía de colores RGB**:
- Rojo: (255, 0, 0)
- Verde: (0, 255, 0)
- Azul: (0, 0, 255)
- Blanco: (255, 255, 255)
- Negro: (0, 0, 0)

### Sistema de Usermods (Plugins)

#### ¿Qué son los Usermods?

Usermods son extensiones del firmware que añaden funcionalidades sin modificar el código principal.

**Ejemplos incluidos**:
- `DHT` - Sensor de temperatura/humedad
- `BH1750_v2` - Sensor de luz ambiental
- `PIR_sensor_switch` - Sensor de movimiento
- `multi_relay` - Múltiples relés
- `audioreactive` - Efectos reactivos al audio

#### Crear Usermod Personalizado

**Opción 1: Usermod V1 (Simple)**

En `wled00/usermods_list.cpp`:

```cpp
// En userSetup()
void userSetup() {
  Serial.println("Mi usermod iniciado");
}

// En userConnected()
void userConnected() {
  Serial.println("WiFi conectado");
}

// En userLoop() - llamado continuamente
void userLoop() {
  // Tu código aquí
  // Se ejecuta frecuentemente
}
```

**Opción 2: Usermod V2 (Recomendado)**

Crear archivo `usermods/my_usermod/usermod.cpp`:

```cpp
#include "wled.h"

class MyUsermod : public Usermod {
public:
  void setup() override {
    Serial.println("Setup del usermod");
  }
  
  void connected() override {
    Serial.println("Conectado a red");
  }
  
  void loop() override {
    // Se ejecuta continuamente
  }
  
  void addToConfig(JsonObject& root) override {
    // Agregar configuración a JSON
  }
  
  bool readFromConfig(JsonObject& root) override {
    // Leer configuración desde JSON
    return true;
  }
  
  uint16_t getId() override {
    return USERMOD_ID_MY_USERMOD;
  }
};
```

Registrar en `wled00/usermods_list.cpp`:
```cpp
registerUsermod(new MyUsermod());
```

#### Compilar con Usermods

```bash
# Copiar usermod a carpeta
cp -r my_usermod wled00/usermods/

# Crear platformio_override.ini
cat > platformio_override.ini << EOF
[env:esp32_custom]
extends = esp32dev
custom_usermods = my_usermod
EOF

# Compilar
npm run build
pio run -e esp32_custom
```

### Personalización de Interfaz Web

#### Estructura de la Interfaz

```
wled00/data/
├── index.htm          → Página principal
├── settings*.htm      → Páginas de configuración
├── css/
│   ├── style.css      → Estilos principales
│   └── color.css      → Estilos de colores
├── js/
│   ├── common.js      → Funciones comunes
│   ├── ui.js          → Lógica de interfaz
│   └── e131.js        → Protocolo E1.31
└── lib/               → Librerías externas
    └── iro.js         → Selector de color
```

#### Modificar Interfaz

**Cambiar colores**:
Editar `wled00/data/css/color.css`:

```css
:root {
  --c-primary: #00d4ff;   /* Azul ciano */
  --c-secondary: #00ff00; /* Verde */
  --c-warning: #ffaa00;   /* Naranja */
}
```

**Agregar botón personalizado**:
En `wled00/data/index.htm`:

```html
<button onclick="miAccion()">Mi Botón</button>

<script>
function miAccion() {
  // Cambiar a efecto específico
  requestJson({effect: 5, bri: 200});
}
</script>
```

**Funciones útiles de JavaScript**:

```javascript
// Obtener elemento
gId("id_elemento")

// Crear elemento
cE("div", "clase", html)

// Enviar comando JSON al servidor
requestJson({
  on: true,
  bri: 255,
  effect: 10,
  col: [[255,0,0]]  // [R,G,B]
})

// Obtener color actual
let r = csel[0], g = csel[1], b = csel[2];

// Cambiar segmento actual
setSegmentMode(0, 10);  // Segmento 0, efecto 10
```

#### Compilar Cambios de Interfaz

Después de editar archivos en `wled00/data/`:

```bash
# Reconstruir headers C++
npm run build

# Compilar firmware con cambios
pio run -e esp32dev
```

### Presets Avanzados

#### JSON Structure

Los presets se guardan en formato JSON:

```json
{
  "seg": [{
    "id": 0,
    "on": true,
    "bri": 255,
    "col": [
      [255, 0, 0],      // RGB primario
      [0, 255, 0],      // RGB secundario  
      [0, 0, 255]       // RGB terciario
    ],
    "fx": 5,            // Efecto (índice)
    "sx": 100,          // Velocidad efecto
    "ix": 128           // Intensidad efecto
  }]
}
```

#### Crear Preset por API

```bash
# Guardar preset actual como #2
curl -X POST http://192.168.1.100/json/state -d '{
  "v": true,
  "psave": 2
}'

# Cargar preset #2
curl -X POST http://192.168.1.100/json/state -d '{
  "ps": 2
}'

# Ciclado automático
curl -X POST http://192.168.1.100/json/state -d '{
  "psave": 1,
  "pss": 2,  // Segundos entre presets
  "psf": 10  // Fade duration
}'
```

### Variables Globales Importantes

En la interfaz web (`common.js`):

```javascript
isOn        // true si LEDs están encendidos
bri         // Brillo actual (0-255)
selectedFx  // Índice del efecto seleccionado
selectedPal // Índice de la paleta seleccionada
csel        // Color seleccionado [R,G,B]
segCount    // Número de segmentos
nlA         // Nightlight activo
```

### Órdenes de Color LED

Diferentes LEDs usan diferentes órdenes de color:

| Tipo | Orden | Uso |
|------|-------|-----|
| RGB | Rojo→Verde→Azul | NeoPixels estándar |
| GRB | Verde→Rojo→Azul | WS2812B más común |
| BRG | Azul→Rojo→Verde | Algunos SK6812 |
| RBG | Rojo→Azul→Verde | Menos común |

**Configurar en Settings → LED Preferences → Color Order**

Si los colores se ven incorrectos, prueba diferentes órdenes.

---

## 🔗 Recursos Adicionales

### Documentación Oficial
- [Wiki WLED](https://kno.wled.ge)
- [Foro Discourse](https://wled.discourse.group)
- [Discord oficial](https://discord.gg/QAh7wJHrRM)

### Herramientas
- [Configurador JSON online](https://wled.me)
- [API explorer](https://www.3-d.ch/wledtools/)
- [Programa para desktop](https://github.com/Aircoookie/WLED-App)

### APIs Útiles

**Obtener estado actual**:
```bash
curl http://192.168.1.100/json/state
```

**Cambiar color**:
```bash
curl -X POST http://192.168.1.100/json/state -d '{"col":[[255,0,0]]}'
```

**Cambiar efecto**:
```bash
curl -X POST http://192.168.1.100/json/state -d '{"effect":10}'
```

### Solución de Problemas

| Problema | Solución |
|----------|----------|
| No se ven los LEDs | Verificar pines GPIO, orden de color |
| WiFi no conecta | Reiniciar dispositivo, verificar SSID/password |
| Interfaz muy lenta | Usar dispositivo con mejor WiFi o cableado Ethernet |
| Efectos entrecortados | Reducir número de LEDs o deshabilitar otros servicios |
| MQTT no funciona | Verificar dirección broker, usuario, contraseña |

### Especificaciones Técnicas

**ESP32**:
- Procesador: Dual-core Xtensa 240 MHz
- RAM: 520 KB
- Flash: 4-16 MB típico
- WiFi: 802.11 b/g/n 2.4 GHz
- Pines: ~30 GPIO disponibles
- LEDs soportados: Hasta 10,000 LEDs con 10 outputs

**ESP8266**:
- Procesador: Xtensa 80-160 MHz
- RAM: 160 KB
- Flash: 1-4 MB típico
- WiFi: 802.11 b/g/n 2.4 GHz
- Pines: ~11 GPIO disponibles
- LEDs soportados: Hasta 1,500 LEDs

---

## 📝 Licencia

WLED está licensed bajo EUPL v1.2. Ver [LICENSE](LICENSE) para detalles.

Creado originalmente por [Aircoookie](https://github.com/Aircoookie)

---

**Última actualización**: Diciembre 2025
