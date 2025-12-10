#pragma once

/* Este usermod usa datos del giroscopio para proporcionar un efecto de "surge" basado en movimiento

Requiere lib_deps = bolderflight/Bolder Flight Systems Eigen@^3.0.0

*/

#include "wled.h"

// Eigen incluir block
#ifdef A0
namespace { constexpr size_t A0_temp {A0}; }
#undef A0
static constexpr size_t A0 {A0_temp};
#endif

#ifdef A1
namespace { constexpr size_t A1_temp {A1}; }
#undef A1
static constexpr size_t A1 {A1_temp};
#endif

#ifdef B0
namespace { constexpr size_t B0_temp {B0}; }
#undef B0
static constexpr size_t B0 {B0_temp};
#endif

#ifdef B1
namespace { constexpr size_t B1_temp {B1}; }
#undef B1
static constexpr size_t B1 {B1_temp};
#endif

#ifdef D0
namespace { constexpr size_t D0_temp {D0}; }
#undef D0
static constexpr size_t D0 {D0_temp};
#endif

#ifdef D1
namespace { constexpr size_t D1_temp {D1}; }
#undef D1
static constexpr size_t D1 {D1_temp};
#endif

#ifdef D2
namespace { constexpr size_t D2_temp {D2}; }
#undef D2
static constexpr size_t D2 {D2_temp};
#endif

#ifdef D3
namespace { constexpr size_t D3_temp {D3}; }
#undef D3
static constexpr size_t D3 {D3_temp};
#endif

#include "eigen.h"
#include <Eigen/Geometry>

constexpr auto ESTIMATED_G = 9.801;  // m/s^2
constexpr auto ESTIMATED_G_COUNTS = 8350.;
constexpr auto ESTIMATED_ANGULAR_RATE = (M_PI * 2000) / (INT16_MAX * 180); // radians per second

// Horribly lame digital filtro código
// Currently implements a estático IIR filtro.
template<typename T, unsigned C>
class xir_filter {
    typedef Eigen::Array<T, C, 1> array_t;
    const array_t a_coeff, b_coeff;
    const T gain;
    array_t x, y;

    public:
    xir_filter(T gain_, array_t a, array_t b) : a_coeff(std::move(a)), b_coeff(std::move(b)), gain(gain_), x(array_t::Zero()), y(array_t::Zero()) {};

    T operator()(T input) {
        x.head(C-1) = x.tail(C-1);  // shift by one
        x(C-1) = input / gain;
        y.head(C-1) = y.tail(C-1);  // shift by one
        y(C-1) = (x * b_coeff).sum();
        y(C-1) -= (y.head(C-1) * a_coeff.head(C-1)).sum();
        return y(C-1);
    }

    T last() { return y(C-1); };
};



class GyroSurge : public Usermod {
  private:
    static const char _name[];
    bool enabled = true;

    // Params
    uint8_t max = 0;
    float sensitivity = 0;

    // Estado
    uint32_t last_sample;
    // 100hz entrada
    // butterworth low pass filtro at 20hz
    xir_filter<float, 3> filter = { 1., { -0.36952738, 0.19581571, 1.}, {0.20657208, 0.41314417, 0.20657208} };
                                  // { 1., { 0., 0., 1.}, { 0., 0., 1. } }; // no filtro


  public:

    /*
     * `configuración()` se llama una vez al arrancar. En este punto WiFi aún no está conectado.
     */
    void setup() {};


    /*
     * `addToConfig()` puede usarse para añadir ajustes persistentes personalizados al fichero `cfg.JSON` en el objeto "um" (usermod).
     * Será llamada por WLED cuando los ajustes se guarden (por ejemplo, al guardar ajustes de LED).
     * Se recomienda revisar ArduinoJson para serialización/deserialización si se usan ajustes personalizados.
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));

      //guardar these vars persistently whenever settings are saved
      top["max"] = max;
      top["sensitivity"] = sensitivity;
    }


    /*
     * `readFromConfig()` puede usarse para leer los ajustes personalizados añadidos con `addToConfig()`.
     * Es llamada por WLED cuando se cargan los ajustes (actualmente al arrancar o tras guardar desde la página de Usermod Settings).
     *
     * `readFromConfig()` se llama ANTES de `configuración()`. Esto permite usar valores persistentes en `configuración()` (p. ej. asignación de pines),
     * pero si necesitas escribir valores persistentes en un búfer dinámico deberás asignarlo aquí en lugar de en `configuración()`.
     *
     * Devuelve `verdadero` si los valores de configuración estaban completos, o `falso` si quieres que WLED guarde los valores por defecto en disco.
     *
     * `getJsonValue()` devuelve falso si falta el valor, o copia el valor en la variable proporcionada y devuelve verdadero si está presente.
     * `configComplete` será verdadero sólo si el objeto del usermod y todos sus valores están presentes. Si faltan valores, WLED llamará a `addToConfig()` para guardarlos.
     *
     * Esta función se garantiza que se llame en el arranque, pero también puede ser llamada cada vez que se actualizan los ajustes.
     */
    bool readFromConfig(JsonObject& root)
    {
      // default settings values could be set here (or below usando the 3-argumento getJsonValue()) instead of in the clase definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single valor being missing after boot (e.g. if the cfg.JSON was manually edited and a valor was removed)

      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top["max"], max, 0);
      configComplete &= getJsonValue(top["sensitivity"], sensitivity, 10);

      return configComplete;
    }

    void loop() {
      // get IMU datos
      um_data_t *um_data;
      if (!UsermodManager::getUMData(&um_data, USERMOD_ID_IMU)) {
        // Apply max
        strip.getSegment(0).fadeToBlackBy(max);
        return;
      }
      uint32_t sample_count = *(uint32_t*)(um_data->u_data[8]);

      if (sample_count != last_sample) {        
        last_sample = sample_count;
        // Calculate based on new datos
        // We use the raw gyro datos (angular rate)        
        auto gyros = (int16_t*)um_data->u_data[4];  // 16384 == 2000 deg/s

        // Compute the overall rotation rate
        // For my aplicación (a plasma sword) we ignorar X axis rotations (eg. around the long axis)
        auto gyro_q = Eigen::AngleAxis<float> {
                        //Eigen::AngleAxis<flotante>(ESTIMATED_ANGULAR_RATE * gyros[0], Eigen::Vector3f::UnitX()) *
                        Eigen::AngleAxis<float>(ESTIMATED_ANGULAR_RATE * gyros[1], Eigen::Vector3f::UnitY()) *
                        Eigen::AngleAxis<float>(ESTIMATED_ANGULAR_RATE * gyros[2], Eigen::Vector3f::UnitZ()) };
        
        // Filtro the results
        filter(std::min(sensitivity * gyro_q.angle(), 1.0f));   // radians per second
/*
        Serie.printf("[%lu] Gy: %d, %d, %d -- ", millis(), (int)gyros[0], (int)gyros[1], (int)gyros[2]);
        Serie.imprimir(gyro_q.angle());
        Serie.imprimir(", ");
        Serie.imprimir(sensitivity * gyro_q.angle());
        Serie.imprimir(" --> ");
        Serie.println(filtro.last());
*/
      }
    }; // noop

    /*
     * `handleOverlayDraw()` se llama justo antes de cada `show()` (actualización del frame de la tira LED) después de que los efectos hayan definido los colores.
     * Úsalo para enmascarar LEDs o fijarles un color diferente independientemente del efecto activo.
     * Comúnmente usado para relojes personalizados (Cronixie, 7 segmentos)
     */
    void handleOverlayDraw()
    {

      // TODO: some kind of timing análisis for filtering ...

      // Calculate brillo boost
      auto r_float = std::max(std::min(filter.last(), 1.0f), 0.f);
      auto result = (uint8_t) (r_float * max);
      //Serie.printf("[%lu] %d -- ", millis(), resultado);
      //Serie.println(r_float);
      // TODO - multiple segmento handling??
      strip.getSegment(0).fadeToBlackBy(max - result);
    }
};

const char GyroSurge::_name[] PROGMEM = "GyroSurge";