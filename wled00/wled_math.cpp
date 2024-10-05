/*
 * Contains some trigonometric functions.
 * The ANSI C equivalents are likely faster, but using any sin/cos/tan function incurs a memory penalty of 460 bytes on ESP8266, likely for lookup tables.
 * This implementation has no extra static memory usage.
 *
 * Source of the cos_t() function: https://web.eecs.utk.edu/~azh/blog/cosine.html (cos_taylor_literal_6terms)
 */

#include <Arduino.h> //PI constant

//#define WLED_DEBUG_MATH
#ifdef WLED_DEBUG_MATH
  // enable additional debug output
  #if defined(WLED_DEBUG_HOST)
    #include "net_debug.h"
    #define DEBUGOUT NetDebug
  #else
    #define DEBUGOUT Serial
  #endif
  #define DEBUGM_PRINT(x) DEBUGOUT.print(x)
  #define DEBUGM_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUGM_PRINTF(x...) DEBUGOUT.printf(x)
  #define DEBUGM_PRINTF_P(x...) DEBUGOUT.printf_P(x)
#else
  #define DEBUGM_PRINT(x)
  #define DEBUGM_PRINTLN(x)
  #define DEBUGM_PRINTF(x...)
  #define DEBUGM_PRINTF_P(x...)
#endif


#define modd(x, y) ((x) - (int)((x) / (y)) * (y))

// Note: cos_t, sin_t and tan_t are very accurate but may be slow
// the math.h functions use several kB of flash and are to be avoided if possible
// sin16_t / cos16_t are faster and much more accurate than the fastled variants
// sin_approx and cos_approx are float wrappers for sin16_t/cos16_t and have an accuracy of +/-0.0015 compared to sinf()
// sin8_t / cos8_t are fastled replacements and use sin16_t / cos16_t. Slightly slower than fastled version but very accurate

float cos_t(float phi) {
  float x = modd(phi, TWO_PI);
  if (x < 0) x = -1 * x;
  int8_t sign = 1;
  if (x > PI)
  {
      x -= PI;
      sign = -1;
  }
  float xx = x * x;

  float res = sign * (1 - ((xx) / (2)) + ((xx * xx) / (24)) - ((xx * xx * xx) / (720)) + ((xx * xx * xx * xx) / (40320)) - ((xx * xx * xx * xx * xx) / (3628800)) + ((xx * xx * xx * xx * xx * xx) / (479001600)));
  DEBUGM_PRINTF_P(PSTR("cos: %f,%f,%f,(%f)\n"),phi,res,cos(x),res-cos(x));
  return res;
}

float sin_t(float phi) {
  float res =  cos_t(HALF_PI - phi);
  DEBUGM_PRINTF_P(PSTR("sin: %f,%f,%f,(%f)\n"),x,res,sin(x),res-sin(x));
  return res;
}

float tan_t(float x) {
  float c = cos_t(x);
  if (c==0.0f) return 0;
  float res = sin_t(x) / c;
  DEBUGM_PRINTF_P(PSTR("tan: %f,%f,%f,(%f)\n"),x,res,tan(x),res-tan(x));
  return res;
}

// 16-bit, integer based Bhaskara I's sine approximation: 16*x*(pi - x) / (5*pi^2 - 4*x*(pi - x))
// input is 16bit unsigned (0-65535), output is 16bit signed (-32767 to +32767)
// optimized integer implementation by @dedehai
int16_t sin16_t(uint16_t theta) {
  int scale = 1;
  if (theta > 0x7FFF) {
    theta = 0xFFFF - theta;
    scale = -1; // second half of the sine function is negative (pi - 2*pi)
  }
  uint32_t precal = theta * (0x7FFF - theta);
  uint64_t numerator = (uint64_t)precal * (4 * 0x7FFF); // 64bit required
  int32_t denominator = 1342095361 - precal; // 1342095361 is 5 * 0x7FFF^2 / 4
  int16_t result = numerator / denominator;
  return result * scale;
}

int16_t cos16_t(uint16_t theta) {
  return sin16_t(theta + 16384); //cos(x) = sin(x+pi/2)
}

uint8_t sin8_t(uint8_t theta) {
  int32_t sin16 = sin16_t((uint16_t)theta * 257); // 255 * 257 = 0xFFFF
  sin16 += 0x7FFF; //shift result to range 0-0xFFFF
  return sin16 >> 8;
}

uint8_t cos8_t(uint8_t theta) {
  return sin8_t(theta + 64); //cos(x) = sin(x+pi/2)
}

float sin_approx(float theta)
{
  theta = modd(theta, TWO_PI); // modulo: bring to -2pi to 2pi range
  if(theta < 0) theta += M_TWOPI; // 0-2pi range
  uint16_t scaled_theta = (uint16_t)(theta * (0xFFFF / M_TWOPI));
  int32_t result = sin16_t(scaled_theta);
  float sin = float(result) / 0x7FFF;
  return sin;
}

float cos_approx(float theta)
{
  return sin_approx(theta + M_PI_2);
}

float tan_approx(float x) {
  float c = cos_approx(x);
  if (c==0.0f) return 0;
  float res = sin_approx(x) / c;
  return res;
}

#define ATAN2_CONST_A 0.1963f
#define ATAN2_CONST_B 0.9817f

// fast atan2() approximation source: public domain
float atan2_t(float y, float x) {
  if (x == 0.0f) return (y > 0.0f) ? M_PI_2 : (y < 0.0f) ? -M_PI_2 : 0.0f;

  float abs_y = (y < 0.0f) ? -y : y + 1e-10f;  // make sure y is not zero to prevent division by 0
  float z = abs_y / x;
  float atan_approx;

  if (z < 1.0f) {
    atan_approx = z / (1.0f + ATAN2_CONST_A * z * z);
    if (x < 0.0f) {
      return (y >= 0.0f) ? atan_approx + PI : atan_approx - PI;
    }
  }
  else {
    z = x / abs_y;
    atan_approx = M_PI_2 - z / (1.0f + ATAN2_CONST_A * z * z);
    if (y < 0.0f) {
      return -atan_approx;
    }
  }
  return atan_approx;
}

//https://stackoverflow.com/questions/3380628
// Absolute error <= 6.7e-5
float acos_t(float x) {
  float negate = float(x < 0);
  float xabs = std::abs(x);
  float ret = -0.0187293f;
  ret = ret * xabs;
  ret = ret + 0.0742610f;
  ret = ret * xabs;
  ret = ret - 0.2121144f;
  ret = ret * xabs;
  ret = ret + HALF_PI;
  ret = ret * sqrt(1.0f-xabs);
  ret = ret - 2 * negate * ret;
  float res = negate * PI + ret;
  DEBUGM_PRINTF_P(PSTR("acos: %f,%f,%f,(%f)\n"),x,res,acos(x),res-acos(x));
  return res;
}

float asin_t(float x) {
  float res = HALF_PI - acos_t(x);
  DEBUGM_PRINTF_P(PSTR("asin: %f,%f,%f,(%f)\n"),x,res,asin(x),res-asin(x));
  return res;
}

// declare a template with no implementation, and only one specialization
// this allows hiding the constants, while ensuring ODR causes optimizations
// to still apply.  (Fixes issues with conflicting 3rd party #define's)
template <typename T> T atan_t(T x);
template<>
float atan_t(float xInput) {
  //For A/B/C, see https://stackoverflow.com/a/42542593
  static const double A { 0.0776509570923569 };
  static const double B { -0.287434475393028 };
  static const double C { ((HALF_PI/2) - A - B) };
  // polynominal factors for approximation between 1 and 5
  static const float C0 {  0.089494f };
  static const float C1 {  0.974207f };
  static const float C2 { -0.326175f };
  static const float C3 {  0.05375f  };
  static const float C4 { -0.003445f };

  bool neg = (xInput < 0);
  float x = std::abs(xInput);
  float res;
  if (x > 5.0f) { // atan(x) converges to pi/2 - (1/x) for large values
    res = HALF_PI - (1.0f/x);
  } else if (x > 1.0f) { //1 < x < 5
    float xx = x * x;
    res = (C4*xx*xx)+(C3*xx*x)+(C2*xx)+(C1*x)+C0;
  } else { // this approximation is only for x <= 1
    float xx = x * x;
    res = ((A*xx + B)*xx + C)*x;
  }
  if (neg) {
    res = -res;
  }
  DEBUGM_PRINTF_P(PSTR("atan: %f,%f,%f,(%f)\n"),xInput,res,atan(xInput),res-atan(xInput));
  return res;
}

float floor_t(float x) {
  bool neg = x < 0;
  int val = x;
  if (neg) val--;
  DEBUGM_PRINTF_P(PSTR("floor: %f,%f,%f\n"),x,(float)val,floor(x));
  return val;
}

float fmod_t(float num, float denom) {
  int tquot = num / denom;
  float res = num - tquot * denom;
  DEBUGM_PRINTF_P(PSTR("fmod: %f,%f,(%f)\n"),res,fmod(num,denom),res-fmod(num,denom));
  return res;
}
