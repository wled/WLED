# API REST de WLED - Referencia Completa

## 📡 Endpoints HTTP

### Estado del Dispositivo

#### GET `/json/state`
Obtiene el estado actual del dispositivo.

**Respuesta de ejemplo**:
```json
{
  "on": true,
  "bri": 255,
  "transition": 7,
  "ps": -1,
  "pl": false,
  "nl": {
    "on": false,
    "dur": 60,
    "fade": true,
    "mode": 0
  },
  "udpn": {
    "send": false,
    "recv": true,
    "nm": false
  },
  "lor": 0,
  "mainseg": 0,
  "seg": [
    {
      "id": 0,
      "start": 0,
      "stop": 120,
      "len": 120,
      "grp": 1,
      "spc": 0,
      "of": 0,
      "on": true,
      "bri": 255,
      "col": [
        [255, 0, 0],
        [0, 255, 0],
        [0, 0, 255]
      ],
      "fx": 5,
      "sx": 128,
      "ix": 128,
      "pal": 0,
      "sel": true,
      "rev": false,
      "cct": 127,
      "lc": false
  }
}
```

#### POST `/json/state`
Cambia el estado del dispositivo.

**Parámetros disponibles**:
```json
{
  "on": true,                    // Encender/apagar
  "bri": 255,                    // Brillo (0-255)
  "transition": 7,               // Transición en unidades de 100ms
  "seg": [
    {
      "id": 0,                   // ID del segmento
      "on": true,                // Segmento encendido
      "bri": 255,                // Brillo del segmento
      "col": [[255,0,0]],        // Colores RGB
      "fx": 5,                   // Efecto
      "sx": 128,                 // Velocidad efecto (0-255)
      "ix": 128,                 // Intensidad efecto (0-255)
      "pal": 0,                  // Paleta
      "rev": false               // Invertir dirección
    }
  ],
  "pl": false,                   // Pausar/reanudar
  "ps": -1,                      // Cargar preset (-1 = no cargar)
  "psave": 1                     // Guardar preset (número)
}
```

### Información del Dispositivo

#### GET `/json/info`
Obtiene información general del dispositivo.

**Respuesta de ejemplo**:
```json
{
  "name": "WLED",
  "udpport": 21324,
  "leds": {
    "count": 120,
    "rgbw": false,
    "wcount": 0,
    "max": 1200,
    "matrix": {
      "matrixrows": 0,
      "matrixcols": 0
    }
  },
  "arch": "esp32",
  "core": "4.3.0",
  "freeheap": 89040,
  "uptime": 3600,
  "opt": 34,
  "brand": "WLED",
  "product": "FOSS",
  "mac": "00:11:22:33:44:55",
  "ip": "192.168.1.100",
  "ws": 1,
  "ndc": 1,
  "live": false,
  "livedatalen": 25
}
```

### Efectos y Paletas

#### GET `/json/effects`
Lista todos los efectos disponibles.

**Respuesta de ejemplo**:
```json
[
  "Solid",
  "Blink",
  "Strobe",
  "Color Wipe",
  "Scan",
  "Scan Dual",
  "Fade",
  "Rainbow Cycle",
  "Rainbow Chase",
  "Rainbow Cycle Chase"
]
```

#### GET `/json/palettes`
Lista todas las paletas disponibles.

**Respuesta de ejemplo**:
```json
[
  "Default",
  "Analogous",
  "Analogous Warm",
  "Analogous Cool",
  "Rainbow",
  "Fire",
  "Cloud",
  "Ocean"
]
```

### Presets

#### GET `/json/presets`
Obtiene lista de presets guardados.

**Respuesta de ejemplo**:
```json
{
  "0": {
    "n": "Bedroom Red",
    "on": true,
    "bri": 100,
    "col": [[255,0,0]]
  },
  "1": {
    "n": "Party Mode",
    "on": true,
    "bri": 255,
    "fx": 5
  }
}
```

#### POST `/json/presets`
Guarda o carga un preset.

**Guardar preset 5**:
```json
{
  "psave": 5,
  "n": "Mi Preset"
}
```

**Cargar preset 5**:
```json
{
  "ps": 5
}
```

### Configuración

#### GET `/json/config`
Obtiene toda la configuración.

**Respuesta**: Objeto grande con todas las configuraciones guardadas.

#### POST `/settings`
Guarda configuración. Requiere multipart form data.

**Parámetros comunes**:
```
ap=SSID_de_wifi         // SSID WiFi
apw=contraseña          // Contraseña WiFi
sta=1                   // 1=conectar a WiFi, 0=AP mode
staticip=192.168.1.100  // IP estática
gw=192.168.1.1          // Gateway
sn=255.255.255.0        // Netmask
dns=8.8.8.8             // DNS
ltip=MQTT_IP            // IP del broker MQTT
mquser=usuario          // Usuario MQTT
mqpass=contraseña       // Contraseña MQTT
```

---

## 🎯 Ejemplos de Uso

### Ejemplo 1: Control Básico con curl

```bash
# Encender con color rojo
curl -X POST http://192.168.1.100/json/state \
  -H "Content-Type: application/json" \
  -d '{"on":true,"bri":200,"col":[[255,0,0]]}'

# Cambiar a efecto Rainbow
curl -X POST http://192.168.1.100/json/state \
  -H "Content-Type: application/json" \
  -d '{"fx":7,"sx":150}'

# Guardar como preset 1
curl -X POST http://192.168.1.100/json/state \
  -H "Content-Type: application/json" \
  -d '{"psave":1}'
```

### Ejemplo 2: Control desde Python

```python
import requests
import json

IP = "192.168.1.100"
BASE_URL = f"http://{IP}/json"

# Obtener estado actual
response = requests.get(f"{BASE_URL}/state")
current_state = response.json()
print(f"Brillo actual: {current_state['bri']}")

# Cambiar color
requests.post(f"{BASE_URL}/state", 
  json={"col": [[255, 100, 0]]})  # Naranja

# Cambiar efecto
requests.post(f"{BASE_URL}/state",
  json={"fx": 5, "sx": 200})  # Rainbow Cycle rápido

# Guardar preset
requests.post(f"{BASE_URL}/state",
  json={"psave": 3})
```

### Ejemplo 3: Control desde Node.js

```javascript
const fetch = require('node-fetch');

const IP = "192.168.1.100";
const apiUrl = `http://${IP}/json/state`;

// Función auxiliar para enviar comandos
async function sendCommand(command) {
  const response = await fetch(apiUrl, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(command)
  });
  return response.json();
}

// Ejemplos
(async () => {
  // Encender
  await sendCommand({ on: true });
  
  // Cambiar color a verde
  await sendCommand({ col: [[0, 255, 0]] });
  
  // Cambiar efecto
  await sendCommand({ fx: 17, sx: 128 });
})();
```

### Ejemplo 4: Home Assistant

En `configuration.yaml`:

```yaml
light:
  - platform: wled
    name: "Mi Sala"
    host: 192.168.1.100
    effects: true
```

Entonces en automatización:

```yaml
automation:
  - alias: Apagar luces a las 23:00
    trigger:
      platform: time
      at: "23:00:00"
    action:
      service: light.turn_off
      entity_id: light.mi_sala

  - alias: Encender a las 06:00
    trigger:
      platform: time
      at: "06:00:00"
    action:
      service: light.turn_on
      entity_id: light.mi_sala
      data:
        effect: "Rainbow Cycle"
        brightness: 200
```

---

## 🎨 Tabla de Colores RGB Comunes

| Color | RGB | Hex |
|-------|-----|-----|
| Rojo | [255, 0, 0] | #FF0000 |
| Verde | [0, 255, 0] | #00FF00 |
| Azul | [0, 0, 255] | #0000FF |
| Blanco | [255, 255, 255] | #FFFFFF |
| Negro | [0, 0, 0] | #000000 |
| Amarillo | [255, 255, 0] | #FFFF00 |
| Cian | [0, 255, 255] | #00FFFF |
| Magenta | [255, 0, 255] | #FF00FF |
| Naranja | [255, 165, 0] | #FFA500 |
| Rosa | [255, 192, 203] | #FFC0CB |
| Púrpura | [128, 0, 128] | #800080 |
| Marrón | [165, 42, 42] | #A52A2A |

---

## 📊 Códigos de Efectos Principales

| Código | Nombre | Descripción |
|--------|--------|-------------|
| 0 | Solid | Color sólido |
| 1 | Blink | Parpadeo simple |
| 2 | Strobe | Estrobo rápido |
| 3 | Color Wipe | Relleno de color |
| 4 | Scan | Barrido |
| 5 | Rainbow Cycle | Arcoíris rotatorio |
| 6 | Rainbow Chase | Persecución de arcoíris |
| 7 | Fade | Desvanecimiento |
| 10 | Twinkle | Centelleo |
| 15 | Fire | Simulación de fuego |
| 17 | Noise | Ruido dinámico |
| 20 | Waves | Ondas sinusoidales |
| 25 | Matrix | Efecto Matrix (código lluvia) |
| 35 | Ripple | Ondas desde centro |

Ver `/json/effects` para la lista completa.

---

## 🔐 Seguridad y Autenticación

### Habilitar OTA Password

```bash
curl -X POST http://192.168.1.100/json/state \
  -H "Content-Type: application/json" \
  -d '{"otapwd":"tuPassword123"}'
```

### Usar con API Key (si está habilitado)

```bash
curl "http://192.168.1.100/json/state?apikey=tu_api_key" \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"on":true}'
```

---

## 🐛 Respuestas de Error

### 200 OK
Comando ejecutado exitosamente.

### 400 Bad Request
Parámetro inválido en el JSON enviado.

**Ejemplo**:
```json
{"error":"Invalid JSON"}
```

### 401 Unauthorized
API key incorrecta o no proporcionada.

### 503 Service Unavailable
Dispositivo demasiado ocupado. Reintentar.

---

## ⏱️ Parámetros de Transición

El parámetro `transition` controla la duración de cambios de color:

```json
{
  "col": [[255, 0, 0]],
  "transition": 7  // 700ms (unidades de 100ms)
}
```

Ejemplos:
- `"transition": 0` → cambio instantáneo
- `"transition": 5` → 500ms
- `"transition": 10` → 1 segundo
- `"transition": 30` → 3 segundos

---

## 🔄 Polling de Estado

Para obtener actualizaciones en tiempo real:

```javascript
// Cada 500ms
setInterval(async () => {
  const response = await fetch('http://192.168.1.100/json/state');
  const state = await response.json();
  console.log(`Brillo: ${state.bri}, Efecto: ${state.seg[0].fx}`);
}, 500);
```

---

## 📝 Referencia Rápida de JSON

```json
{
  "on": true,                        // Boolean
  "bri": 255,                        // 0-255
  "transition": 7,                   // 0-255 (×100ms)
  "seg": [{
    "id": 0,                         // 0-9
    "start": 0,                      // LED inicial
    "stop": 120,                     // LED final
    "on": true,                      // Boolean
    "bri": 255,                      // 0-255
    "col": [                         // Array de colores
      [255, 0, 0],                   // Color 1 (RGB)
      [0, 255, 0],                   // Color 2
      [0, 0, 255]                    // Color 3
    ],
    "fx": 5,                         // 0-120+
    "sx": 128,                       // 0-255 (velocidad)
    "ix": 128,                       // 0-255 (intensidad)
    "pal": 0,                        // 0-50+ (paleta)
    "rev": false                     // Invertir
  }],
  "ps": -1,                          // Cargar preset (-1 = no)
  "psave": -1,                       // Guardar preset (-1 = no)
  "nl": {                            // Nightlight
    "on": false,
    "dur": 60,                       // Duración en minutos
    "mode": 0
  }
}
```

---

Última actualización: Diciembre 2025
