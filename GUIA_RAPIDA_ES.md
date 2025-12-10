# Guía Rápida: Primeros Pasos con WLED

## ⚡ Configuración en 5 Minutos

### 1. Descargar y Flashear (2 min)

**Opción A: Herramienta Web (Recomendada)**
1. Ir a https://install.wled.me
2. Seleccionar tu placa (ESP32 o ESP8266)
3. Conectar dispositivo por USB
4. Hacer clic en "Install"
5. Seguir las instrucciones

**Opción B: Desde CLI**
```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 \
  write_flash -z 0x0 \
  WLED_0.13.0_ESP32.bin
```

### 2. Conectar a WiFi (1 min)

1. Buscar red: `WLED-XXXXXX` en tus WiFi disponibles
2. Conectar (sin contraseña)
3. Abrir navegador: http://192.168.4.1
4. Ir a Gear (⚙️) → WiFi Setup
5. Seleccionar tu red y contraseña
6. Guardar y reiniciar

### 3. Conectar LEDs (1 min)

1. Ir a Settings → LED Preferences → Pin configuration
2. Seleccionar GPIO pin (ej: GPIO 5)
3. Seleccionar tipo (NeoPixel WS2812)
4. Ingresar cantidad de LEDs
5. Guardar

### 4. ¡Disfrutar! (1 min)

- Selector de color para cambiar color
- Dropdown de efectos para animaciones
- Deslizador de velocidad para ajustar rapidez

---

## 🎨 Control Rápido por API

### Cambiar Color

```bash
# Rojo
curl "http://192.168.1.100/json/state" -X POST -d '{"col":[[255,0,0]]}'

# Verde
curl "http://192.168.1.100/json/state" -X POST -d '{"col":[[0,255,0]]}'

# Azul
curl "http://192.168.1.100/json/state" -X POST -d '{"col":[[0,0,255]]}'

# Blanco
curl "http://192.168.1.100/json/state" -X POST -d '{"col":[[255,255,255]]}'
```

### Cambiar Efecto

```bash
# Rainbow (efecto 1)
curl "http://192.168.1.100/json/state" -X POST -d '{"fx":1}'

# Blink (efecto 2)
curl "http://192.168.1.100/json/state" -X POST -d '{"fx":2}'

# Fire (efecto 17)
curl "http://192.168.1.100/json/state" -X POST -d '{"fx":17}'
```

### Encender/Apagar

```bash
# Encender
curl "http://192.168.1.100/json/state" -X POST -d '{"on":true}'

# Apagar
curl "http://192.168.1.100/json/state" -X POST -d '{"on":false}'
```

### Cambiar Brillo

```bash
# 50% de brillo
curl "http://192.168.1.100/json/state" -X POST -d '{"bri":128}'

# 100% de brillo
curl "http://192.168.1.100/json/state" -X POST -d '{"bri":255}'

# Brillo muy bajo
curl "http://192.168.1.100/json/state" -X POST -d '{"bri":10}'
```

---

## 🔧 Troubleshooting Básico

### "No veo la red WiFi WLED"

1. Esperar 30 segundos después de enchufar
2. Buscar de nuevo en redes disponibles
3. Si aún no aparece: reiniciar dispositivo (apagar/encender)

### "No se conecta a mi WiFi"

1. Verificar contraseña (mayúsculas/minúsculas importan)
2. Asegurar que el router transmite SSID (no está oculto)
3. Estar cerca del router
4. Reintentar después de unos segundos

### "No veo los LEDs encenderse"

1. **Verificar conexión**: ¿Está el cable de datos conectado?
2. **Verificar alimentación**: ¿Tienen los LEDs poder suficiente?
3. **Verificar configuración**: Settings → LED Preferences → está correcta?
4. **Probar con color blanco**: Algunos efectos pueden no verse

### "Los colores se ven incorrectos"

1. Ir a Settings → LED Preferences → Color Order
2. Probar diferentes órdenes (RGB, GRB, BRG)
3. GRB es la más común para WS2812B

### "El dispositivo se reinicia constantemente"

1. Verificar que la alimentación es suficiente
2. Desconectar sensores/usermods si están agregados
3. Probar con menos LEDs (reducir cantidad en configuración)

---

## 📱 Control por Celular

### Con App Oficial

1. Descargar "WLED Native" de Play Store o App Store
2. App descubre automáticamente el dispositivo
3. Mismos controles que web UI

### Con Navegador Móvil

1. En teléfono, conectar a mismo WiFi que WLED
2. Abrir navegador
3. Ingresar http://[IP-del-dispositivo]
4. ¡A controlar!

---

## 🎛️ Configuración Común

### Zona de Dormitorio

1. **Efecto**: Solid (color sólido)
2. **Color**: Blanco cálido (#FFF5E1)
3. **Brillo**: 30%
4. **Velocidad**: N/A

### Zona de Fiesta

1. **Efecto**: Rainbow Cycle
2. **Paleta**: Party
3. **Velocidad**: 150
4. **Intensidad**: 200

### Zona de Cine

1. **Efecto**: Solid
2. **Color**: Rojo oscuro (#220000)
3. **Brillo**: 20%

---

## ⚙️ Parámetros Clave

| Parámetro | Rango | Descripción |
|-----------|-------|-------------|
| `on` | true/false | Encender/apagar |
| `bri` | 0-255 | Brillo global |
| `col` | [R,G,B] | Color RGB |
| `fx` | 0-120+ | Índice de efecto |
| `sx` | 0-255 | Velocidad del efecto |
| `ix` | 0-255 | Intensidad del efecto |
| `pal` | 0-50+ | Índice de paleta |
| `seg` | 0-9 | Segmento a controlar |

---

## 🚀 Próximos Pasos

1. **Leer documentación completa**: `DOCUMENTACION_ES.md`
2. **Explorar efectos**: Probar todos los 100+ efectos disponibles
3. **Crear presets**: Guardar tus configuraciones favoritas
4. **Agregar sensores**: DHT, PIR, BH1750, etc.
5. **Automatizar**: Integrar con Home Assistant, Alexa, etc.

---

Última actualización: Diciembre 2025
