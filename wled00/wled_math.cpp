/*
 * Contains some trigonometric functions.
 * The ANSI C equivalents are likely faster, but using any sin/cos/tan function incurs a memory penalty of 460 bytes on ESP8266, likely for lookup tables.
 * This implementation has no extra static memory usage.
 *
 * Source of the cos_t() function: https://web.eecs.utk.edu/~azh/blog/cosine.html (cos_taylor_literal_6terms)
 */

#include <Arduino.h> //PI constant

//#define WLED_DEBUG_MATH

// Note: cos_t, sin_t and tan_t are very accurate but slow
// the math.h functions use several kB of flash and are to be avoided if possible
// sin16_t / cos16_t are faster and much more accurate than the fastled variants
// sin_approx and cos_approx are float wrappers for sin16_t/cos16_t and have an accuracy better than +/-0.0015 compared to sinf()
// sin8_t / cos8_t are fastled replacements and use sin16_t / cos16_t. Slightly slower than fastled version but very accurate


// Taylor series approximations, replaced with Bhaskara I's approximation
/*
#define modd(x, y) ((x) - (int)((x) / (y)) * (y))

float cos_t(float phi)
{
  float x = modd(phi, M_TWOPI);
  if (x < 0) x = -1 * x;
  int8_t sign = 1;
  if (x > M_PI)
  {
      x -= M_PI;
      sign = -1;
  }
  float xx = x * x;

  float res = sign * (1 - ((xx) / (2)) + ((xx * xx) / (24)) - ((xx * xx * xx) / (720)) + ((xx * xx * xx * xx) / (40320)) - ((xx * xx * xx * xx * xx) / (3628800)) + ((xx * xx * xx * xx * xx * xx) / (479001600)));
  #ifdef WLED_DEBUG_MATH
  Serial.printf("cos: %f,%f,%f,(%f)\n",phi,res,cos(x),res-cos(x));
  #endif
  return res;
}

float sin_t(float phi) {
  float res =  cos_t(M_PI_2 - phi);
  #ifdef WLED_DEBUG_MATH
  Serial.printf("sin: %f,%f,%f,(%f)\n",x,res,sin(x),res-sin(x));
  #endif
  return res;
}

float tan_t(float x) {
  float c = cos_t(x);
  if (c==0.0f) return 0;
  float res = sin_t(x) / c;
  #ifdef WLED_DEBUG_MATH
  Serial.printf("tan: %f,%f,%f,(%f)\n",x,res,tan(x),res-tan(x));
  #endif
  return res;
}
*/

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
  return sin16_t(theta + 0x4000); //cos(x) = sin(x+pi/2)
}

uint8_t sin8_t(uint8_t theta) {
  int32_t sin16 = sin16_t((uint16_t)theta * 257); // 255 * 257 = 0xFFFF
  sin16 += 0x7FFF + 128; //shift result to range 0-0xFFFF, +128 for rounding
  return min(sin16, int32_t(0xFFFF)) >> 8; // min performs saturation, and prevents overflow
}

uint8_t cos8_t(uint8_t theta) {
  return sin8_t(theta + 64); //cos(x) = sin(x+pi/2)
}

float sin_approx(float theta) {
  uint16_t scaled_theta = (int)(theta * (float)(0xFFFF / M_TWOPI)); // note: do not cast negative float to uint! cast to int first (undefined on C3)
  int32_t result = sin16_t(scaled_theta);
  float sin = float(result) / 0x7FFF;
  return sin;
}

float cos_approx(float theta) {
  uint16_t scaled_theta = (int)(theta * (float)(0xFFFF / M_TWOPI)); // note: do not cast negative float to uint! cast to int first (undefined on C3)
  int32_t result = sin16_t(scaled_theta + 0x4000);
  float cos = float(result) / 0x7FFF;
  return cos;
}

float tan_approx(float x) {
  float c = cos_approx(x);
  if (c==0.0f) return 0;
  float res = sin_approx(x) / c;
  return res;
}

#define ATAN2_CONST_A 0.1963f
#define ATAN2_CONST_B 0.9817f

// atan2_t approximation, with the idea from https://gist.github.com/volkansalma/2972237?permalink_comment_id=3872525#gistcomment-3872525
float atan2_t(float y, float x) {
	float abs_y = fabs(y);
  float abs_x = fabs(x);
  float r = (abs_x - abs_y) / (abs_y + abs_x + 1e-10f); // avoid division by zero by adding a small nubmer
  float angle;
  if(x < 0) {
    r = -r;
    angle = M_PI_2 + M_PI_4;
  }
  else
    angle = M_PI_2 - M_PI_4;

  float add = (ATAN2_CONST_A * (r * r) - ATAN2_CONST_B) * r;
	angle += add;
  angle = y < 0 ? -angle : angle;
	return angle;
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
  ret = ret + M_PI_2;
  ret = ret * sqrt(1.0f-xabs);
  ret = ret - 2 * negate * ret;
  float res = negate * M_PI + ret;
  #ifdef WLED_DEBUG_MATH
  Serial.printf("acos: %f,%f,%f,(%f)\n",x,res,acos(x),res-acos(x));
  #endif
  return res;
}

float asin_t(float x) {
  float res = M_PI_2 - acos_t(x);
  #ifdef WLED_DEBUG_MATH
  Serial.printf("asin: %f,%f,%f,(%f)\n",x,res,asin(x),res-asin(x));
  #endif
  return res;
}

// declare a template with no implementation, and only one specialization
// this allows hiding the constants, while ensuring ODR causes optimizations
// to still apply.  (Fixes issues with conflicting 3rd party #define's)
template <typename T> T atan_t(T x);
template<>
float atan_t(float x) {
  //For A/B/C, see https://stackoverflow.com/a/42542593
  static const double A { 0.0776509570923569 };
  static const double B { -0.287434475393028 };
  static const double C { ((M_PI_4) - A - B) };
  // polynominal factors for approximation between 1 and 5
  static const float C0 {  0.089494f };
  static const float C1 {  0.974207f };
  static const float C2 { -0.326175f };
  static const float C3 {  0.05375f  };
  static const float C4 { -0.003445f };

  #ifdef WLED_DEBUG_MATH
  float xinput = x;
  #endif
  bool neg = (x < 0);
  x = std::abs(x);
  float res;
  if (x > 5.0f) { // atan(x) converges to pi/2 - (1/x) for large values
    res = M_PI_2 - (1.0f/x);
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
  #ifdef WLED_DEBUG_MATH
  Serial.printf("atan: %f,%f,%f,(%f)\n",xinput,res,atan(xinput),res-atan(xinput));
  #endif
  return res;
}

float floor_t(float x) {
  bool neg = x < 0;
  int val = x;
  if (neg) val--;
  #ifdef WLED_DEBUG_MATH
  Serial.printf("floor: %f,%f,%f\n",x,(float)val,floor(x));
  #endif
  return val;
}

float fmod_t(float num, float denom) {
  int tquot = num / denom;
  float res = num - tquot * denom;
  #ifdef WLED_DEBUG_MATH
  Serial.printf("fmod: %f,%f,(%f)\n",res,fmod(num,denom),res-fmod(num,denom));
  #endif
  return res;
}

// bit-wise integer square root calculation (exact)
uint32_t sqrt32_bw(uint32_t x) {
  uint32_t res = 0;
  uint32_t bit;
  uint32_t num = x; // use 32bit for faster calculation

  if(num < 1 << 10)  bit = 1 << 10; // speed optimization for small numbers < 32^2
  else if (num < 1 << 20) bit = 1 << 20; // speed optimization for medium numbers < 1024^2
  else bit = 1 << 30; // start with highest power of 4 <= 2^32

  while (bit > num) bit >>= 2; // reduce iterations

  while (bit != 0) {
    if (num >= res + bit) {
      num -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return res;
}
