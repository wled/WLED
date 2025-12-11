# Guía: Actualizar Componentes de WLED

Esta guía te enseña cómo mantener WLED actualizado, incluyendo el firmware, dependencias, librerías y herramientas de compilación.

## Tabla de Contenidos

1. [Tipos de Actualizaciones](#tipos-de-actualizaciones)
2. [Actualizar WLED (Firmware)](#actualizar-wled-firmware)
3. [Actualizar Dependencias de Node.js](#actualizar-dependencias-de-nodejs)
4. [Actualizar Dependencias de Python](#actualizar-dependencias-de-python)
5. [Actualizar PlatformIO](#actualizar-platformio)
6. [Actualizar Arduino Core](#actualizar-arduino-core)
7. [Solución de Problemas](#solución-de-problemas)

---

## Tipos de Actualizaciones

### 🔄 Actualizaciones Disponibles

| Componente | Propósito | Frecuencia |
|-----------|----------|-----------|
| **WLED Firmware** | Nuevo código, efectos, características | Cada 1-2 meses |
| **Node.js Dependencies** | Dependencias de compilación Web UI | Según sea necesario |
| **Python Requirements** | Herramientas PlatformIO y scripts | Según sea necesario |
| **PlatformIO** | Sistema de compilación | Cada 2-4 semanas |
| **Arduino Core (ESP8266/ESP32)** | Núcleo del microcontrolador | Cada 1-2 meses |
| **Librerías C++** | Librerías de funcionamiento (NeoPixel, MQTT, etc) | Automático en compilación |

---

## Actualizar WLED (Firmware)

### Opción 1: Descarga OTA (Over-The-Air) - Recomendado

**Paso 1: Acceder a la interfaz web**

1. Abre tu navegador
2. Ve a `http://wled.local` o `http://[IP_DEL_ESP8266]`
3. Inicia sesión si tienes contraseña configurada

**Paso 2: Buscar actualizaciones**

1. Ve a **Configuración** (⚙️ icono)
2. Selecciona **Sistema**
3. Busca la sección **Actualización automática** o **Software Update**
4. Haz clic en **Buscar actualizaciones** o **Check for updates**

**Paso 3: Descargar e instalar**

1. Si hay una actualización disponible, verás el número de versión
2. Haz clic en **Actualizar** o **Update**
3. Espera a que se complete (puede tomar 1-2 minutos)
4. El dispositivo se reiniciará automáticamente

**Ventajas:**
- ✅ Simple y rápido
- ✅ No requiere cables USB
- ✅ No necesita compilación
- ✅ Se mantienen todas las configuraciones

**Desventajas:**
- ❌ Solo disponible si el WLED actual funciona
- ❌ Binarios pre-compilados (no personalización)

### Opción 2: Compilar e Instalar desde Código Fuente

Si necesitas personalizar WLED o la OTA no funciona:

**Paso 1: Actualizar el código fuente**

```bash
cd ~/WLED
git pull origin main
```

**Salida esperada:**
```
remote: Enumerating objects: 50, done.
remote: Counting objects: 100% (50/50), done.
Unpacking objects: 100% (25/25), done.
From https://github.com/Aircoookie/WLED
   abc1234..def5678  main       -> origin/main
Updating abc1234..def5678
Fast-forward
 wled00/FX.cpp   | 100 ++
 wled00/wled.h   |  10 +-
 ...
```

**Paso 2: Verificar cambios**

```bash
git log --oneline -5
# Muestra los últimos 5 commits
```

**Paso 3: Compilar Web UI**

```bash
npm run build
```

**Paso 4: Compilar firmware**

```bash
pio run -e nodemcuv2
# Reemplaza 'nodemcuv2' con tu placa
```

**Paso 5: Flashear**

```bash
pio run -e nodemcuv2 --target upload
```

**Ventajas:**
- ✅ Control total sobre características
- ✅ Puedes personalizar
- ✅ Acceso a versiones de desarrollo
- ✅ Mejoras y fixes más recientes

**Desventajas:**
- ❌ Más tiempo (compilación toma 10-15 minutos)
- ❌ Requiere cables y configuración
- ❌ Puede perder algunas configuraciones (depende)

---

## Actualizar Dependencias de Node.js

Las dependencias de Node.js se usan para compilar la interfaz web.

### Verificar versiones instaladas

```bash
npm list
# Muestra todas las dependencias y sus versiones
```

**Salida esperada:**
```
wled@2506160 /workspaces/WLED
├── crc@3.8.0
├── html-minifier@4.0.0
├── terser@5.14.0
└── ...
```

### Actualizar a las últimas versiones

**Opción A: Actualizar dependencias menores (recomendado)**

```bash
npm update
```

Esto actualiza a parches y versiones menores, mantiene compatibilidad.

**Opción B: Actualizar a versiones mayores (cuidado)**

```bash
npm upgrade
# o
npm install -g npm-check-updates
ncu -u
npm install
```

⚠️ **Advertencia**: Actualizar versiones mayores puede romper compatibilidad.

### Limpiar caché y reinstalar

Si hay problemas después de actualizar:

```bash
rm -rf node_modules package-lock.json
npm install
```

---

## Actualizar Dependencias de Python

Las dependencias de Python incluyen PlatformIO y scripts de compilación.

### Verificar versiones instaladas

```bash
pip list
# Muestra todos los paquetes instalados
```

**Salida esperada:**
```
Package            Version
------------------ ---------
platformio         6.1.7
PyYAML            6.0
requests          2.28.1
...
```

### Actualizar todas las dependencias

```bash
pip install -r requirements.txt --upgrade
```

### Actualizar paquetes específicos

```bash
# Actualizar solo PlatformIO
pip install platformio --upgrade

# Actualizar solo esptool (para flashear)
pip install esptool --upgrade
```

### Verificar qué está desactualizado

```bash
pip list --outdated
# Muestra paquetes con versiones más nuevas disponibles
```

---

## Actualizar PlatformIO

PlatformIO es el sistema de compilación para ESP8266/ESP32.

### Método 1: Actualizar vía pip

```bash
pip install platformio --upgrade
```

### Método 2: Actualizar vía terminal en VS Code

```bash
pio upgrade
```

### Verificar versión instalada

```bash
pio --version
# Muestra: PlatformIO Core 6.1.7
```

### Actualizar platform packages (Arduino Core)

PlatformIO descarga automáticamente el Arduino Core necesario, pero puedes actualizar manualmente:

```bash
# Actualizar ESP8266 platform
pio platform update espressif8266

# Actualizar ESP32 platform
pio platform update espressif32

# Actualizar todas las plataformas
pio platform update
```

---

## Actualizar Arduino Core

El Arduino Core es el código base que permite compilar para ESP8266/ESP32.

### Verificar versiones instaladas

```bash
pio platform list
```

**Salida esperada:**
```
 Platform        ID         Version  Name
 ============== ========== =========== ==================
 Espressif 8266  espressif8266  2.7.4.7    Espressif 8266
 Espressif 32   espressif32  6.1.0      Espressif 32
```

### Actualizar versiones específicas

```bash
# Actualizar a última versión disponible
pio platform update espressif8266

# Actualizar a versión específica (ejemplo)
pio platform install espressif8266@2.7.4.7

# Actualizar Arduino core para ESP32
pio platform update espressif32
```

### Limpiar y reinstalar

Si hay problemas de compilación después de actualizar:

```bash
# Eliminar caché de PlatformIO
pio run --target clean

# Eliminar plataforma completamente
pio platform uninstall espressif8266

# Reinstalar
pio platform install espressif8266
```

---

## Solución de Problemas

### Problema: "Error: El compilador no se encuentra"

**Causa**: Arduino Core desactualizado o dañado

**Solución:**
```bash
# Limpiar todo
pio platform uninstall espressif8266
rm -rf ~/.platformio

# Reinstalar
pio platform install espressif8266
pio run -e nodemcuv2
```

### Problema: "Error: Librerías no encontradas"

**Causa**: Dependencias de Python desactualizadas

**Solución:**
```bash
pip install -r requirements.txt --upgrade --force-reinstall
npm install
npm run build
pio run -e nodemcuv2
```

### Problema: "Error: incompatibilidad de versión"

**Causa**: Actualización de versión mayor sin compatibilidad

**Solución:**
```bash
# Revertir a versión estable
git checkout vX.X.X  # Reemplazar con versión anterior

# O restaurar últimos cambios buenos
git log --oneline -10
git checkout <hash_commit_bueno>

# Recompilar
npm run build
pio run -e nodemcuv2
```

### Problema: "Timeout durante compilación"

**Causa**: Descarga de componentes lentos o problemas de red

**Solución:**
```bash
# Esperar e intentar de nuevo (PlatformIO descarga en paralelo)
pio run -e nodemcuv2 --verbose

# Si persiste, limpiar y reintentar
pio run --target clean
rm -rf .pio
pio run -e nodemcuv2
```

### Problema: "Error después de actualizar OTA"

**Causa**: Firmware corrupto o incompatibilidad

**Solución:**
1. **Reset de fábrica**:
   - Ve a **Configuración** → **Sistema** → **Reset**
   - Selecciona "Borrar EEPROM también"
   
2. **Flasheo manual desde cero**:
   ```bash
   # Borrar memoria completamente
   esptool.py --port COM3 erase_flash
   
   # Flashear última versión estable compilada
   pio run -e nodemcuv2 --target upload
   ```

---

## Checklist de Actualización

```
☐ 1. Hacer backup de configuraciones (si es importante)
     - Ve a Configuración → Descargar Configuración
     
☐ 2. Actualizar código fuente
     - git pull origin main
     
☐ 3. Actualizar dependencias
     - npm install / npm update
     - pip install -r requirements.txt --upgrade
     
☐ 4. Limpiar builds previos
     - pio run --target clean
     
☐ 5. Compilar Web UI
     - npm run build
     
☐ 6. Compilar firmware
     - pio run -e [tu_placa]
     
☐ 7. Flashear
     - pio run -e [tu_placa] --target upload
     
☐ 8. Probar funcionamiento
     - Verificar que se conecta a WiFi
     - Probar controles básicos (color, efectos)
     - Revisar consola serial por errores
     
☐ 9. Restaurar configuración
     - Si es necesario, restaurar backup
```

---

## Comandos Rápidos de Referencia

```bash
# Verificar qué está desactualizado
npm outdated
pip list --outdated

# Actualizar todo (Node.js)
npm update

# Actualizar todo (Python)
pip install -r requirements.txt --upgrade

# Actualizar PlatformIO
pip install platformio --upgrade

# Actualizar Arduino Cores
pio platform update

# Limpiar e reinstalar dependencias
rm -rf node_modules package-lock.json && npm install
pip install -r requirements.txt --force-reinstall

# Compilación completa desde cero
npm run build && pio run --target clean && pio run -e nodemcuv2

# Ver último commit en repositorio remoto
git fetch origin
git log --oneline origin/main -5
```

---

## Recursos Adicionales

- **WLED GitHub Releases**: [github.com/Aircoookie/WLED/releases](https://github.com/Aircoookie/WLED/releases)
- **PlatformIO Documentation**: [docs.platformio.org](https://docs.platformio.org)
- **Node.js Documentation**: [nodejs.org/docs](https://nodejs.org/docs)
- **Python pip**: [pip.pypa.io](https://pip.pypa.io)

---

**Última actualización**: Diciembre 2025

**Consejo Final**: Actualiza regularmente (cada 1-2 meses) para obtener mejoras, nuevos efectos y fixes de seguridad. Siempre haz backup de configuraciones importantes antes de grandes cambios.
