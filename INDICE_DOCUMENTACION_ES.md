# 📚 WLED - Documentación Completa en Español

Bienvenido a la documentación de WLED en español. Este conjunto de documentos cubre todos los aspectos del proyecto, desde conceptos básicos hasta desarrollo avanzado.

## 📖 Documentos Disponibles

### 🚀 Inicio Rápido
- **[GUIA_RAPIDA_ES.md](GUIA_RAPIDA_ES.md)** - Configuración en 5 minutos
  - Descarga e instalación rápida
  - Control básico por API
  - Troubleshooting rápido
  - Control desde celular

### 📘 Documentación Completa
- **[DOCUMENTACION_ES.md](DOCUMENTACION_ES.md)** - Referencia exhaustiva (necesario leer)
  - Funcionamiento general de WLED
  - Guía completa de compilación
  - Configuración de hardware y red
  - Sistema de personalizaciones
  - Usermods y extensiones

### 🔌 API REST
- **[API_REFERENCIA_ES.md](API_REFERENCIA_ES.md)** - Control programático
  - Endpoints HTTP disponibles
  - Ejemplos en curl, Python, Node.js
  - Home Assistant integration
  - Seguridad y autenticación

### 🛠️ Compilación Avanzada
- **[COMPILACION_AVANZADA_ES.md](COMPILACION_AVANZADA_ES.md)** - Para desarrolladores
  - Compilación personalizada
  - Crear efectos y paletas
  - Integración de sensores
  - Optimización de firmware
  - Debugging

---

## 🎯 Guía de Lectura por Caso de Uso

### 👤 "Acabo de recibir un WLED"
1. Lee: **GUIA_RAPIDA_ES.md**
2. Sigue los 5 minutos de setup
3. Disfruta controlando tus LEDs

### 🏠 "Quiero integrar WLED en Home Assistant"
1. Lee: **DOCUMENTACION_ES.md** - Sección Configuración
2. Lee: **API_REFERENCIA_ES.md** - Sección Home Assistant
3. Configura la integración

### 💻 "Quiero compilar WLED personalizado"
1. Lee: **DOCUMENTACION_ES.md** - Sección Compilación
2. Lee: **COMPILACION_AVANZADA_ES.md** completo
3. Sigue los ejemplos prácticos

### 🔌 "Quiero agregar un sensor DHT/PIR/BH1750"
1. Lee: **DOCUMENTACION_ES.md** - Sección Personalización
2. Lee: **COMPILACION_AVANZADA_ES.md** - Sección Integración de Sensores
3. Descarga usermod y compila

### 🎨 "Quiero crear mis propios efectos"
1. Lee: **DOCUMENTACION_ES.md** - Sección de Efectos
2. Lee: **COMPILACION_AVANZADA_ES.md** - Crear Efectos
3. Estudia ejemplos en `wled00/FX.cpp`

### 📱 "Quiero controlar WLED desde mi app"
1. Lee: **API_REFERENCIA_ES.md** completo
2. Elige tu lenguaje (Python, JavaScript, etc)
3. Sigue los ejemplos de código

---

## 🔍 Búsqueda Rápida por Tema

### Instalación y Setup
- [Descargar e instalar](GUIA_RAPIDA_ES.md#-configuración-en-5-minutos)
- [Conectar a WiFi](DOCUMENTACION_ES.md#configuración-de-red-y-wifi)
- [Conectar LEDs](DOCUMENTACION_ES.md#configuración-de-hardware)

### Uso Diario
- [Cambiar color](GUIA_RAPIDA_ES.md#cambiar-color)
- [Cambiar efecto](GUIA_RAPIDA_ES.md#cambiar-efecto)
- [Crear presets](DOCUMENTACION_ES.md#presets-avanzados)
- [Automatizaciones](API_REFERENCIA_ES.md#ejemplo-4-home-assistant)

### Configuración
- [Pines GPIO](DOCUMENTACION_ES.md#pines-gpio)
- [Segmentos LED](DOCUMENTACION_ES.md#configuración-de-segmentos)
- [MQTT](DOCUMENTACION_ES.md#mqtt)
- [Alexa](DOCUMENTACION_ES.md#alexa)
- [E1.31/Art-Net](DOCUMENTACION_ES.md#sincronización-de-red)

### Compilación
- [Requisitos previos](DOCUMENTACION_ES.md#requisitos-previos)
- [Proceso básico](DOCUMENTACION_ES.md#proceso-completo-de-compilación)
- [Con usermods](COMPILACION_AVANZADA_ES.md#compilación-con-usermods)
- [Optimización](COMPILACION_AVANZADA_ES.md#optimización-del-firmware)

### Desarrollo
- [Crear usermods](DOCUMENTACION_ES.md#crear-usermod-personalizado)
- [Crear efectos](COMPILACION_AVANZADA_ES.md#crear-efectos-personalizados)
- [Crear paletas](COMPILACION_AVANZADA_ES.md#crear-paletas-personalizadas)
- [Debug](COMPILACION_AVANZADA_ES.md#debug-y-troubleshooting)

### API
- [GET /json/state](API_REFERENCIA_ES.md#get-jsonstate)
- [POST /json/state](API_REFERENCIA_ES.md#post-jsonstate)
- [Cambiar color](API_REFERENCIA_ES.md#cambiar-color)
- [Cambiar efecto](API_REFERENCIA_ES.md#cambiar-efecto)
- [Ejemplos Python](API_REFERENCIA_ES.md#ejemplo-2-control-desde-python)

### Solución de Problemas
- [Conexión WiFi](GUIA_RAPIDA_ES.md#troubleshooting-básico)
- [LEDs no encienden](GUIA_RAPIDA_ES.md#no-veo-los-leds-encenderse)
- [Colores incorrectos](GUIA_RAPIDA_ES.md#los-colores-se-ven-incorrectos)
- [Reinicios constantes](GUIA_RAPIDA_ES.md#el-dispositivo-se-reinicia-constantemente)
- [Compilación falla](DOCUMENTACION_ES.md#solución-de-problemas-de-compilación)

---

## 📊 Mapa de Contenidos

```
WLED Documentación
│
├─ Guía Rápida (5 min)
│  ├─ Setup inicial
│  ├─ Control básico
│  └─ Troubleshooting
│
├─ Documentación Completa
│  ├─ Funcionamiento
│  │  ├─ Características
│  │  ├─ Interfaz de usuario
│  │  └─ Arquitectura interna
│  │
│  ├─ Compilación
│  │  ├─ Fase 1: Web UI
│  │  ├─ Fase 2: Firmware
│  │  └─ Desarrollo
│  │
│  ├─ Configuración
│  │  ├─ Hardware (GPIO, LEDs)
│  │  ├─ Red (WiFi, MQTT)
│  │  ├─ Seguridad
│  │  └─ Servicios (Alexa, E1.31)
│  │
│  └─ Personalización
│     ├─ Efectos (100+)
│     ├─ Paletas de color
│     ├─ Usermods
│     └─ Interfaz web
│
├─ API REST
│  ├─ Endpoints
│  ├─ Ejemplos código
│  ├─ Colores RGB
│  └─ Códigos de efectos
│
└─ Compilación Avanzada
   ├─ Usermods V2
   ├─ Crear efectos
   ├─ Crear paletas
   ├─ Sensores
   ├─ Optimización
   └─ Debug
```

---

## 🚀 Flujo Típico de Uso

```
1. Usuario recibe WLED
        ↓
2. Lee GUIA_RAPIDA_ES.md
        ↓
3. Setup en 5 minutos
        ↓
4. Control básico desde web
        ↓
5. Explora efectos y presets
        ↓
6. [Según necesidad]
   ├─ Integración Home Assistant → API_REFERENCIA_ES.md
   ├─ Personalización → DOCUMENTACION_ES.md
   ├─ Desarrollo avanzado → COMPILACION_AVANZADA_ES.md
   └─ Control programático → API_REFERENCIA_ES.md
```

---

## 💡 Puntos Clave a Recordar

### ✅ Lo que SÍ debes hacer
- **Siempre** compilar Web UI primero (`npm run build`)
- **Usar** el selector de pin correcto para tu placa
- **Verificar** el orden de color de tus LEDs (GRB es común)
- **Leer** la documentación antes de hacer cambios
- **Hacer pruebas** en dispositivo real
- **Hacer backup** de tus configuraciones

### ❌ Lo que NO debes hacer
- **No** editar directamente archivos `html_*.h`
- **No** cambiar `platformio.ini` (usar `platformio_override.ini`)
- **No** cancelar compilaciones largas
- **No** esperar que todos los usermods funcionen simultáneamente
- **No** usar los mismos GPIO para múltiples funciones
- **No** olvidar guardar la configuración después de cambios

---

## 🤝 Comunidad

- **Discord**: https://discord.gg/QAh7wJHrRM
- **Foro**: https://wled.discourse.group
- **Wiki oficial**: https://kno.wled.ge
- **GitHub**: https://github.com/wled-dev/WLED

---

## 📝 Información del Documento

- **Versión**: 1.0 (Diciembre 2025)
- **Idioma**: Español
- **Compatibilidad**: WLED v2506160+
- **Entorno**: ESP32, ESP8266, ESP32-S3, ESP32-C3

---

## 🔗 Índice de Todos los Documentos

1. **README.md** (original en inglés) - Información general del proyecto
2. **DOCUMENTACION_ES.md** - Documentación completa en español
3. **GUIA_RAPIDA_ES.md** - Guía de inicio rápido
4. **API_REFERENCIA_ES.md** - Referencia de API REST
5. **COMPILACION_AVANZADA_ES.md** - Guía de compilación avanzada
6. **INDICE_DOCUMENTACION_ES.md** - Este archivo

---

## ❓ Preguntas Frecuentes

**P: ¿Por dónde empiezo?**
R: Comienza con GUIA_RAPIDA_ES.md, luego DOCUMENTACION_ES.md

**P: ¿Cómo compilo WLED?**
R: Lee la sección Compilación en DOCUMENTACION_ES.md

**P: ¿Cómo controlo WLED desde mi app?**
R: Lee API_REFERENCIA_ES.md

**P: ¿Cómo agregó un sensor?**
R: Lee COMPILACION_AVANZADA_ES.md sección Integración de Sensores

**P: ¿Qué placa necesito?**
R: Lee DOCUMENTACION_ES.md sección Hardware Compatible

---

**¡Felicidades! Ya tienes todo lo que necesitas para dominar WLED. 🎉**

Para ayuda adicional, visita la comunidad oficial en Discord o el foro.
