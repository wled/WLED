#ifndef PalettesWLED_h
#define PalettesWLED_h

/*
 * Color palettes for FastLED effects (65-73).
 */

// From ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// Unfortunately, these are stored in RAM!

// Gradient palette "ib_jul01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ing/xmas/ib_jul01.c3g
const uint8_t ib_jul01_gp[] PROGMEM = {
    0, 230,   6,  17,
   94,  37,  96,  90,
  132, 144, 189, 106,
  255, 187,   3,  13};

// Gradient palette "es_vintage_57_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/vintage/es_vintage_57.c3g
const uint8_t es_vintage_57_gp[] PROGMEM = {
    0,  41,   8,   5,
   53,  92,   1,   0,
  104, 155,  96,  36,
  153, 217, 191,  72,
  255, 132, 129,  52};

// Gradient palette "es_vintage_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/vintage/es_vintage_01.c3g
const uint8_t es_vintage_01_gp[] PROGMEM = {
    0,  54,  18,  32,
   51,  89,   0,  30,
   76, 176, 170,  48,
  101, 255, 189,  92,
  127, 153,  56,  50,
  153,  89,   0,  30,
  229,  54,  18,  32,
  255,  54,  18,  32};

// Gradient palette "es_rivendell_15_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/rivendell/es_rivendell_15.c3g
const uint8_t es_rivendell_15_gp[] PROGMEM = {
    0,  35,  69,  54,
  101,  88, 105,  82,
  165, 143, 140, 109,
  242, 208, 204, 175,
  255, 208, 204, 175};

// Gradient palette "rgi_15_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ds/rgi/rgi_15.c3g
const uint8_t rgi_15_gp[] PROGMEM = {
    0,  54,  14, 111,
   31, 142,  24,  86,
   63, 231,  34,  61,
   95, 146,  31,  88,
  127,  61,  29, 114,
  159, 124,  47, 113,
  191, 186,  66, 112,
  223, 143,  57, 116,
  255, 100,  48, 120};

// Gradient palette "retro2_16_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ma/retro2/retro2_16.c3g
const uint8_t retro2_16_gp[] PROGMEM = {
    0, 227, 191,  12,
  255, 132,  52,   2};

// Gradient palette "Analogous_1_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/red/Analogous_1.c3g
const uint8_t Analogous_1_gp[] PROGMEM = {
    0,  51,   0, 255,
   63, 102,   0, 255,
  127, 153,   0, 255,
  191, 204,   0, 128,
  255, 255,   0,   0};

// Gradient palette "es_pinksplash_08_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/pink_splash/es_pinksplash_08.c3g
const uint8_t es_pinksplash_08_gp[] PROGMEM = {
    0, 195,  63, 255,
  127, 231,   9,  97,
  175, 237, 205, 218,
  221, 212,  38, 184,
  255, 212,  38, 184};

// Gradient palette "es_ocean_breeze_036_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/ocean_breeze/es_ocean_breeze_036.c3g
const uint8_t es_ocean_breeze_036_gp[] PROGMEM = {
    0,  25,  48,  62,
   89,  38, 166, 183,
  153, 205, 233, 255,
  255,   0, 145, 162};

// Gradient palette "departure_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/mjf/departure.c3g
const uint8_t departure_gp[] PROGMEM = {
    0,  68,  34,   0,
   42, 102,  51,   0,
   63, 160, 108,  60,
   84, 218, 166, 120,
  106, 238, 212, 188,
  116, 255, 255, 255,
  138, 200, 255, 200,
  148, 100, 255, 100,
  170,   0, 255,   0,
  191,   0, 192,   0,
  212,   0, 128,   0,
  255,   0, 128,   0};

// Gradient palette "es_landscape_64_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/landscape/es_landscape_64.c3g
const uint8_t es_landscape_64_gp[] PROGMEM = {
    0,   0,   0,   0,
   37,  43,  89,  26,
   76,  87, 178,  53,
  127, 163, 235,   8,
  128, 195, 234, 130,
  130, 227, 233, 252,
  153, 205, 219, 234,
  204, 146, 179, 253,
  255,  39, 107, 228};

// Gradient palette "es_landscape_33_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/landscape/es_landscape_33.c3g
const uint8_t es_landscape_33_gp[] PROGMEM = {
    0,  19,  45,   0,
   19, 116,  86,   3,
   38, 214, 128,   7,
   63, 245, 197,  25,
   66, 124, 196, 156,
  255,   9,  39,  11};

// Gradient palette "rainbowsherbet_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ma/icecream/rainbowsherbet.c3g
const uint8_t rainbowsherbet_gp[] PROGMEM = {
    0, 255, 102,  51,
   43, 255, 140, 102,
   86, 255,  51, 102,
  127, 255, 153, 178,
  170, 255, 255, 250,
  209, 128, 255,  97,
  255, 169, 255, 148};

// Gradient palette "gr65_hult_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/hult/gr65_hult.c3g
const uint8_t gr65_hult_gp[] PROGMEM = {
    0, 252, 216, 252,
   48, 255, 192, 255,
   89, 241,  95, 243,
  160,  65, 153, 221,
  216,  34, 184, 182,
  255,  34, 184, 182};

// Gradient palette "gr64_hult_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/hult/gr64_hult.c3g
const uint8_t gr64_hult_gp[] PROGMEM = {
    0,  34, 184, 182,
   66,  14, 162, 160,
  104, 139, 137,  11,
  130, 188, 186,  30,
  150, 139, 137,  11,
  201,  10, 156, 154,
  239,   0, 128, 128,
  255,   0, 128, 128};

// Gradient palette "GMT_drywet_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/gmt/GMT_drywet.c3g
const uint8_t GMT_drywet_gp[] PROGMEM = {
    0, 134,  97,  42,
   42, 238, 199, 100,
   84, 180, 238, 135,
  127,  50, 238, 235,
  170,  12, 120, 238,
  212,  38,   1, 183,
  255,   8,  51, 113};

// Gradient palette "ib15_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ing/general/ib15.c3g
const uint8_t ib15_gp[] PROGMEM = {
    0, 187, 160, 205,
   72, 212, 158, 159,
   89, 236, 155, 113,
  107, 255,  95,  74,
  141, 201,  98, 121,
  255, 146, 101, 168};

// Gradient palette "Tertiary_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/vermillion/Tertiary_01.c3g
const uint8_t Tertiary_01_gp[] PROGMEM = {
    0,   0,  25, 255,
   63,  51, 140, 128,
  127, 102, 255,   0,
  191, 178, 140,  26,
  255, 255,  25,  51};

// Gradient palette "lava_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/neota/elem/lava.c3g
const uint8_t lava_gp[] PROGMEM = {
    0,   0,   0,   0,
   46,  93,   0,   0,
   96, 187,   0,   0,
  108, 204,  38,  13,
  119, 221,  76,  26,
  146, 238, 115,  38,
  174, 255, 153,  51,
  188, 255, 178,  51,
  202, 255, 204,  51,
  218, 255, 230,  51,
  234, 255, 255,  51,
  244, 255, 255, 153,
  255, 255, 255, 255};

// Gradient palette "fierce-ice_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/neota/elem/fierce-ice.c3g
const uint8_t fierce_ice_gp[] PROGMEM = {
    0,   0,   0,   0,
   59,   0,  51, 128,
  119,   0, 102, 255,
  149,  51, 153, 255,
  180, 102, 204, 255,
  217, 178, 230, 255,
  255, 255, 255, 255};

// Gradient palette "Colorfull_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Colorfull.c3g
const uint8_t Colorfull_gp[] PROGMEM = {
    0,  76, 155,  54,
   25, 111, 174,  89,
   60, 146, 193, 125,
   93, 166, 166, 136,
  106, 185, 138, 147,
  109, 193, 121, 148,
  113, 202, 104, 149,
  116, 229, 179, 174,
  124, 255, 255, 199,
  168, 178, 218, 209,
  255, 100, 182, 219};

// Gradient palette "Pink_Purple_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Pink_Purple.c3g
const uint8_t Pink_Purple_gp[] PROGMEM = {
    0,  95,  32, 121,
   25, 106,  40, 128,
   51, 117,  48, 135,
   76, 154, 135, 192,
  102, 190, 222, 249,
  109, 215, 236, 252,
  114, 240, 250, 255,
  122, 213, 200, 241,
  149, 187, 149, 226,
  183, 196, 130, 209,
  255, 206, 111, 191};

// Gradient palette "Sunset_Real_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Sunset_Real.c3g
const uint8_t Sunset_Real_gp[] PROGMEM = {
    0, 191,   0,   0,
   22, 223,  85,   0,
   51, 255, 170,   0,
   85, 217,  85,  89,
  135, 178,   0, 178,
  198,  89,   0, 195,
  255,   0,   0, 212};

// Gradient palette "Sunset_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Sunset_Yellow.c3g
const uint8_t Sunset_Yellow_gp[] PROGMEM = {
    0,  76, 135, 191,
   36, 143, 188, 178,
   87, 210, 241, 165,
  100, 232, 237, 151,
  107, 255, 232, 138,
  115, 252, 202, 141,
  120, 249, 172, 144,
  128, 252, 202, 141,
  180, 255, 232, 138,
  223, 255, 242, 131,
  255, 255, 252, 125};

// Gradient palette "Beech_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Beech.c3g
const uint8_t Beech_gp[] PROGMEM = {
    0, 255, 254, 238,
   12, 255, 254, 238,
   22, 255, 254, 238,
   26, 228, 224, 186,
   28, 201, 195, 135,
   28, 186, 255, 234,
   50, 138, 251, 238,
   71,  90, 246, 243,
   93,  45, 225, 231,
  120,   0, 204, 219,
  133,   8, 168, 186,
  136,  16, 132, 153,
  136,  65, 189, 217,
  208,  33, 159, 207,
  255,   0, 129, 197};

// Gradient palette "Another_Sunset_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Another_Sunset.c3g
const uint8_t Another_Sunset_gp[] PROGMEM = {
    0, 185, 121,  73,
   29, 142, 103,  71,
   68, 100,  84,  69,
   68, 249, 184,  66,
   97, 241, 204, 105,
  124, 234, 225, 144,
  178, 117, 125, 140,
  255,   0,  26, 136};

// Gradient palette "es_autumn_19_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/autumn/es_autumn_19.c3g
const uint8_t es_autumn_19_gp[] PROGMEM = {
    0, 106,  14,   8,
   51, 153,  41,  19,
   84, 190,  70,  24,
  104, 201, 202, 136,
  112, 187, 137,   5,
  122, 199, 200, 142,
  124, 201, 202, 135,
  135, 187, 137,   5,
  142, 202, 203, 129,
  163, 187,  68,  24,
  204, 142,  35,  17,
  249,  90,   5,   4,
  255,  90,   5,   4};

// Gradient palette "BlacK_Blue_Magenta_White_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Blue_Magenta_White.c3g
const uint8_t BlacK_Blue_Magenta_White_gp[] PROGMEM = {
    0,   0,   0,   0,
   42,   0,   0, 128,
   84,   0,   0, 255,
  127, 128,   0, 255,
  170, 255,   0, 255,
  212, 255, 128, 255,
  255, 255, 255, 255};

// Gradient palette "BlacK_Magenta_Red_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Magenta_Red.c3g
const uint8_t BlacK_Magenta_Red_gp[] PROGMEM = {
    0,   0,   0,   0,
   63, 128,   0, 128,
  127, 255,   0, 255,
  191, 255,   0, 128,
  255, 255,   0,   0};

// Gradient palette "BlacK_Red_Magenta_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Red_Magenta_Yellow.c3g
const uint8_t BlacK_Red_Magenta_Yellow_gp[] PROGMEM = {
    0,   0,   0,   0,
   42, 128,   0,   0,
   84, 255,   0,   0,
  127, 255,   0, 128,
  170, 255,   0, 255,
  212, 255, 128, 128,
  255, 255, 255,   0};

// Gradient palette "Blue_Cyan_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/Blue_Cyan_Yellow.c3g
const uint8_t Blue_Cyan_Yellow_gp[] PROGMEM = {
    0,   0,   0, 255,
   63,   0, 128, 255,
  127,   0, 255, 255,
  191, 128, 255, 128,
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
    0,  30,  92, 179,
   14,  23, 111, 193,
   28,  11, 142, 216,
   42,   4, 161, 230,
   56,  25, 181, 241,
   70,  51, 188, 207,
   84, 102, 204, 206,
   99, 153, 219, 184,
  113, 192, 229, 136,
  127, 204, 230,  75,
  141, 243, 240,  29,
  155, 254, 222,  39,
  170, 252, 199,   7,
  184, 248, 157,  14,
  198, 245, 114,  21,
  226, 219,  30,  38,
  240, 164,  38,  44,
  255, 164,  38,  44};

// Gradient palette "bhw1_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_01.c3g
const uint8_t retro_clown_gp[] PROGMEM = {
    0, 244, 168,  48,
  117, 230,  78,  92,
  255, 173,  54, 228};

// Gradient palette "bhw1_04_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_04.c3g
const uint8_t candy_gp[] PROGMEM = {
    0, 245, 242,  31,
   15, 244, 168,  48,
  142, 126,  21, 161,
  198,  90,  22, 160,
  255,   0,   0, 128};

// Gradient palette "bhw1_05_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_05.c3g
const uint8_t toxy_reaf_gp[] PROGMEM = {
    0,   5, 239, 137,
  255, 158,  35, 221};

// Gradient palette "bhw1_06_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_06.c3g
const uint8_t fairy_reaf_gp[] PROGMEM = {
    0, 225,  19, 194,
  160,  19, 225, 223,
  219, 210, 242, 227,
  255, 255, 255, 255};

// Gradient palette "bhw1_14_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_14.c3g
const uint8_t semi_blue_gp[] PROGMEM = {
    0,   0,   0,   0,
   12,  35,   4,  48,
   53,  70,   8,  96,
   80,  56,  48, 168,
  119,  43,  89, 239,
  145,  64,  59, 175,
  186,  86,  30, 110,
  233,  43,  15,  55,
  255,   0,   0,   0};

// Gradient palette "bhw1_three_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_three.c3g
const uint8_t pink_candy_gp[] PROGMEM = {
    0, 255, 255, 255,
   45,  64,  64, 255,
  112, 244,  16, 193,
  140, 255, 255, 255,
  155, 244,  16, 193,
  196, 131,  13, 175,
  255, 255, 255, 255};

// Gradient palette "bhw1_w00t_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_w00t.c3g
const uint8_t red_reaf_gp[] PROGMEM = {
    0,  49,  68, 126,
  104, 162, 195, 249,
  188, 255,   0,   0,
  255, 110,  14,  14};

// Gradient palette "bhw2_23_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_23.c3g
const uint8_t aqua_flash_gp[] PROGMEM = {
    0,   0,   0,   0,
   66, 144, 242, 246,
   96, 255, 255,  64,
  124, 255, 255, 255,
  153, 255, 255,  64,
  188, 144, 242, 246,
  255,   0,   0,   0};

// Gradient palette "bhw2_xc_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_xc.c3g
const uint8_t yelblu_hot_gp[] PROGMEM = {
    0,  56,  30,  68,
   58,  89,   0, 130,
  122, 103,   0,  86,
  158, 205,  57,  29,
  183, 223, 117,  35,
  219, 241, 177,  41,
  255, 247, 247,  35};

// Gradient palette "bhw2_45_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_45.c3g
const uint8_t lite_light_gp[] PROGMEM = {
    0,   0,   0,   0,
    9,  30,  21,  30,
   40,  60,  43,  60,
   66,  60,  43,  60,
  101,  76,  16,  77,
  255,   0,   0,   0};

// Gradient palette "bhw2_22_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_22.c3g
const uint8_t red_flash_gp[] PROGMEM = {
    0,   0,   0,   0,
   99, 244,  12,  12,
  130, 253, 228, 172,
  155, 244,  12,  12,
  255,   0,   0,   0};

// Gradient palette "bhw3_40_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw3/bhw3_40.c3g
const uint8_t blink_red_gp[] PROGMEM = {
    0,   7,   7,   7,
   43,  53,  25,  73,
   76,  76,  15,  46,
  109, 214,  39, 108,
  127, 255, 156, 191,
  165, 194,  73, 212,
  204, 120,  66, 242,
  255,  93,  29,  90};

// Gradient palette "bhw3_52_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw3/bhw3_52.c3g
const uint8_t red_shift_gp[] PROGMEM = {
    0, 114,  22, 105,
   45, 118,  22,  85,
   99, 201,  45,  67,
  132, 238, 187,  70,
  175, 232,  85,  34,
  201, 232,  56,  59,
  255,   5,   0,   4};

// Gradient palette "bhw4_097_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw4/bhw4_097.c3g
const uint8_t red_tide_gp[] PROGMEM = {
    0, 252,  46,   0,
   28, 255, 139,  33,
   43, 247, 158,  74,
   58, 247, 216, 134,
   84, 245,  94,  15,
  114, 187,  65,  16,
  140, 255, 241, 127,
  168, 187,  65,  16,
  196, 251, 233, 167,
  216, 255,  94,   9,
  255, 140,   8,   6};

// Gradient palette "bhw4_017_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw4/bhw4_017.c3g
const uint8_t candy2_gp[] PROGMEM = {
    0, 124, 102, 114,
   25,  55,  49,  83,
   48, 136,  96,  96,
   73, 243, 214,  34,
   89, 222, 104,  54,
  130,  55,  49,  83,
  163, 255, 177,  58,
  186, 243, 214,  34,
  211, 124, 102, 114,
  255,  29,  19,  18};

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

// array of fastled palettes (palette 6 - 12)
const TProgmemRGBPalette16 *const fastledPalettes[] PROGMEM = {
  &PartyColors_p,               //06-00 Party
  &CloudColors_p,               //07-01 Cloud
  &LavaColors_p,                //08-02 Lava
  &OceanColors_p,               //09-03 Ocean
  &ForestColors_p,              //10-04 Forest
  &RainbowColors_p,             //11-05 Rainbow
  &RainbowStripeColors_p        //12-06 Rainbow Bands
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
