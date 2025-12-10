# Guía Avanzada: Compilación y Personalización del Firmware

## 🔧 Compilación Personalizada

### Compilación con Usermods

Los usermods son extensiones que agregan funcionalidades sin modificar el código principal de WLED.

#### Estructura de un Usermod V2

```cpp
#pragma once
#include "wled.h"

class MyCustomUsermod : public Usermod {
private:
  // Variables privadas
  int myCounter = 0;
  unsigned long lastTime = 0;

public:
  void setup() override {
    // Ejecutado una sola vez al inicio
    Serial.println("MyCustomUsermod iniciado");
    pinMode(GPIO_NUM_34, INPUT);  // Ejemplo: configurar GPIO
  }

  void connected() override {
    // Ejecutado cuando se conecta a WiFi
    Serial.println("Conectado a WiFi");
  }

  void loop() override {
    // Ejecutado continuamente (~10-50 Hz)
    unsigned long now = millis();
    if (now - lastTime > 1000) {  // Cada 1000ms
      lastTime = now;
      myCounter++;
      // Tu lógica aquí
    }
  }

  void addToConfig(JsonObject& root) override {
    // Agregar configuración a JSON
    JsonObject obj = root.createNestedObject("myusermod");
    obj["enabled"] = true;
    obj["setting1"] = 42;
  }

  bool readFromConfig(JsonObject& root) override {
    // Leer configuración desde JSON
    JsonObject obj = root["myusermod"];
    if (!obj.isNull()) {
      if (obj["enabled"]) {
        Serial.println("Usermod habilitado");
      }
      return true;
    }
    return false;
  }

  uint16_t getId() override {
    return USERMOD_ID_CUSTOM;  // ID único
  }
};
```

#### Registrar Usermod

En `wled00/usermods_list.cpp`:

```cpp
#include "../usermods/my_usermod/usermod.h"

void registerUsermods() {
  registerUsermod(new MyCustomUsermod());
  // Agregar más usermods aquí
}
```

#### Compilar con Usermods

1. **Crear carpeta**: `wled00/usermods/my_usermod/`
2. **Crear archivo**: `usermod.h` con el código del usermod
3. **Crear `platformio_override.ini`**:

```ini
[env:custom_build]
extends = esp32dev
custom_usermods = my_usermod
build_flags =
  ${esp32dev.build_flags}
  -DWLED_ENABLE_CUSTOM_FEATURE
```

4. **Compilar**:
```bash
npm run build
pio run -e custom_build
```

### Deshabilitar Características para Ahorrar Espacio

En `wled00/wled.h`, descomentar para deshabilitar:

```cpp
// Protocolo E1.31
#define WLED_DISABLE_E131

// Servidor MQTT
#define WLED_DISABLE_MQTT

// WebSocket realtime
#define WLED_DISABLE_REALTIME

// Interfaz web (solo JSON API)
#define WLED_DISABLE_WEBUI

// Sync por UDP
#define WLED_DISABLE_UDPNOTIFIER

// Soporte de presets
#define WLED_DISABLE_PRESETS

// Alexa
#define WLED_DISABLE_ALEXA

// Paletas extendidas
#define WLED_DISABLE_PALETTE_EXTENTIONS
```

### Compilación para ESP8266 con Espacio Limitado

```bash
# Versión mínima para ESP8266 1MB
pio run -e esp01_1m_full

# Compilación optimizada
pio run -e esp8266_2m -O2
```

### Configuración de Memoria EEPROM

En `wled00/wled.h`:

```cpp
// Tamaño de EEPROM (ESP8266)
#define EEPROM_SIZE 4096

// Ubicación de config
#define EEPROM_START 0
```

---

## 🎨 Crear Efectos Personalizados

### Estructura Básica de un Efecto

```cpp
// En wled00/FX.cpp

uint16_t mode_mi_efecto_personalizado(void) {
  // Obtener información del segmento
  int len = SEGLEN;                    // Longitud del segmento
  uint32_t now = now = millis();       // Tiempo actual
  
  for (int i = 0; i < len; i++) {
    // Calcular color para cada LED
    uint8_t hue = (i + SEGMENT.speed) % 255;
    uint8_t sat = 255;
    uint8_t val = SEGMENT.intensity;
    
    // Convertir HSV a RGB
    uint32_t color = CHSV(hue, sat, val).rgb();
    
    // Aplicar color
    setPixelColor(i, color);
  }
  
  return FRAMETIME;  // Próximo frame en FRAMETIME ms
}
```

### Registrar Efecto

En `wled00/FX.h`:

```cpp
_addMode(mode_mi_efecto_personalizado, "Mi Efecto");
```

### Ejemplo: Efecto de Rebote

```cpp
uint16_t mode_bounce(void) {
  // Variables estáticas se mantienen entre llamadas
  static int position = 0;
  static int direction = 1;
  
  int len = SEGLEN;
  
  // Limpiar LEDs previos
  fill(BLACK);
  
  // Calcular nueva posición
  position += direction * (1 + (SEGMENT.speed >> 4));
  
  if (position <= 0 || position >= len - 1) {
    direction = -direction;  // Rebotar
  }
  
  position = constrain(position, 0, len - 1);
  
  // Dibujar punto
  setPixelColor(position, SEGMENT.colors[0]);
  
  return FRAMETIME;
}
```

### Ejemplo: Efecto de Onda

```cpp
uint16_t mode_wave_advanced(void) {
  uint32_t now = millis();
  int len = SEGLEN;
  
  for (int i = 0; i < len; i++) {
    // Crear onda sinusoidal
    float phase = (float)i / len * 2 * PI;
    float amp = sin(phase + now / 100.0);
    
    uint8_t brightness = (uint8_t)(128 + 127 * amp);
    uint32_t color = color32(
      SEGMENT.colors[0] & 0xFF0000,
      SEGMENT.colors[0] & 0x00FF00,
      brightness
    );
    
    setPixelColor(i, color);
  }
  
  return FRAMETIME;
}
```

---

## 🎨 Crear Paletas Personalizadas

En `wled00/palettes.cpp`:

```cpp
// Estructura: {posición(0-255), R, G, B}
DEFINE_GRADIENT_PALETTE(my_gradient_palette) {
    0,    255,   0,   0,    // Rojo en inicio
   85,    255, 255,   0,    // Amarillo en 1/3
  170,      0, 255,   0,    // Verde en 2/3
  255,      0,   0, 255     // Azul en final
};
```

Registrar en tabla de paletas:

```cpp
const TProgmemRGBGradientPalettePtr gGradientPalettes[] = {
  // Paletas existentes...
  my_gradient_palette,
  my_ocean_palette,
  // etc
};
```

---

## 📊 Optimización del Firmware

### Reducir Tamaño Binario

```ini
[env:esp32_minimal]
extends = esp32dev
build_flags =
  ${esp32dev.build_flags}
  -Os                              # Optimizar tamaño
  -DWLED_DISABLE_E131
  -DWLED_DISABLE_MQTT
  -DWLED_DISABLE_ALEXA
  -DWLED_DISABLE_BLYNK
```

### Aumentar Rendimiento

```ini
[env:esp32_performance]
extends = esp32dev
build_flags =
  ${esp32dev.build_flags}
  -O3                              # Optimizar velocidad
  -ffast-math                      # Operaciones rápidas
  -funroll-loops
```

### Usar PSRAM (ESP32-WROVER)

En `platformio_override.ini`:

```ini
[env:esp32_wrover]
extends = esp32dev
board = esp32-wrover-kit
board_build.f_cpu = 240000000L
build_flags =
  ${esp32dev.build_flags}
  -DBOARD_HAS_PSRAM
  -mfix-esp32-psram-cache-issue
```

---

## 🔌 Integración de Sensores

### DHT (Temperatura/Humedad)

En `wled00/usermods_list.cpp`:

```cpp
#include "../usermods/dht/usermod_dht.h"

void registerUsermods() {
  registerUsermod(new UsermodDHT());
}
```

Configurar pin en settings web.

### Sensor de Luz (BH1750)

Crear usermod:

```cpp
class BH1750Usermod : public Usermod {
private:
  uint8_t addr = 0x23;  // I2C address
  
public:
  void setup() override {
    // Inicializar I2C
    Wire.beginTransmission(addr);
    Wire.write(0x10);  // Resolución continua
    Wire.endTransmission();
  }
  
  void loop() override {
    // Leer valor de luz
    Wire.requestFrom(addr, 2);
    uint16_t lux = (Wire.read() << 8) | Wire.read();
    
    // Ajustar brillo automáticamente
    // bri = map(lux, 0, 50000, 10, 255);
  }
};
```

### Sensor PIR (Movimiento)

```cpp
class PIRUsermod : public Usermod {
private:
  int pirPin = 34;
  
public:
  void setup() override {
    pinMode(pirPin, INPUT);
  }
  
  void loop() override {
    if (digitalRead(pirPin) == HIGH) {
      // Movimiento detectado
      // Encender luces
    }
  }
};
```

---

## 🚀 Características Avanzadas

### Segmentos Virtuales

Crear múltiples segmentos lógicos:

```cpp
// En configuración, crear 4 segmentos
// Segmento 0: LEDs 0-30
// Segmento 1: LEDs 31-60
// Segmento 2: LEDs 61-90
// Segmento 3: LEDs 91-120
```

### Sincronización de Múltiples Dispositivos

```json
{
  "udpn": {
    "send": true,
    "recv": true,
    "nm": false
  }
}
```

Todos los dispositivos con `recv: true` sincronizarán con el que tiene `send: true`.

### WebSocket en Tiempo Real

Para obtener datos en vivo desde JavaScript:

```javascript
var ws;

function initWebSocket() {
  ws = new WebSocket('ws://' + ip);
  ws.binaryType = 'arraybuffer';
  
  ws.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
      // Datos en tiempo real (matriz LED actual)
      updateLEDs(new Uint8Array(event.data));
    }
  };
}
```

### Presets Automáticos

Crear secuencia automática:

```json
{
  "ps": -1,           // No cargar preset inmediatamente
  "pss": 10,          // Cambiar cada 10 segundos
  "psf": 2,           // Fade 200ms entre presets
  "tb": 1             // Tabla de reproducción habilitada
}
```

---

## 🐛 Debug y Troubleshooting

### Habilitar Debug Serial

En `wled00/wled.h`:

```cpp
#define DEBUG 1
```

Compilar y abrir monitor serial:

```bash
pio device monitor -b 115200
```

### Ver Mensajes de Log

```cpp
// En tu usermod
Serial.println("Mi mensaje de debug");
Serial.printf("Valor: %d\n", valor);
```

### Analizar Memoria

```cpp
Serial.printf("Heap libre: %d bytes\n", ESP.getFreeHeap());
Serial.printf("PSRAM libre: %d bytes\n", ESP.getFreePsram());
```

---

## 📦 Compilación para CI/CD

Crear workflow de GitHub Actions:

```yaml
name: Build WLED

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-node@v2
        with:
          node-version: '20'
      
      - name: Install dependencies
        run: |
          npm ci
          pip install -r requirements.txt
      
      - name: Build Web UI
        run: npm run build
      
      - name: Run tests
        run: npm test
      
      - name: Build firmware
        run: pio run -e esp32dev
```

---

## 🔐 Seguridad en Firmware

### Habilitar HTTPS

En `platformio_override.ini`:

```ini
[env:esp32_https]
extends = esp32dev
build_flags =
  ${esp32dev.build_flags}
  -DWLED_ENABLE_HTTPS
```

### Autenticación

En código:

```cpp
#define WLED_USE_AUTH
```

---

## 📋 Checklist de Compilación

Antes de liberar una compilación personalizada:

- [ ] Ejecutar `npm run build`
- [ ] Ejecutar `npm test`
- [ ] Compilar al menos 2 entornos diferentes
- [ ] Probar en dispositivo real
- [ ] Verificar consumo de memoria
- [ ] Revisar logs de compilación para advertencias
- [ ] Actualizar documentación de cambios
- [ ] Crear commit con mensaje claro

---

## 🔗 Recursos de Desarrollo

### Archivos Importantes

```
wled00/
├── wled.h                    # Configuración principal
├── FX.cpp/h                  # Motor de efectos
├── palettes.cpp/h            # Paletas de color
├── json.cpp                  # Manejo de JSON
├── webserver.cpp             # Servidor web
├── bus_manager.h             # Gestión de LEDs
├── pin_manager.h             # Gestión de GPIO
└── usermods_list.cpp         # Registro de usermods
```

### Librerías Utilizadas

- **Arduino Core**: Framework base
- **FastLED**: Librería de efectos de luz
- **ArduinoJson**: Procesamiento JSON
- **AsyncTCP**: Sockets asincronos
- **ESPAsyncWebServer**: Servidor web

---

Última actualización: Diciembre 2025
