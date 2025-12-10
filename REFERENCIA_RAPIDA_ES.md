# 📋 Referencia Rápida - Comandos y Configuraciones

## ⌨️ Comandos Útiles

### Compilación

```bash
# Compilar Web UI (OBLIGATORIO primero)
npm run build

# Compilar firmware para ESP32
pio run -e esp32dev

# Compilar para ESP8266
pio run -e nodemcuv2

# Compilar y subir a dispositivo
pio run -e esp32dev --target upload

# Monitor serial
pio device monitor -b 115200

# Listar todos los entornos
pio run --list-targets

# Compilación con usermods
pio run -e custom_build
```

### Testing

```bash
# Ejecutar tests
npm test

# Watch mode para desarrollo
npm run dev
```

---

## 🎨 Comandos API (curl)

### Control Básico

```bash
# Encender
curl -X POST http://192.168.1.100/json/state -d '{"on":true}'

# Apagar
curl -X POST http://192.168.1.100/json/state -d '{"on":false}'

# Cambiar color a rojo
curl -X POST http://192.168.1.100/json/state -d '{"col":[[255,0,0]]}'

# Cambiar brillo
curl -X POST http://192.168.1.100/json/state -d '{"bri":128}'

# Cambiar efecto
curl -X POST http://192.168.1.100/json/state -d '{"fx":5,"sx":150}'
```

### Información

```bash
# Ver estado actual
curl http://192.168.1.100/json/state

# Ver info del dispositivo
curl http://192.168.1.100/json/info

# Ver efectos disponibles
curl http://192.168.1.100/json/effects

# Ver paletas
curl http://192.168.1.100/json/palettes
```

### Presets

```bash
# Guardar preset 1
curl -X POST http://192.168.1.100/json/state -d '{"psave":1}'

# Cargar preset 1
curl -X POST http://192.168.1.100/json/state -d '{"ps":1}'

# Ver presets disponibles
curl http://192.168.1.100/json/presets
```

---

## 🔧 Configuración Típica por Tipo de LED

### WS2812B (NeoPixel más común)

```
GPIO Pin: 5 (o cualquiera disponible)
Type: WS2812B / NeoPixel RGBW
Color Order: GRB (verde-rojo-azul)
Start LED: 0
Count: [tu cantidad]
Skip First: No
```

### APA102 (SPI)

```
Data GPIO: 13 (MOSI)
Clock GPIO: 14 (CLK)
Type: APA102 / Dotstar
Start LED: 0
Count: [tu cantidad]
```

### SK6812

```
GPIO Pin: 5
Type: SK6812 RGBW
Color Order: Probar RGB o GRB
Start LED: 0
Count: [tu cantidad]
```

---

## 🎛️ Pines GPIO Recomendados por Placa

### ESP32 DevKit

```
GPIO 5   (D5)  ← RECOMENDADO para LED 1
GPIO 16  (D16) ← LED 2
GPIO 17  (D17) ← LED 3
GPIO 4   (D4)  ← LED 4
GPIO 18  (D18) ← LED 5
GPIO 19  (D19) ← LED 6
GPIO 21  (D21) ← LED 7
GPIO 22  (D22) ← LED 8
GPIO 23  (D23) ← LED 9
GPIO 25  (D25) ← LED 10

EVITAR: GPIO 0, 6, 7, 8, 9, 10, 11
```

### ESP8266 (NodeMCU)

```
GPIO 5   (D1) ← RECOMENDADO
GPIO 4   (D2) ← Alternativa
GPIO 14  (D5) ← Si GPIO 5 no disponible
GPIO 12  (D6) ← Último recurso
GPIO 13  (D7) ← Último recurso

EVITAR: GPIO 0, 1, 3, 15
```

### ESP-01S

```
GPIO 0  ← Única opción recomendada
GPIO 2  ← Alternativa
EVITAR: GPIO 1, 3 (serial)
```

---

## 🌈 Códigos de Efectos (sin paleta)

```
0  = Solid
1  = Blink
2  = Strobe
3  = Color Wipe
4  = Scan
5  = Scan Dual
6  = Fade
7  = Rainbow Cycle ⭐ (popular)
8  = Rainbow Chase
9  = Rainbow Cycle Chase
10 = Twinkle
11 = Twinkle Fade
12 = Twinkle Fade Progressive
13 = Blink Rainbow
14 = Chase White
15 = Fire Flicker
16 = Fire
17 = Noise
18 = Noise with Fade
20 = Waves
```

Ver `/json/effects` en tu dispositivo para la lista completa (100+)

---

## 🎨 Paletas Populares

```
0  = Default (Rainbow)
1  = Analogous
2  = Analogous Warm
3  = Analogous Cool
5  = Rainbow (standard)
7  = Fire
8  = Cloud
9  = Ocean
10 = Forest
11 = Party
12 = Heat
13 = Pastel
14 = Sunset
```

Ver `/json/palettes` en tu dispositivo para la lista completa

---

## 🔐 Colores RGB Más Comunes

```javascript
Rojo:        [255,   0,   0]
Verde:       [  0, 255,   0]
Azul:        [  0,   0, 255]
Blanco:      [255, 255, 255]
Negro:       [  0,   0,   0]
Amarillo:    [255, 255,   0]
Cian:        [  0, 255, 255]
Magenta:     [255,   0, 255]
Naranja:     [255, 165,   0]
Rosa:        [255, 192, 203]
Púrpura:     [128,   0, 128]
Lima:        [  0, 255,   0] (verde brillante)
Teal:        [  0, 128, 128]
Blanco cálido:[255, 200, 100]
Blanco frío: [150, 200, 255]
```

---

## ⚙️ Configuración Mínima para Empezar

1. **GPIO**: Selecciona el pin (ej: GPIO 5)
2. **Tipo LED**: WS2812B o APA102
3. **Cantidad**: Número de LEDs
4. **Orden Color**: GRB o RGB (probar si no funciona)

¡Eso es todo para lo básico!

---

## 🚨 Troubleshooting Rápido

| Problema | Causa Probable | Solución |
|----------|---|---|
| No encienden LEDs | GPIO incorrecto | Cambiar GPIO en settings |
| Colores invertidos | Orden de color mal | Cambiar Color Order |
| WiFi no conecta | Contraseña incorrecta | Reiniciar, intentar de nuevo |
| Lento/lag | Muchos LEDs + efectos | Reducir cantidad o efectos |
| Se reinicia | Voltaje insuficiente | Mejorar alimentación |
| Interfaz lenta | WiFi débil | Acercarse al router |

---

## 📊 Parámetros JSON Esenciales

```json
{
  "on": true,              // Encender/apagar
  "bri": 255,              // Brillo 0-255
  "col": [[255,0,0]],      // Color RGB
  "fx": 7,                 // Efecto (0-120+)
  "sx": 128,               // Velocidad (0-255)
  "ix": 128,               // Intensidad (0-255)
  "pal": 0,                // Paleta (0-50+)
  "transition": 7,         // Transición (×100ms)
  "ps": -1,                // Cargar preset (-1=no)
  "psave": -1              // Guardar preset (-1=no)
}
```

---

## 🔌 Órdenes de Color LED

| Orden | LEDs Comunes | Formato |
|-------|---|---|
| GRB | WS2812B | Verde → Rojo → Azul |
| RGB | APA102 | Rojo → Verde → Azul |
| BRG | SK6812 | Azul → Rojo → Verde |
| RBG | Algunos | Rojo → Azul → Verde |

Si los colores se ven mal, prueba diferente orden.

---

## 📱 Control Rápido desde Celular

```
1. Conectar a mismo WiFi que WLED
2. Abrir navegador
3. Ir a: http://[IP-del-dispositivo]
4. ¡A disfrutar!

Ejemplo: http://192.168.1.100
```

---

## 🛠️ Archivos Importantes

```
Usar Web UI:     wled00/data/
Cambiar efectos: wled00/FX.cpp
Cambiar config:  wled00/wled.h
Compilación:     platformio.ini
```

**⚠️ NUNCA editar directamente**: `wled00/html_*.h`

---

## 📚 Dónde Encontrar Más Ayuda

- 📖 **Documentación completa**: `DOCUMENTACION_ES.md`
- ⚡ **Inicio rápido**: `GUIA_RAPIDA_ES.md`
- 🔌 **API REST**: `API_REFERENCIA_ES.md`
- 🛠️ **Compilación avanzada**: `COMPILACION_AVANZADA_ES.md`
- 📚 **Índice general**: `INDICE_DOCUMENTACION_ES.md`
- 💬 **Discord**: https://discord.gg/QAh7wJHrRM
- 🌐 **Wiki oficial**: https://kno.wled.ge

---

## ✅ Checklist de Configuración Básica

- [ ] Descargué e instalé WLED
- [ ] Conecté a mi WiFi
- [ ] Seleccioné el GPIO correcto
- [ ] Ingresé el número de LEDs
- [ ] Probé cambiar color
- [ ] Probé cambiar efecto
- [ ] Guardé un preset
- [ ] Leí la documentación completa

---

**Última actualización**: Diciembre 2025
**Versión WLED**: 2506160+
