#ifndef PalettesWLED_h
#define PalettesWLED_h

/*
 * WLED Color palettes
 *
 * Note: palettes imported from http://seaviewsensing.com/pub/cpt-city are gamma corrected using gammas (1.182, 1.0, 1.136)
 *       this is done to match colors of the palettes after applying the (default) global gamma of 2.2 to versions
 *       prior to WLED 0.16 which used pre-applied gammas of (2.6,2.2,2.5) for these palettes.
 *       Palettes from FastLED are intended to be used without gamma correction, an inverse gamma of 2.2 is applied to original colors
 */

// Gradient palette "ib_jul01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ing/xmas/ib_jul01.c3g
const uint8_t ib_jul01_gp[] PROGMEM = {
    0, 226,   6,  12,
   94,  26,  96,  78,
  132, 130, 189,  94,
  255, 177,   3,   9};

// Gradient palette "es_vintage_57_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/vintage/es_vintage_57.c3g
const uint8_t es_vintage_57_gp[] PROGMEM = {
    0,  29,   8,   3,
   53,  76,   1,   0,
  104, 142,  96,  28,
  153, 211, 191,  61,
  255, 117, 129,  42};

// Gradient palette "es_vintage_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/vintage/es_vintage_01.c3g
const uint8_t es_vintage_01_gp[] PROGMEM = {
    0,  41,  18,  24,
   51,  73,   0,  22,
   76, 165, 170,  38,
  101, 255, 189,  80,
  127, 139,  56,  40,
  153,  73,   0,  22,
  229,  41,  18,  24,
  255,  41,  18,  24};

// Gradient palette "es_rivendell_15_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/rivendell/es_rivendell_15.c3g
const uint8_t es_rivendell_15_gp[] PROGMEM = {
    0,  24,  69,  44,
  101,  73, 105,  70,
  165, 129, 140,  97,
  242, 200, 204, 166,
  255, 200, 204, 166};

// Gradient palette "rgi_15_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ds/rgi/rgi_15.c3g
const uint8_t rgi_15_gp[] PROGMEM = {
    0,  41,  14,  99,
   31, 128,  24,  74,
   63, 227,  34,  50,
   95, 132,  31,  76,
  127,  47,  29, 102,
  159, 109,  47, 101,
  191, 176,  66, 100,
  223, 129,  57, 104,
  255,  84,  48, 108};

// Gradient palette "retro2_16_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ma/retro2/retro2_16.c3g
const uint8_t retro2_16_gp[] PROGMEM = {
    0, 222, 191,   8,
  255, 117,  52,   1};

// Gradient palette "Analogous_1_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/red/Analogous_1.c3g
const uint8_t Analogous_1_gp[] PROGMEM = {
    0,  38,   0, 255,
   63,  86,   0, 255,
  127, 139,   0, 255,
  191, 196,   0, 117,
  255, 255,   0,   0};

// Gradient palette "es_pinksplash_08_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/pink_splash/es_pinksplash_08.c3g
const uint8_t es_pinksplash_08_gp[] PROGMEM = {
    0, 186,  63, 255,
  127, 227,   9,  85,
  175, 234, 205, 213,
  221, 205,  38, 176,
  255, 205,  38, 176,
};

// Gradient palette "es_ocean_breeze_036_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/ocean_breeze/es_ocean_breeze_036.c3g
const uint8_t es_ocean_breeze_036_gp[] PROGMEM = {
    0,  16,  48,  51,
   89,  27, 166, 175,
  153, 197, 233, 255,
  255,   0, 145, 152};

// Gradient palette "departure_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/mjf/departure.c3g
const uint8_t departure_gp[] PROGMEM = {
    0,  53,  34,   0,
   42,  86,  51,   0,
   63, 147, 108,  49,
   84, 212, 166, 108,
  106, 235, 212, 180,
  116, 255, 255, 255,
  138, 191, 255, 193,
  148,  84, 255,  88,
  170,   0, 255,   0,
  191,   0, 192,   0,
  212,   0, 128,   0,
  255,   0, 128,   0};

// Gradient palette "es_landscape_64_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/landscape/es_landscape_64.c3g
const uint8_t es_landscape_64_gp[] PROGMEM = {
    0,   0,   0,   0,
   37,  31,  89,  19,
   76,  72, 178,  43,
  127, 150, 235,   5,
  128, 186, 234, 119,
  130, 222, 233, 252,
  153, 197, 219, 231,
  204, 132, 179, 253,
  255,  28, 107, 225};

// Gradient palette "es_landscape_33_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/landscape/es_landscape_33.c3g
const uint8_t es_landscape_33_gp[] PROGMEM = {
    0,  12,  45,   0,
   19, 101,  86,   2,
   38, 207, 128,   4,
   63, 243, 197,  18,
   66, 109, 196, 146,
  255,   5,  39,   7};

// Gradient palette "rainbowsherbet_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ma/icecream/rainbowsherbet.c3g
const uint8_t rainbowsherbet_gp[] PROGMEM = {
    0, 255, 102,  41,
   43, 255, 140,  90,
   86, 255,  51,  90,
  127, 255, 153, 169,
  170, 255, 255, 249,
  209, 113, 255,  85,
  255, 157, 255, 137};

// Gradient palette "gr65_hult_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/hult/gr65_hult.c3g
const uint8_t gr65_hult_gp[] PROGMEM = {
    0, 251, 216, 252,
   48, 255, 192, 255,
   89, 239,  95, 241,
  160,  51, 153, 217,
  216,  24, 184, 174,
  255,  24, 184, 174};

// Gradient palette "gr64_hult_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/hult/gr64_hult.c3g
const uint8_t gr64_hult_gp[] PROGMEM = {
    0,  24, 184, 174,
   66,   8, 162, 150,
  104, 124, 137,   7,
  130, 178, 186,  22,
  150, 124, 137,   7,
  201,   6, 156, 144,
  239,   0, 128, 117,
  255,   0, 128, 117};

// Gradient palette "GMT_drywet_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/gmt/GMT_drywet.c3g
const uint8_t GMT_drywet_gp[] PROGMEM = {
    0, 119,  97,  33,
   42, 235, 199,  88,
   84, 169, 238, 124,
  127,  37, 238, 232,
  170,   7, 120, 236,
  212,  27,   1, 175,
  255,   4,  51, 101};

// Gradient palette "ib15_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ing/general/ib15.c3g
const uint8_t ib15_gp[] PROGMEM = {
    0, 177, 160, 199,
   72, 205, 158, 149,
   89, 233, 155, 101,
  107, 255,  95,  63,
  141, 192,  98, 109,
  255, 132, 101, 159};

// Gradient palette "Tertiary_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/vermillion/Tertiary_01.c3g
const uint8_t Tertiary_01_gp[] PROGMEM = {
    0,   0,  25, 255,
   63,  38, 140, 117,
  127,  86, 255,   0,
  191, 167, 140,  19,
  255, 255,  25,  41};

// Gradient palette "lava_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/neota/elem/lava.c3g
const uint8_t lava_gp[] PROGMEM = {
    0,   0,   0,   0,
   46,  77,   0,   0,
   96, 177,   0,   0,
  108, 196,  38,   9,
  119, 215,  76,  19,
  146, 235, 115,  29,
  174, 255, 153,  41,
  188, 255, 178,  41,
  202, 255, 204,  41,
  218, 255, 230,  41,
  234, 255, 255,  41,
  244, 255, 255, 143,
  255, 255, 255, 255};

// Gradient palette "fierce-ice_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/neota/elem/fierce-ice.c3g
const uint8_t fierce_ice_gp[] PROGMEM = {
    0,   0,   0,   0,
   59,   0,  51, 117,
  119,   0, 102, 255,
  149,  38, 153, 255,
  180,  86, 204, 255,
  217, 167, 230, 255,
  255, 255, 255, 255};

// Gradient palette "Colorfull_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Colorfull.c3g
const uint8_t Colorfull_gp[] PROGMEM = {
    0,  61, 155,  44,
   25,  95, 174,  77,
   60, 132, 193, 113,
   93, 154, 166, 125,
  106, 175, 138, 136,
  109, 183, 121, 137,
  113, 194, 104, 138,
  116, 225, 179, 165,
  124, 255, 255, 192,
  168, 167, 218, 203,
  255,  84, 182, 215};

// Gradient palette "Pink_Purple_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Pink_Purple.c3g
const uint8_t Pink_Purple_gp[] PROGMEM = {
    0,  79,  32, 109,
   25,  90,  40, 117,
   51, 102,  48, 124,
   76, 141, 135, 185,
  102, 180, 222, 248,
  109, 208, 236, 252,
  114, 237, 250, 255,
  122, 206, 200, 239,
  149, 177, 149, 222,
  183, 187, 130, 203,
  255, 198, 111, 184};

// Gradient palette "Sunset_Real_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Sunset_Real.c3g
const uint8_t Sunset_Real_gp[] PROGMEM = {
    0, 181,   0,   0,
   22, 218,  85,   0,
   51, 255, 170,   0,
   85, 211,  85,  77,
  135, 167,   0, 169,
  198,  73,   0, 188,
  255,   0,   0, 207};

// Gradient palette "Sunset_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Sunset_Yellow.c3g
const uint8_t Sunset_Yellow_gp[] PROGMEM = {
    0,  61, 135, 184,
   36, 129, 188, 169,
   87, 203, 241, 155,
  100, 228, 237, 141,
  107, 255, 232, 127,
  115, 251, 202, 130,
  120, 248, 172, 133,
  128, 251, 202, 130,
  180, 255, 232, 127,
  223, 255, 242, 120,
  255, 255, 252, 113};

// Gradient palette "Beech_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Beech.c3g
const uint8_t Beech_gp[] PROGMEM = {
    0, 255, 254, 236,
   12, 255, 254, 236,
   22, 255, 254, 236,
   26, 223, 224, 178,
   28, 192, 195, 124,
   28, 176, 255, 231,
   50, 123, 251, 236,
   71,  74, 246, 241,
   93,  33, 225, 228,
  120,   0, 204, 215,
  133,   4, 168, 178,
  136,  10, 132, 143,
  136,  51, 189, 212,
  208,  23, 159, 201,
  255,   0, 129, 190};

// Gradient palette "Another_Sunset_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Another_Sunset.c3g
const uint8_t Another_Sunset_gp[] PROGMEM = {
    0, 175, 121,  62,
   29, 128, 103,  60,
   68,  84,  84,  58,
   68, 248, 184,  55,
   97, 239, 204,  93,
  124, 230, 225, 133,
  178, 102, 125, 129,
  255,   0,  26, 125};

// Gradient palette "es_autumn_19_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/autumn/es_autumn_19.c3g
const uint8_t es_autumn_19_gp[] PROGMEM = {
    0,  90,  14,   5,
   51, 139,  41,  13,
   84, 180,  70,  17,
  104, 192, 202, 125,
  112, 177, 137,   3,
  122, 190, 200, 131,
  124, 192, 202, 124,
  135, 177, 137,   3,
  142, 194, 203, 118,
  163, 177,  68,  17,
  204, 128,  35,  12,
  249,  74,   5,   2,
  255,  74,   5,   2};

// Gradient palette "BlacK_Blue_Magenta_White_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Blue_Magenta_White.c3g
const uint8_t BlacK_Blue_Magenta_White_gp[] PROGMEM = {
    0,   0,   0,   0,
   42,   0,   0, 117,
   84,   0,   0, 255,
  127, 113,   0, 255,
  170, 255,   0, 255,
  212, 255, 128, 255,
  255, 255, 255, 255};

// Gradient palette "BlacK_Magenta_Red_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Magenta_Red.c3g
const uint8_t BlacK_Magenta_Red_gp[] PROGMEM = {
    0,   0,   0,   0,
   63, 113,   0, 117,
  127, 255,   0, 255,
  191, 255,   0, 117,
  255, 255,   0,   0};

// Gradient palette "BlacK_Red_Magenta_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Red_Magenta_Yellow.c3g
const uint8_t BlacK_Red_Magenta_Yellow_gp[] PROGMEM = {
    0,   0,   0,   0,
   42, 113,   0,   0,
   84, 255,   0,   0,
  127, 255,   0, 117,
  170, 255,   0, 255,
  212, 255, 128, 117,
  255, 255, 255,   0};

// Gradient palette "Blue_Cyan_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/Blue_Cyan_Yellow.c3g
const uint8_t Blue_Cyan_Yellow_gp[] PROGMEM = {
    0,   0,   0, 255,
   63,   0, 128, 255,
  127,   0, 255, 255,
  191, 113, 255, 117,
  255, 255, 255,   0};

//Custom palette by Aircoookie
const byte Orange_Teal_gp[] PROGMEM = {
    0,   0,150, 92,
   55,   0,150, 92,
  200, 255, 72,  0,
  255, 255, 72,  0};

//Custom palette by Aircoookie
const byte Tiamat_gp[] PROGMEM = {
    0,   1,  2, 14, //gc
   33,   2,  5, 35, //gc from 47, 61,126
  100,  13,135, 92, //gc from 88,242,247
  120,  43,255,193, //gc from 135,255,253
  140, 247,  7,249, //gc from 252, 69,253
  160, 193, 17,208, //gc from 231, 96,237
  180,  39,255,154, //gc from 130, 77,213
  200,   4,213,236, //gc from 57,122,248
  220,  39,252,135, //gc from 177,254,255
  240, 193,213,253, //gc from 203,239,253
  255, 255,249,255};

//Custom palette by Aircoookie
const byte April_Night_gp[] PROGMEM = {
    0,   1,  5, 45, //deep blue
   10,   1,  5, 45,
   25,   5,169,175, //light blue
   40,   1,  5, 45,
   61,   1,  5, 45,
   76,  45,175, 31, //green
   91,   1,  5, 45,
  112,   1,  5, 45,
  127, 249,150,  5, //yellow
  143,   1,  5, 45,
  162,   1,  5, 45,
  178, 255, 92,  0, //pastel orange
  193,   1,  5, 45,
  214,   1,  5, 45,
  229, 223, 45, 72, //pink
  244,   1,  5, 45,
  255,   1,  5, 45};

const byte Orangery_gp[] PROGMEM = {
    0, 255, 95, 23,
   30, 255, 82,  0,
   60, 223, 13,  8,
   90, 144, 44,  2,
  120, 255,110, 17,
  150, 255, 69,  0,
  180, 158, 13, 11,
  210, 241, 82, 17,
  255, 213, 37,  4};

//inspired by Mark Kriegsman https://gist.github.com/kriegsman/756ea6dcae8e30845b5a
const byte C9_gp[] PROGMEM = {
    0, 184,  4,  0, //red
   60, 184,  4,  0,
   65, 144, 44,  2, //amber
  125, 144, 44,  2,
  130,   4, 96,  2, //green
  190,   4, 96,  2,
  195,   7,  7, 88, //blue
  255,   7,  7, 88};

const byte Sakura_gp[] PROGMEM = {
    0, 196, 19, 10,
   65, 255, 69, 45,
  130, 223, 45, 72,
  195, 255, 82,103,
  255, 223, 13, 17};

const byte Aurora_gp[] PROGMEM = {
    0,   1,  5, 45, //deep blue
   64,   0,200, 23,
  128,   0,255,  0, //green
  170,   0,243, 45,
  200,   0,135,  7,
  255,   1,  5, 45};//deep blue

const byte Atlantica_gp[] PROGMEM = {
    0,   0, 28,112, //#001C70
   50,  32, 96,255, //#2060FF
  100,   0,243, 45,
  150,  12, 95, 82, //#0C5F52
  200,  25,190, 95, //#19BE5F
  255,  40,170, 80};//#28AA50

  const byte C9_2_gp[] PROGMEM = {
    0,   6, 126,   2, //green
   45,   6, 126,   2,
   46,   4,  30, 114, //blue
   90,   4,  30, 114,
   91, 255,   5,   0, //red
  135, 255,   5,   0,
  136, 196,  57,   2, //amber
  180, 196,  57,   2,
  181, 137,  85,   2, //yellow
  255, 137,  85,   2};

  //C9, but brighter and with a less purple blue
  const byte C9_new_gp[] PROGMEM = {
    0, 255,   5,   0, //red
   60, 255,   5,   0,
   61, 196,  57,   2, //amber (start 61?)
  120, 196,  57,   2,
  121,   6, 126,   2, //green (start 126?)
  180,   6, 126,   2,
  181,   4,  30, 114, //blue (start 191?)
  255,   4,  30, 114};

// Gradient palette "temperature_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/arendal/temperature.c3g
const uint8_t temperature_gp[] PROGMEM = {
    0,  20,  92, 171,
   14,  15, 111, 186,
   28,   6, 142, 211,
   42,   2, 161, 227,
   56,  16, 181, 239,
   70,  38, 188, 201,
   84,  86, 204, 200,
   99, 139, 219, 176,
  113, 182, 229, 125,
  127, 196, 230,  63,
  141, 241, 240,  22,
  155, 254, 222,  30,
  170, 251, 199,   4,
  184, 247, 157,   9,
  198, 243, 114,  15,
  226, 213,  30,  29,
  240, 151,  38,  35,
  255, 151,  38,  35};

// Gradient palette "bhw1_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_01.c3g
const uint8_t retro_clown_gp[] PROGMEM = {
    0, 242, 168,  38,
  117, 226,  78,  80,
  255, 161,  54, 225,
};

// Gradient palette "bhw1_04_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_04.c3g
const uint8_t candy_gp[] PROGMEM = {
    0, 243, 242,  23,
   15, 242, 168,  38,
  142, 111,  21, 151,
  198,  74,  22, 150,
  255,   0,   0, 117};

// Gradient palette "bhw1_05_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_05.c3g
const uint8_t toxy_reaf_gp[] PROGMEM = {
    0,   2, 239, 126,
  255, 145,  35, 217};

// Gradient palette "bhw1_06_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_06.c3g
const uint8_t fairy_reaf_gp[] PROGMEM = {
    0, 220,  19, 187,
  160,  12, 225, 219,
  219, 203, 242, 223,
  255, 255, 255, 255};

// Gradient palette "bhw1_14_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_14.c3g
const uint8_t semi_blue_gp[] PROGMEM = {
    0,   0,   0,   0,
   12,  24,   4,  38,
   53,  55,   8,  84,
   80,  43,  48, 159,
  119,  31,  89, 237,
  145,  50,  59, 166,
  186,  71,  30,  98,
  233,  31,  15,  45,
  255,   0,   0,   0};

// Gradient palette "bhw1_three_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_three.c3g
const uint8_t pink_candy_gp[] PROGMEM = {
    0, 255, 255, 255,
   45,  50,  64, 255,
  112, 242,  16, 186,
  140, 255, 255, 255,
  155, 242,  16, 186,
  196, 116,  13, 166,
  255, 255, 255, 255};

// Gradient palette "bhw1_w00t_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_w00t.c3g
const uint8_t red_reaf_gp[] PROGMEM = {
    0,  36,  68, 114,
  104, 149, 195, 248,
  188, 255,   0,   0,
  255,  94,  14,   9};

// Gradient palette "bhw2_23_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_23.c3g
const uint8_t aqua_flash_gp[] PROGMEM = {
    0,   0,   0,   0,
   66, 130, 242, 245,
   96, 255, 255,  53,
  124, 255, 255, 255,
  153, 255, 255,  53,
  188, 130, 242, 245,
  255,   0,   0,   0};

// Gradient palette "bhw2_xc_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_xc.c3g
const uint8_t yelblu_hot_gp[] PROGMEM = {
    0,  43,  30,  57,
   58,  73,   0, 119,
  122,  87,   0,  74,
  158, 197,  57,  22,
  183, 218, 117,  27,
  219, 239, 177,  32,
  255, 246, 247,  27,
};

// Gradient palette "bhw2_45_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_45.c3g
const uint8_t lite_light_gp[] PROGMEM = {
    0,   0,   0,   0,
    9,  20,  21,  22,
   40,  46,  43,  49,
   66,  46,  43,  49,
  101,  61,  16,  65,
  255,   0,   0,   0};

// Gradient palette "bhw2_22_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_22.c3g
const uint8_t red_flash_gp[] PROGMEM = {
    0,   0,   0,   0,
   99, 242,  12,   8,
  130, 253, 228, 163,
  155, 242,  12,   8,
  255,   0,   0,   0};

// Gradient palette "bhw3_40_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw3/bhw3_40.c3g
const uint8_t blink_red_gp[] PROGMEM = {
    0,   4,   7,   4,
   43,  40,  25,  62,
   76,  61,  15,  36,
  109, 207,  39,  96,
  127, 255, 156, 184,
  165, 185,  73, 207,
  204, 105,  66, 240,
  255,  77,  29,  78};

// Gradient palette "bhw3_52_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw3/bhw3_52.c3g
const uint8_t red_shift_gp[] PROGMEM = {
    0,  98,  22,  93,
   45, 103,  22,  73,
   99, 192,  45,  56,
  132, 235, 187,  59,
  175, 228,  85,  26,
  201, 228,  56,  48,
  255,   2,   0,   2};

// Gradient palette "bhw4_097_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw4/bhw4_097.c3g
const uint8_t red_tide_gp[] PROGMEM = {
    0, 251,  46,   0,
   28, 255, 139,  25,
   43, 246, 158,  63,
   58, 246, 216, 123,
   84, 243,  94,  10,
  114, 177,  65,  11,
  140, 255, 241, 115,
  168, 177,  65,  11,
  196, 250, 233, 158,
  216, 255,  94,   6,
  255, 126,   8,   4};

// Gradient palette "bhw4_017_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw4/bhw4_017.c3g
const uint8_t candy2_gp[] PROGMEM = {
    0, 109, 102, 102,
   25,  42,  49,  71,
   48, 121,  96,  84,
   73, 241, 214,  26,
   89, 216, 104,  44,
  130,  42,  49,  71,
  163, 255, 177,  47,
  186, 241, 214,  26,
  211, 109, 102, 102,
  255,  20,  19,  13};

const byte trafficlight_gp[] PROGMEM = {
    0,   0,   0, 0,   //black
   85,   0, 255, 0,   //green
  170, 255, 255, 0,   //yellow
  255, 255,   0, 0};  //red

const byte Aurora2_gp[] PROGMEM = {
    0,  17, 177,  13,    //Greenish
   64, 121, 242,   5,    //Greenish
  128,  25, 173, 121,    //Turquoise
  192, 250,  77, 127,    //Pink
  255, 171, 101, 221};   //Purple

// FastLed palettes, corrected with inverse gamma of 2.2 to match original looks

// Party colors
const TProgmemRGBPalette16 PartyColors_gc22 FL_PROGMEM = {
  0x9B00D5, 0xBD00B8, 0xDA0092, 0xF3005C,
  0xF45500, 0xDC8F00, 0xD5B400, 0xD5D500,
  0xD59B00, 0xEF6600, 0xF90044, 0xE10086,
  0xC400B0, 0xA300CF, 0x7600E8, 0x0032FC};

// Rainbow colors
const TProgmemRGBPalette16 RainbowColors_gc22 FL_PROGMEM = {
  0xFF0000, 0xEB7000, 0xD59B00, 0xD5BA00,
  0xD5D500, 0x9CEB00, 0x00FF00, 0x00EB70,
  0x00D59B, 0x009CD4, 0x0000FF, 0x7000EB,
  0x9B00D5, 0xBA00BB, 0xD5009B, 0xEB0072};

// Rainbow colors with alternatating stripes of black
const TProgmemRGBPalette16 RainbowStripeColors_gc22 FL_PROGMEM = {
  0xFF0000, 0x000000, 0xD59B00, 0x000000,
  0xD5D500, 0x000000, 0x00FF00, 0x000000,
  0x00D59B, 0x000000, 0x0000FF, 0x000000,
  0x9B00D5, 0x000000, 0xD5009B, 0x000000};

// array of fastled palettes (palette 6 - 12)
const TProgmemRGBPalette16 *const fastledPalettes[] PROGMEM = {
  &PartyColors_gc22,            //06-00 Party
  &CloudColors_p,               //07-01 Cloud
  &LavaColors_p,                //08-02 Lava
  &OceanColors_p,               //09-03 Ocean
  &ForestColors_p,              //10-04 Forest
  &RainbowColors_gc22,          //11-05 Rainbow
  &RainbowStripeColors_gc22     //12-06 Rainbow Bands
};

// Single array of defined cpt-city color palettes.
// This will let us programmatically choose one based on
// a number, rather than having to activate each explicitly
// by name every time.
const uint8_t* const gGradientPalettes[] PROGMEM = {
  Sunset_Real_gp,               //13-00 Sunset
  es_rivendell_15_gp,           //14-01 Rivendell
  es_ocean_breeze_036_gp,       //15-02 Breeze
  rgi_15_gp,                    //16-03 Red & Blue
  retro2_16_gp,                 //17-04 Yellowout
  Analogous_1_gp,               //18-05 Analogous
  es_pinksplash_08_gp,          //19-06 Splash
  Sunset_Yellow_gp,             //20-07 Pastel
  Another_Sunset_gp,            //21-08 Sunset2
  Beech_gp,                     //22-09 Beech
  es_vintage_01_gp,             //23-10 Vintage
  departure_gp,                 //24-11 Departure
  es_landscape_64_gp,           //25-12 Landscape
  es_landscape_33_gp,           //26-13 Beach
  rainbowsherbet_gp,            //27-14 Sherbet
  gr65_hult_gp,                 //28-15 Hult
  gr64_hult_gp,                 //29-16 Hult64
  GMT_drywet_gp,                //30-17 Drywet
  ib_jul01_gp,                  //31-18 Jul
  es_vintage_57_gp,             //32-19 Grintage
  ib15_gp,                      //33-20 Rewhi
  Tertiary_01_gp,               //34-21 Tertiary
  lava_gp,                      //35-22 Fire
  fierce_ice_gp,                //36-23 Icefire
  Colorfull_gp,                 //37-24 Cyane
  Pink_Purple_gp,               //38-25 Light Pink
  es_autumn_19_gp,              //39-26 Autumn
  BlacK_Blue_Magenta_White_gp,  //40-27 Magenta
  BlacK_Magenta_Red_gp,         //41-28 Magred
  BlacK_Red_Magenta_Yellow_gp,  //42-29 Yelmag
  Blue_Cyan_Yellow_gp,          //43-30 Yelblu
  Orange_Teal_gp,               //44-31 Orange & Teal
  Tiamat_gp,                    //45-32 Tiamat
  April_Night_gp,               //46-33 April Night
  Orangery_gp,                  //47-34 Orangery
  C9_gp,                        //48-35 C9
  Sakura_gp,                    //49-36 Sakura
  Aurora_gp,                    //50-37 Aurora
  Atlantica_gp,                 //51-38 Atlantica
  C9_2_gp,                      //52-39 C9 2
  C9_new_gp,                    //53-40 C9 New
  temperature_gp,               //54-41 Temperature
  Aurora2_gp,                   //55-42 Aurora 2
  retro_clown_gp,               //56-43 Retro Clown
  candy_gp,                     //57-44 Candy
  toxy_reaf_gp,                 //58-45 Toxy Reaf
  fairy_reaf_gp,                //59-46 Fairy Reaf
  semi_blue_gp,                 //60-47 Semi Blue
  pink_candy_gp,                //61-48 Pink Candy
  red_reaf_gp,                  //62-49 Red Reaf
  aqua_flash_gp,                //63-50 Aqua Flash
  yelblu_hot_gp,                //64-51 Yelblu Hot
  lite_light_gp,                //65-52 Lite Light
  red_flash_gp,                 //66-53 Red Flash
  blink_red_gp,                 //67-54 Blink Red
  red_shift_gp,                 //68-55 Red Shift
  red_tide_gp,                  //69-56 Red Tide
  candy2_gp,                    //70-57 Candy2
  trafficlight_gp               //71-58 Traffic Light
};

#endif
