/*
 * Color palettes for FastLED effects (65-73).
 */

// From ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// Unfortunately, these are stored in RAM!

// Gradient palette "ib_jul01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ing/xmas/tn/ib_jul01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

#ifndef PalettesWLED_h
#define PalettesWLED_h

// Redefine FastLED's gradient palette declaration:
#define DEFINE_PALETTE(X) const byte X[] PROGMEM =

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// Gradient palette "ib_jul01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ing/xmas/tn/ib_jul01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

DEFINE_PALETTE(ib_jul01_gp) {
    0, 194,  1,  1,
   94,   1, 29, 18,
  132,  57,131, 28,
  255, 113,  1,  1};

// Gradient palette "es_vintage_57_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/vintage/tn/es_vintage_57.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_vintage_57_gp ) {
    0,   2,  1,  1,
   53,  18,  1,  0,
  104,  69, 29,  1,
  153, 167,135, 10,
  255,  46, 56,  4};

// Gradient palette "es_vintage_01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/vintage/tn/es_vintage_01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_PALETTE( es_vintage_01_gp ) {
    0,   4,  1,  1,
   51,  16,  0,  1,
   76,  97,104,  3,
  101, 255,131, 19,
  127,  67,  9,  4,
  153,  16,  0,  1,
  229,   4,  1,  1,
  255,   4,  1,  1};

// Gradient palette "es_rivendell_15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/rivendell/tn/es_rivendell_15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_rivendell_15_gp ) {
    0,   1, 14,  5,
  101,  16, 36, 14,
  165,  56, 68, 30,
  242, 150,156, 99,
  255, 150,156, 99};

// Gradient palette "rgi_15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ds/rgi/tn/rgi_15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( rgi_15_gp ) {
    0,   4,  1, 31,
   31,  55,  1, 16,
   63, 197,  3,  7,
   95,  59,  2, 17,
  127,   6,  2, 34,
  159,  39,  6, 33,
  191, 112, 13, 32,
  223,  56,  9, 35,
  255,  22,  6, 38};

// Gradient palette "retro2_16_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ma/retro2/tn/retro2_16.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 8 bytes of program space.

DEFINE_PALETTE( retro2_16_gp ) {
    0, 188,135,  1,
  255,  46,  7,  1};

// Gradient palette "Analogous_1_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/red/tn/Analogous_1.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Analogous_1_gp ) {
    0,   3,  0,255,
   63,  23,  0,255,
  127,  67,  0,255,
  191, 142,  0, 45,
  255, 255,  0,  0};

// Gradient palette "es_pinksplash_08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/pink_splash/tn/es_pinksplash_08.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_pinksplash_08_gp ) {
    0, 126, 11,255,
  127, 197,  1, 22,
  175, 210,157,172,
  221, 157,  3,112,
  255, 157,  3,112};

// Gradient palette "es_pinksplash_07_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/pink_splash/tn/es_pinksplash_07.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( es_pinksplash_07_gp ) {
    0, 229,  1,  1,
   61, 242,  4, 63,
  101, 255, 12,255,
  127, 249, 81,252,
  153, 255, 11,235,
  193, 244,  5, 68,
  255, 232,  1,  5};

// Gradient palette "Coral_reef_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/other/tn/Coral_reef.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( Coral_reef_gp ) {
    0,  40,199,197,
   50,  10,152,155,
   96,   1,111,120,
   96,  43,127,162,
  139,  10, 73,111,
  255,   1, 34, 71};

// Gradient palette "es_ocean_breeze_068_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_068.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_ocean_breeze_068_gp ) {
    0, 100,156,153,
   51,   1, 99,137,
  101,   1, 68, 84,
  104,  35,142,168,
  178,   0, 63,117,
  255,   1, 10, 10};

// Gradient palette "es_ocean_breeze_036_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_036.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

DEFINE_PALETTE( es_ocean_breeze_036_gp ) {
    0,   1,  6,  7,
   89,   1, 99,111,
  153, 144,209,255,
  255,   0, 73, 82};

// Gradient palette "departure_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/mjf/tn/departure.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 88 bytes of program space.

DEFINE_PALETTE( departure_gp ) {
    0,   8,  3,  0,
   42,  23,  7,  0,
   63,  75, 38,  6,
   84, 169, 99, 38,
  106, 213,169,119,
  116, 255,255,255,
  138, 135,255,138,
  148,  22,255, 24,
  170,   0,255,  0,
  191,   0,136,  0,
  212,   0, 55,  0,
  255,   0, 55,  0};

// Gradient palette "es_landscape_64_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_64.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( es_landscape_64_gp ) {
    0,   0,  0,  0,
   37,   2, 25,  1,
   76,  15,115,  5,
  127,  79,213,  1,
  128, 126,211, 47,
  130, 188,209,247,
  153, 144,182,205,
  204,  59,117,250,
  255,   1, 37,192};

// Gradient palette "es_landscape_33_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_33.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_landscape_33_gp ) {
    0,   1,  5,  0,
   19,  32, 23,  1,
   38, 161, 55,  1,
   63, 229,144,  1,
   66,  39,142, 74,
  255,   1,  4,  1};

// Gradient palette "rainbowsherbet_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ma/icecream/tn/rainbowsherbet.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( rainbowsherbet_gp ) {
    0, 255, 33,  4,
   43, 255, 68, 25,
   86, 255,  7, 25,
  127, 255, 82,103,
  170, 255,255,242,
  209,  42,255, 22,
  255,  87,255, 65};

// Gradient palette "gr65_hult_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/hult/tn/gr65_hult.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( gr65_hult_gp ) {
    0, 247,176,247,
   48, 255,136,255,
   89, 220, 29,226,
  160,   7, 82,178,
  216,   1,124,109,
  255,   1,124,109};

// Gradient palette "gr64_hult_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/hult/tn/gr64_hult.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_PALETTE( gr64_hult_gp ) {
    0,   1,124,109,
   66,   1, 93, 79,
  104,  52, 65,  1,
  130, 115,127,  1,
  150,  52, 65,  1,
  201,   1, 86, 72,
  239,   0, 55, 45,
  255,   0, 55, 45};

// Gradient palette "GMT_drywet_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gmt/tn/GMT_drywet.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( GMT_drywet_gp ) {
    0,  47, 30,  2,
   42, 213,147, 24,
   84, 103,219, 52,
  127,   3,219,207,
  170,   1, 48,214,
  212,   1,  1,111,
  255,   1,  7, 33};

// Gradient palette "ib15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ing/general/tn/ib15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( ib15_gp ) {
    0, 113, 91,147,
   72, 157, 88, 78,
   89, 208, 85, 33,
  107, 255, 29, 11,
  141, 137, 31, 39,
  255,  59, 33, 89};

// Gradient palette "Fuschia_7_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ds/fuschia/tn/Fuschia-7.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Fuschia_7_gp ) {
    0,  43,  3,153,
   63, 100,  4,103,
  127, 188,  5, 66,
  191, 161, 11,115,
  255, 135, 20,182};

// Gradient palette "es_emerald_dragon_08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/emerald_dragon/tn/es_emerald_dragon_08.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

DEFINE_PALETTE( es_emerald_dragon_08_gp ) {
    0,  97,255,  1,
  101,  47,133,  1,
  178,  13, 43,  1,
  255,   2, 10,  1};

// Gradient palette "lava_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/lava.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_PALETTE( lava_gp ) {
    0,   0,  0,  0,
   46,  18,  0,  0,
   96, 113,  0,  0,
  108, 142,  3,  1,
  119, 175, 17,  1,
  146, 213, 44,  2,
  174, 255, 82,  4,
  188, 255,115,  4,
  202, 255,156,  4,
  218, 255,203,  4,
  234, 255,255,  4,
  244, 255,255, 71,
  255, 255,255,255};

// Gradient palette "fire_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/fire.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( fire_gp ) {
    0,   1,  1,  0,
   76,  32,  5,  0,
  146, 192, 24,  0,
  197, 220,105,  5,
  240, 252,255, 31,
  250, 252,255,111,
  255, 255,255,255};

// Gradient palette "Colorfull_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Colorfull.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_PALETTE( Colorfull_gp ) {
    0,  10, 85,  5,
   25,  29,109, 18,
   60,  59,138, 42,
   93,  83, 99, 52,
  106, 110, 66, 64,
  109, 123, 49, 65,
  113, 139, 35, 66,
  116, 192,117, 98,
  124, 255,255,137,
  168, 100,180,155,
  255,  22,121,174};

// Gradient palette "Magenta_Evening_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Magenta_Evening.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( Magenta_Evening_gp ) {
    0,  71, 27, 39,
   31, 130, 11, 51,
   63, 213,  2, 64,
   70, 232,  1, 66,
   76, 252,  1, 69,
  108, 123,  2, 51,
  255,  46,  9, 35};

// Gradient palette "Pink_Purple_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Pink_Purple.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_PALETTE( Pink_Purple_gp ) {
    0,  19,  2, 39,
   25,  26,  4, 45,
   51,  33,  6, 52,
   76,  68, 62,125,
  102, 118,187,240,
  109, 163,215,247,
  114, 217,244,255,
  122, 159,149,221,
  149, 113, 78,188,
  183, 128, 57,155,
  255, 146, 40,123};

// Gradient palette "Sunset_Real_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Sunset_Real.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( Sunset_Real_gp ) {
    0, 120,  0,  0,
   22, 179, 22,  0,
   51, 255,104,  0,
   85, 167, 22, 18,
  135, 100,  0,103,
  198,  16,  0,130,
  255,   0,  0,160};

// Gradient palette "es_autumn_19_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/autumn/tn/es_autumn_19.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_PALETTE( es_autumn_19_gp ) {
    0,  26,  1,  1,
   51,  67,  4,  1,
   84, 118, 14,  1,
  104, 137,152, 52,
  112, 113, 65,  1,
  122, 133,149, 59,
  124, 137,152, 52,
  135, 113, 65,  1,
  142, 139,154, 46,
  163, 113, 13,  1,
  204,  55,  3,  1,
  249,  17,  1,  1,
  255,  17,  1,  1};

// Gradient palette "BlacK_Blue_Magenta_White_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/BlacK_Blue_Magenta_White.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( BlacK_Blue_Magenta_White_gp ) {
    0,   0,  0,  0,
   42,   0,  0, 45,
   84,   0,  0,255,
  127,  42,  0,255,
  170, 255,  0,255,
  212, 255, 55,255,
  255, 255,255,255};

// Gradient palette "BlacK_Magenta_Red_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/BlacK_Magenta_Red.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( BlacK_Magenta_Red_gp ) {
    0,   0,  0,  0,
   63,  42,  0, 45,
  127, 255,  0,255,
  191, 255,  0, 45,
  255, 255,  0,  0};

// Gradient palette "BlacK_Red_Magenta_Yellow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/BlacK_Red_Magenta_Yellow.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( BlacK_Red_Magenta_Yellow_gp ) {
    0,   0,  0,  0,
   42,  42,  0,  0,
   84, 255,  0,  0,
  127, 255,  0, 45,
  170, 255,  0,255,
  212, 255, 55, 45,
  255, 255,255,  0};

// Gradient palette "Blue_Cyan_Yellow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/Blue_Cyan_Yellow.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Blue_Cyan_Yellow_gp ) {
    0,   0,  0,255,
   63,   0, 55,255,
  127,   0,255,255,
  191,  42,255, 45,
  255, 255,255,  0};

// Gradient palette "Sunset_Yellow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Sunset_Yellow.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_PALETTE( Sunset_Yellow_gp ) {
    0,  10, 62,123,
   36,  56,130,103,
   87, 153,225, 85,
  100, 199,217, 68,
  107, 255,207, 54,
  115, 247,152, 57,
  120, 239,107, 61,
  128, 247,152, 57,
  180, 255,207, 54,
  223, 255,227, 48,
  255, 255,248, 42};


// Gradient palette "cloud_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rc/tn/cloud.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 12 bytes of program space.

DEFINE_PALETTE( cloud_gp ) {
    0, 247,149, 91,
  127, 208, 32, 71,
  255,  42, 79,188};


// Gradient palette "fireandice_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rc/tn/fireandice.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( fireandice_gp ) {
    0,  80,  2,  1,
   51, 206, 15,  1,
  101, 242, 34,  1,
  153,  16, 67,128,
  204,   2, 21, 69,
  255,   1,  2,  4};

// Gradient palette "bhw2_39_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/bhw/bhw2/tn/bhw2_39.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( bhw2_39_gp ) {
    0,   2,184,188,
   33,  56, 27,162,
   66,  56, 27,162,
  122, 255,255, 45,
  150, 227, 65,  6,
  201,  67, 13, 27,
  255,  16,  1, 53};

// Gradient palette "tashangel_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rc/tn/tashangel.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( tashangel_gp ) {
    0, 133, 68,197,
   51,   2,  1, 33,
  101,  50, 35,130,
  153, 199,225,237,
  204,  41,187,228,
  255, 133, 68,197};

// Gradient palette "butterflytalker_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rc/tn/butterflytalker.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( butterflytalker_gp ) {
    0,   1,  1,  6,
   51,   6, 11, 52,
   89, 107,107,192,
  127, 101,161,192,
  165, 107,107,192,
  204,   6, 11, 52,
  255,   0,  0,  0};

// Gradient palette "os250k_metres_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/os/tn/os250k-metres.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( os250k_metres_gp ) {
    0, 255,255,255,
   11, 255,255,255,
   11, 255,252,214,
   34, 255,252,214,
   34, 255,248,178,
   57, 255,248,178,
   57, 255,211,130,
   81, 255,211,130,
   81, 255,176, 89,
  115, 255,176, 89,
  115, 255,147, 63,
  173, 255,147, 63,
  173, 255,127, 55,
  255, 255,127, 55};

// Gradient palette "Night_Midnight_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Night_Midnight.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( Night_Midnight_gp ) {
    0,  15, 25, 27,
   36,  22, 48, 91,
   59,  32, 80,203,
   74, 110,154,228,
   77, 255,255,255,
   82, 110,154,228,
   96,  32, 80,203,
  189,   5, 18, 73,
  255,   0,  1, 12};

// Gradient palette "Afterdusk_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Afterdusk.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_PALETTE( Afterdusk_gp ) {
    0,   0,  0,  0,
   25,   1,  1,  1,
   48,   1,  1,  1,
   67,  41, 49, 52,
   70, 210,219,216,
   73, 155,115,137,
   81, 109, 46, 78,
   86, 109, 46, 78,
   97, 109, 46, 78,
  165,  50, 15, 79,
  255,  16,  1, 80};

// Gradient palette "BlueSky_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/BlueSky.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( BlueSky_gp ) {
    0,   1,  7, 39,
   25,   2, 25, 88,
   61,   9, 53,160,
   88,  46,115,201,
  102, 120,203,245,
  108,  88,169,230,
  124,  63,139,216,
  216,  21, 96,203,
  255,   2, 60,188};

// Gradient palette "Gold_Orange_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Gold_Orange.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_PALETTE( Gold_Orange_gp ) {
    0, 244, 88, 11,
   21, 247,118, 26,
   40, 249,152, 50,
   62, 252,201, 82,
   72, 255,255,125,
   79, 255,211,119,
   83, 255,169,112,
   87, 255,211,119,
   94, 255,255,125,
  103, 244,207, 54,
  118, 237,164, 16,
  202, 242,124, 13,
  255, 244, 88, 11};

// Gradient palette "Analogous_02_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/vermillion/tn/Analogous_02.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Analogous_02_gp ) {
    0,  32,  0,123,
   63, 110,  5, 79,
  127, 255, 23, 45,
  191, 255, 21, 30,
  255, 255, 18, 18};

// Gradient palette "Analogous_04a_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/pink/tn/Analogous_04a.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( Analogous_04a_gp ) {
    0,  67, 55,255,
   42,  67, 55,255,
   84,  67, 55,255,
   84, 120, 33,255,
  127, 120, 33,255,
  170, 120, 33,255,
  170, 255, 23, 45,
  212, 255, 23, 45,
  255, 255, 23, 45};

// Gradient palette "Cyan_Orange_Stripped_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/other/tn/Cyan_Orange_Stripped.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 48 bytes of program space.

DEFINE_PALETTE( Cyan_Orange_Stripped_gp ) {
    0,   1,108,212,
   60,   1,108,212,
  121,   1,108,212,
  121,   0,  0,  0,
  124,   0,  0,  0,
  127,   0,  0,  0,
  127, 229,127, 15,
  188, 242,186, 92,
  248, 255,255,255,
  248,   0,  0,  0,
  251,   0,  0,  0,
  255,   0,  0,  0};

// Gradient palette "Cyan_White_Green_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/Cyan_White_Green.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Cyan_White_Green_gp ) {
    0,   0,255,255,
   63,  42,255,255,
  127, 255,255,255,
  191,  42,255, 45,
  255,   0,255,  0};

// Gradient palette "Wild_Orange_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/lb/mp/tn/Wild_Orange.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( Wild_Orange_gp ) {
    0,   0,  0,  0,
    0, 144, 11,  1,
    0, 144, 11,  1,
    5, 144, 11,  1,
   10, 194, 36,  1,
   30, 252, 79,  1,
   86, 249,175,100,
  106, 244,122, 25,
  124, 237, 79,  1,
  157, 244,154,  2,
  196, 252,255,  5,
  209, 252,223,  3,
  239, 255,108,  1,
  255, 255, 36,  1};

// Gradient palette "IKat_Radial_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/lb/mp/tn/IKat_Radial.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( IKat_Radial_gp ) {
    0,   3,  7,  4,
   56, 255,255,255,
  127,   3,  7,  4,
  196, 255,255,255,
  255,   3,  7,  4};
  
// Gradient palette "Citrus_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/lb/misc/tn/Citrus.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Citrus_gp ) {
    0, 252,164,  5,
   63, 149,25,  3,
  135, 255,166,  9,
  201, 147,39,  3,
  255, 237,119,  4};

// Gradient palette "Teal_Blue_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/lb/misc/tn/Teal_Blue.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Teal_Blue_gp ) {
    0,   1, 73, 88,
   63,   1, 43, 52,
  127,   1, 77, 95,
  196,   1, 58, 67,
  255,   1, 45, 50};

// Gradient palette "Ldby_Orange_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/lb/misc/tn/Ldby_Orange.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Ldby_Orange_gp ) {
    0, 217, 45, 17,
   61, 179, 21,  8,
  130, 222, 49, 21,
  193, 203, 32,  7,
  255, 173, 22,  6};

// Gradient palette "purple_orange_d07_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/km/tn/purple-orange-d07.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( purple_orange_d07_gp ) {
    0,  53, 27, 91,
   36,  53, 27, 91,
   36, 121, 55,111,
   72, 121, 55,111,
   72, 179,107,137,
  109, 179,107,137,
  109, 179,189,182,
  145, 179,189,182,
  145, 234,152, 59,
  182, 234,152, 59,
  182, 227, 92, 11,
  218, 227, 92, 11,
  218, 165, 40,  1,
  255, 165, 40,  1};

// Gradient palette "blue_tan_d08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/km/tn/blue-tan-d08.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 64 bytes of program space.

DEFINE_PALETTE( blue_tan_d08_gp ) {
    0,   7, 77,210,
   31,   7, 77,210,
   31,  21,112,216,
   63,  21,112,216,
   63,  53,149,207,
   95,  53,149,207,
   95, 123,180,192,
  127, 123,180,192,
  127, 186,186,127,
  159, 186,186,127,
  159, 182,159, 50,
  191, 182,159, 50,
  191, 155,117, 14,
  223, 155,117, 14,
  223, 115, 72,  2,
  255, 115, 72,  2};

// Gradient palette "green_purple_d07_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/km/tn/green-purple-d07.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( green_purple_d07_gp ) {
    0,   1, 90, 12,
   36,   1, 90, 12,
   36,  12,147, 51,
   72,  12,147, 51,
   72,  56,189,120,
  109,  56,189,120,
  109, 179,189,182,
  145, 179,189,182,
  145, 179,107,137,
  182, 179,107,137,
  182, 121, 55,111,
  218, 121, 55,111,
  218,  53, 27, 91,
  255,  53, 27, 91};

// Gradient palette "knoza_00_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/knoza/tn/knoza-00.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( knoza_00_gp ) {
    0,  56, 56,237,
    1, 115,  1,  1,
   24, 115,  1,  1,
   25, 237,130, 46,
  101, 237,186,  1,
  113, 237,186,  1,
  115,   2,  1,  1,
  138,   2,  1,  1,
  139, 237,186,  1,
  153, 237,186,  1,
  228, 237,130, 46,
  229, 115,  1,  1,
  253, 115,  1,  1,
  255,  56, 56,237};
// Gradient palette "knoza_18_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/knoza/tn/knoza-18.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 64 bytes of program space.

DEFINE_PALETTE( knoza_18_gp ) {
    0,   8,  1,  1,
    2,   1,239,  1,
   51,   1,239,  1,
   52, 175,130,  1,
  100, 175,130,  1,
  101,   1,  1,  1,
  115,   1,  1,  1,
  117, 237,239,237,
  138, 237,239,237,
  139,   1,  1,  1,
  153,   1,  1,  1,
  153, 175,130,  1,
  203, 175,130,  1,
  203,   1,239,  1,
  252,   1,239,  1,
  255,   8,  1,  1};

// Gradient palette "calpan_18_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/calpan/tn/calpan-18.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 40 bytes of program space.

DEFINE_PALETTE( calpan_18_gp ) {
    0, 133, 31,137,
    1, 117,  2, 88,
   24, 117,  2, 88,
   25, 239,241,245,
   32, 239,241,245,
   51, 239,241,245,
   53, 117,  2, 88,
   76, 117,  2, 88,
   77, 133, 31,137,
  255, 239,241,245};

// Gradient palette "calbayo_18_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/calbayo/tn/calbayo-18.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 64 bytes of program space.

DEFINE_PALETTE( calbayo_18_gp ) {
    0, 210,131,  1,
   60, 210,131,  1,
   62,  41,  2,  3,
   99,  41,  2,  3,
  100, 106, 40,  1,
  101, 210,131,  1,
  126, 210,131,  1,
  127, 210, 31,  6,
  165, 210, 31,  6,
  166, 210,131,  1,
  188, 210,131,  1,
  191,   3,  6,  6,
  226,   3,  6,  6,
  228, 210,131,  1,
  253, 210,131,  1,
  255,   1, 58, 29};

// Gradient palette "fib53_15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/fib53/tn/fib53-15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 60 bytes of program space.

DEFINE_PALETTE( fib53_15_gp ) {
    0, 239, 11, 31,
  101, 239, 11, 31,
  101, 239,241,240,
  127, 239,241,240,
  128,   1,  1,  1,
  152,   1,  1,  1,
  153, 239,241,240,
  178, 239,241,240,
  179, 239, 11, 31,
  202, 239, 11, 31,
  203, 239,241,240,
  229, 239,241,240,
  230,   1,  1,  1,
  253,   1,  1,  1,
  255, 239,241,240};

// Gradient palette "purple_orange_d08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/km/tn/purple-orange-d08.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 64 bytes of program space.

DEFINE_PALETTE( purple_orange_d08_gp ) {
    0,  49, 26, 89,
   31,  49, 26, 89,
   31, 107, 49,106,
   63, 107, 49,106,
   63, 165, 88,127,
   95, 165, 88,127,
   95, 188,151,158,
  127, 188,151,158,
  127, 210,178,117,
  159, 210,178,117,
  159, 239,135, 37,
  191, 239,135, 37,
  191, 220, 81,  7,
  223, 220, 81,  7,
  223, 159, 37,  1,
  255, 159, 37,  1};

// Gradient palette "pmh_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/esdb/tn/pmh.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 48 bytes of program space.

DEFINE_PALETTE( pmh_gp ) {
    0, 255, 55,145,
   42, 255, 55,145,
   42, 255,182,145,
   84, 255,182,145,
   84, 255,255,105,
  127, 255,255,105,
  127, 171,255,174,
  170, 171,255,174,
  170, 101,255,212,
  212, 101,255,212,
  212, 171, 82,212,
  255, 171, 82,212};

// Gradient palette "konjo_08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/konjo/tn/konjo-08.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( konjo_08_gp ) {
    0, 213,229,240,
  127, 213,229,240,
  128, 133,168,188,
  150, 133,168,188,
  150,  21, 57, 91,
  152,   0,  6, 33,
  177,   0,  6, 33,
  179,   0,  2,  9,
  200,   0,  2,  9,
  203,   0,  6, 33,
  227,   0,  6, 33,
  229,  30,  0,  2,
  252,  30,  0,  2,
  255,   0,  6, 33};

// Gradient palette "konjo_18_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/konjo/tn/konjo-18.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 88 bytes of program space.

DEFINE_PALETTE( konjo_18_gp ) {
    0, 109,  5,  1,
   13, 109,  5,  1,
   14, 133,168,188,
   37, 133,168,188,
   39,   0,  6, 33,
   63,   0,  6, 33,
   63,  77,130,184,
   87,  77,130,184,
   89,   0,  2,  9,
  114,   0,  2,  9,
  115, 213,229,240,
  140, 213,229,240,
  142,   0,  2,  9,
  165,   0,  2,  9,
  166,  77,130,184,
  191,  77,130,184,
  193,   0,  6, 33,
  216,   0,  6, 33,
  217, 133,168,188,
  240, 133,168,188,
  241, 109,  5,  1,
  255, 109,  5,  1};

// Gradient palette "konjo_19_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/konjo/tn/konjo-19.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 88 bytes of program space.

DEFINE_PALETTE( konjo_19_gp ) {
    0, 109,  5,  1,
   13, 109,  5,  1,
   14, 133,168,188,
   37, 133,168,188,
   39,   0,  6, 33,
   63,   0,  6, 33,
   65, 109,  5,  1,
   87, 109,  5,  1,
   89,   0,  2,  9,
  114,   0,  2,  9,
  115, 213,229,240,
  140, 213,229,240,
  142,   0,  2,  9,
  165,   0,  2,  9,
  166, 109,  5,  1,
  192, 109,  5,  1,
  193,   0,  6, 33,
  216,   0,  6, 33,
  217, 133,168,188,
  240, 133,168,188,
  241, 109,  5,  1,
  255, 109,  5,  1};

// Gradient palette "konkikyo_19_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/konkikyo/tn/konkikyo-19.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 60 bytes of program space.

DEFINE_PALETTE( konkikyo_19_gp ) {
    0,   1,  2,  9,
  101,   1,  2,  9,
  102, 199,213,252,
  122, 199,213,252,
  126,   1,  2,  9,
  151,   1,  2,  9,
  151,  24,128,245,
  177,  24,128,245,
  178,   1,  2,  9,
  203,   1,  2,  9,
  203, 177,133,  1,
  229, 177,133,  1,
  229,   1,  2,  9,
  252,   1,  2,  9,
  255,   1,  2,  9};

// Gradient palette "sulz_22_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/sulz/tn/sulz-22.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 96 bytes of program space.

DEFINE_PALETTE( sulz_22_gp ) {
    0, 247, 54,  7,
    1, 247, 54,  7,
   24, 247, 54,  7,
   25,   1,  1,  1,
   51,   1,  1,  1,
   51, 247,248,247,
   75, 247,248,247,
   75, 247, 54,  7,
  101, 247, 54,  7,
  102,   1,  1,  1,
  115,   1,  1,  1,
  115, 247, 54,  7,
  138, 247, 54,  7,
  139,   1,  1,  1,
  153,   1,  1,  1,
  153, 247, 54,  7,
  179, 247, 54,  7,
  181, 247,248,247,
  202, 247,248,247,
  203,   1,  1,  1,
  228,   1,  1,  1,
  228, 247, 54,  7,
  249, 247, 54,  7,
  255, 247, 54,  7};

// Gradient palette "Pills_2_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ds/icons/tn/Pills-2.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 12 bytes of program space.

DEFINE_PALETTE( Pills_2_gp ) {
    0, 192,147, 11,
  127, 148,104, 59,
  255, 109, 69,155};

// Gradient palette "Pink_Yellow_Orange_1_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ds/icons/tn/Pink-Yellow-Orange-1.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( Pink_Yellow_Orange_1_gp ) {
    0, 255,199,  0,
   34, 255,121,  0,
  106, 255, 63,  0,
  168, 194, 13,  6,
  255, 146,  1, 37};
// Gradient palette "es_autumn_04_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/autumn/tn/es_autumn_04.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_autumn_04_gp ) {
    0,   2,  1,  1,
  101,  27,  1,  0,
  165, 210, 22,  1,
  234, 255,166, 42,
  255, 255,166, 42};

// Gradient palette "es_autumn_02_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/autumn/tn/es_autumn_02.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_autumn_02_gp ) {
    0,  86,  6,  1,
  127, 255,255,125,
  153, 255,255,125,
  242, 194, 96,  1,
  255, 194, 96,  1};

// Gradient palette "es_candide_30_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/candide/tn/es_candide_30.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_candide_30_gp ) {
    0, 242,244,242,
   63, 133,255,137,
  127, 242,146,194,
  191, 104,187,245,
  252, 232,239,237,
  255, 232,239,237};

// Gradient palette "es_chic_16_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/chic/tn/es_chic_16.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_PALETTE( es_chic_16_gp ) {
    0,   4,  1,  1,
   51, 135, 99,  3,
   63, 222,248,160,
   76, 110,118, 50,
   89,  72, 55,  6,
  127,   4,  1,  1,
  165,  72, 55,  6,
  172,  90, 84, 22,
  178, 110,118, 50,
  191, 222,248,160,
  204, 135, 99,  3,
  247,   4,  1,  1,
  255,   4,  1,  1};

// Gradient palette "es_coffee_01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/coffee/tn/es_coffee_01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_PALETTE( es_coffee_01_gp ) {
    0, 152,173,123,
   13, 152,154,106,
   25, 150,136, 91,
   63, 133, 78, 35,
   86, 112, 46, 15,
  114,  86, 15,  1,
  153,  68,  6,  1,
  178,  46,  1,  1,
  191,  31,  1,  1,
  216,  14,  1,  0,
  255,   6,  1,  0};

// Gradient palette "es_emerald_dragon_01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/emerald_dragon/tn/es_emerald_dragon_01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_emerald_dragon_01_gp ) {
    0,   1,  1,  1,
   79,   1, 19,  7,
  130,   1, 59, 25,
  229,  28,255,255,
  255,  28,255,255};

// Gradient palette "es_landscape_57_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_57.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_landscape_57_gp ) {
    0,  27, 91,  0,
   89, 126,171,106,
   91, 157,199,255,
  143,  45,142,245,
  191,   3, 96,235,
  255,   1, 15, 22};
// Gradient palette "es_landscape_22_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_22.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_landscape_22_gp ) {
    0,   1,  6,  1,
   38,   7, 49,  1,
   63,  21,124,  1,
   68, 173,244,252,
  127,  10,164,156,
  255,   5, 68, 66};
// Gradient palette "es_landscape_47_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_47.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_landscape_47_gp ) {
    0, 175,125, 44,
   38,  88, 45,  3,
   58,  46, 27,  1,
   76,  20, 14,  0,
   79, 249,193,140,
  255, 121, 27,  1};

// Gradient palette "es_landscape_10_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_10.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_PALETTE( es_landscape_10_gp ) {
    0, 244,213, 55,
   24, 242,209, 53,
   51, 237,203, 51,
   63, 210,252,252,
   89, 171,225,230,
  127, 123,221,203,
  204,  25,122,144,
  255,  10, 93,115};

// Gradient palette "es_landscape_76_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_76.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_landscape_76_gp ) {
    0, 252,178, 82,
  127, 208, 91,  7,
  132, 153,173,188,
  191, 163,187,221,
  255, 130,191,250};

// Gradient palette "es_landscape_61_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_61.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( es_landscape_61_gp ) {
    0,  90,199,  1,
   89,  73,219,  6,
  127,  34,189,  6,
  128, 113,221, 75,
  130, 255,252,255,
  178,  64,189,255,
  255,   1,122,255};

// Gradient palette "es_landscape_60_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_60.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_PALETTE( es_landscape_60_gp ) {
    0, 161,112, 18,
   51, 130, 78,  1,
   89,  95, 59,  1,
   91, 133,151,140,
  136,  22, 92, 91,
  178,   1, 49, 52,
  242,   0,  1,  1,
  255,   0,  1,  1};

// Gradient palette "es_landscape_51_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_51.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_landscape_51_gp ) {
    0, 128,128,103,
   39, 165,161,144,
   76, 206,195,190,
  114,  15, 71,247,
  178,   1,  9, 71,
  255,   1,  1, 10};

// Gradient palette "es_landscape_06_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_06.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 12 bytes of program space.

DEFINE_PALETTE( es_landscape_06_gp ) {
    0,  90,199,  1,
   89, 173,244,252,
  255,  57,175,207};

// Gradient palette "es_ocean_breeze_049_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_049.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( es_ocean_breeze_049_gp ) {
    0, 184,231,250,
   76,   0,112,203,
   77,  29,168,228,
   79, 179,235,255,
  153,  64,189,255,
  255,   0,124,199};

// Gradient palette "es_ocean_breeze_057_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_057.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_PALETTE( es_ocean_breeze_057_gp ) {
    0, 115, 82, 49,
   76,  87, 51, 22,
   79, 249, 71,  9,
  101, 249,122, 17,
  140, 247,121, 38,
  178, 175,125, 71,
  229, 123,108, 83,
  255,  83, 97, 83};

// Gradient palette "es_ocean_breeze_074_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_074.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( es_ocean_breeze_074_gp ) {
    0,   1,  1,  1,
  101,  34, 23,  3,
  127,  53, 26,  2,
  130, 203, 65,  7,
  153,  78, 56,  8,
  191,  22, 37, 11,
  255,   1,  4,  1};

// Gradient palette "es_pinksplash_05_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/pink_splash/tn/es_pinksplash_05.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( es_pinksplash_05_gp ) {
    0, 206,  1, 25,
   20, 192, 45, 82,
   38, 179,182,182,
   76, 206,  1, 25,
  127, 255,135,252,
  178, 206,  1, 25,
  216, 179,182,182,
  231, 192, 45, 82,
  255, 206,  1, 25};

// Gradient palette "es_pinksplash_10_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/pink_splash/tn/es_pinksplash_10.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( es_pinksplash_10_gp ) {
    0,  26, 17, 27,
   63, 184,  1, 37,
   76, 234,141,174,
   89, 148,  2, 35,
  127,  26, 17, 27,
  252,  90, 65, 89,
  255,  90, 65, 89};

// Gradient palette "es_vintage_56_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/vintage/tn/es_vintage_56.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( es_vintage_56_gp ) {
    0, 220,225,221,
   51,  83, 79,  7,
  109,  25,  0,  1,
  119, 255,131, 19,
  127, 217,221,184,
  135, 255,131, 19,
  145,  25,  0,  1,
  204,  60, 46,  1,
  255, 220,225,221};

// Gradient palette "es_vintage_10_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/vintage/tn/es_vintage_10.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

DEFINE_PALETTE( es_vintage_10_gp ) {
    0,   1,  3,  1,
   51,   7,  1,  1,
  127, 112, 18,  0,
  255, 206,207,182};

// Gradient palette "gold_yellow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/clth/tn/gold-yellow.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( gold_yellow_gp ) {
    0,   0,  0,  0,
   94,  42, 29,  0,
  189, 255,135,  0,
  213, 255,189,  4,
  238, 255,255, 25,
  246, 255,255,103,
  255, 255,255,255};

// Gradient palette "radioactive_slime_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/faun/tn/radioactive-slime.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_PALETTE( radioactive_slime_gp ) {
    0,   0,  0,  0,
   25,   1,  4,  1,
   58,   1, 19,  1,
   76,   4, 30,  4,
  101,  17, 43, 13,
  118,  12, 69, 13,
  135,   8,100, 13,
  150,  27,146, 36,
  174,  59,199, 75,
  195, 135,195, 79,
  222, 255,189, 84,
  239, 255,221, 96,
  255, 255,255,111};

// Gradient palette "pastel_rainbow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/othr/tn/pastel-rainbow.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_PALETTE( pastel_rainbow_gp ) {
    0,   0,  0,  0,
   33,   1,  2,  8,
   67,   7, 12, 45,
   88,  27, 18, 31,
  110,  67, 27, 19,
  129,  83, 38, 52,
  147, 100, 53,103,
  168,  90, 96, 93,
  189,  79,156, 83,
  206, 110,178,132,
  222, 148,203,197,
  238, 197,227,223,
  255, 255,255,255};

// Gradient palette "purple_sunset_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/othr/tn/purple-sunset.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 60 bytes of program space.

DEFINE_PALETTE( purple_sunset_gp ) {
    0,   0,  0,  0,
   31,   1,  1,  1,
   62,   1,  1,  7,
   62,   3,  2,  6,
   63,   6,  4,  5,
   88,  16,  8,  9,
  114,  31, 14, 15,
  131,  45, 22, 22,
  148,  61, 31, 31,
  152,  65, 39, 37,
  155,  69, 48, 45,
  192, 118, 86, 46,
  225, 184,135, 47,
  238, 197,161, 72,
  255, 213,187,103};

// Gradient palette "rainfall_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/jjg/misc/tn/rainfall.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( rainfall_gp ) {
    0, 192,118,  3,
   36, 192,118,  3,
   36, 222,118, 24,
   72, 222,118, 24,
   72, 224,209, 37,
  109, 224,209, 37,
  109,  58,159, 43,
  145,  58,159, 43,
  145,   7,133, 52,
  182,   7,133, 52,
  182,   4,118, 50,
  218,   4,118, 50,
  218,   1, 85,  8,
  255,   1, 85,  8};

// Gradient palette "sulz_12_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/sulz/tn/sulz-12.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 96 bytes of program space.

DEFINE_PALETTE( sulz_12_gp ) {
    0, 247,229,247,
    2,   1,  3, 10,
   23,   1,  3, 10,
   23,  17, 38, 14,
   45,  17, 38, 14,
   46,   1,  3, 10,
   69,   1,  3, 10,
   70,  17, 38, 14,
   91,  17, 38, 14,
   92,   1,  3, 10,
  113,   1,  3, 10,
  115, 247,229,247,
  137, 247,229,247,
  140,   1,  3, 10,
  160,   1,  3, 10,
  160,  17, 38, 14,
  182,  17, 38, 14,
  183,   1,  3, 10,
  206,   1,  3, 10,
  206,  17, 38, 14,
  228,  17, 38, 14,
  229,   1,  3, 10,
  253,   1,  3, 10,
  255, 247,229,247};

// Gradient palette "sulz_10_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/sulz/tn/sulz-10.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 96 bytes of program space.

DEFINE_PALETTE( sulz_10_gp ) {
    0, 247,229,247,
    2,   1,  3, 10,
   23,   1,  3, 10,
   23, 117,  1,168,
   45, 117,  1,168,
   46,   1,  3, 10,
   69,   1,  3, 10,
   69, 117,  1,168,
   91, 117,  1,168,
   92,   1,  3, 10,
  113,   1,  3, 10,
  115, 247,229,247,
  137, 247,229,247,
  140,   1,  3, 10,
  160,   1,  3, 10,
  160, 117,  1,168,
  182, 117,  1,168,
  183,   1,  3, 10,
  206,   1,  3, 10,
  206, 117,  1,168,
  229, 117,  1,168,
  229,   1,  3, 10,
  253,   1,  3, 10,
  255, 247,229,247};

// Gradient palette "sulz_15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/sulz/tn/sulz-15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 96 bytes of program space.

DEFINE_PALETTE( sulz_15_gp ) {
    0, 247,229,247,
    2,   1,  3, 10,
   23,   1,  3, 10,
   23,  57,  1,  1,
   45,  57,  1,  1,
   46,   1,  3, 10,
   69,   1,  3, 10,
   69,  57,  1,  1,
   90,  57,  1,  1,
   92,   1,  3, 10,
  113,   1,  3, 10,
  115, 247,229,247,
  137, 247,229,247,
  140,   1,  3, 10,
  160,   1,  3, 10,
  160,  57,  1,  1,
  181,  57,  1,  1,
  183,   1,  3, 10,
  206,   1,  3, 10,
  207,  57,  1,  1,
  229,  57,  1,  1,
  229,   1,  3, 10,
  253,   1,  3, 10,
  255, 247,229,247};

// Gradient palette "sulz_21_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/sulz/tn/sulz-21.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 96 bytes of program space.

DEFINE_PALETTE( sulz_21_gp ) {
    0, 247,248,  7,
    1, 247,248,  7,
   23, 247,248,  7,
   25,   1,  1,  1,
   51,   1,  1,  1,
   51, 247,248,247,
   75, 247,248,247,
   75, 247,248,  7,
  100, 247,248,  7,
  102,   1,  1,  1,
  115,   1,  1,  1,
  115, 247,248,  7,
  138, 247,248,  7,
  139,   1,  1,  1,
  153,   1,  1,  1,
  153, 247,248,  7,
  179, 247,248,  7,
  181, 247,248,247,
  202, 247,248,247,
  203,   1,  1,  1,
  228,   1,  1,  1,
  229, 247,248,  7,
  249, 247,248,  7,
  255, 247,248,  7};

// Gradient palette "fib53_07_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/fib53/tn/fib53-07.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 96 bytes of program space.

DEFINE_PALETTE( fib53_07_gp ) {
    0,  98,114,102,
    2,  98,114,240,
   24,  98,114,240,
   25, 239,114,240,
   50, 239,114,240,
   50,  98,241,240,
   75,  98,241,240,
   76, 239,114,102,
  101, 239,114,102,
  102, 239,241,240,
  118, 239,241,240,
  120,   1,  1,  1,
  134,   1,  1,  1,
  135, 239,241,240,
  151, 239,241,240,
  153, 239,114,102,
  177, 239,114,102,
  179,  98,241,240,
  203,  98,241,240,
  204, 239,114,240,
  228, 239,114,240,
  229,  98,114,240,
  252,  98,114,240,
  255,  98,114,102};

// Gradient palette "fib53_13_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/fib53/tn/fib53-13.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 60 bytes of program space.

DEFINE_PALETTE( fib53_13_gp ) {
    0,   6, 61,240,
  101,   6, 61,240,
  101, 239,241,240,
  127, 239,241,240,
  128,   1,  1,  1,
  152,   1,  1,  1,
  153, 239,241,240,
  178, 239,241,240,
  178,   6, 61,240,
  202,   6, 61,240,
  203, 239,241,240,
  229, 239,241,240,
  230,   1,  1,  1,
  253,   1,  1,  1,
  255, 239,241,240};

// Gradient palette "fib53_17_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/fib53/tn/fib53-17.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 64 bytes of program space.

DEFINE_PALETTE( fib53_17_gp ) {
    0, 227,231,140,
    2,   1,  1,  1,
   12,   1,  1,  1,
   13,  73,184, 31,
   76,  73,184, 31,
   77,   1,  1,  1,
   89,   1,  1,  1,
   89,   1,121,  1,
  166,   1,121,  1,
  166,   1,  1,  1,
  179,   1,  1,  1,
  179,   1, 56,  1,
  241,   1, 56,  1,
  241,   1,  1,  1,
  252,   1,  1,  1,
  255, 227,231,140};

// Gradient palette "fib53_05_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/fib53/tn/fib53-05.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 72 bytes of program space.

DEFINE_PALETTE( fib53_05_gp ) {
    0, 239,241,240,
   23, 239,241,240,
   25, 239,114,102,
   51, 239,114,102,
   51,  98,114,240,
   75,  98,114,240,
   77, 239,114,240,
   99, 239,114,240,
  101,  98,114,102,
  125,  98,114,102,
  127,  98,241,240,
  152,  98,241,240,
  153,   1,  1,  1,
  178,   1,  1,  1,
  179,  98,241,240,
  204,  98,241,240,
  205, 239,241,240,
  255, 239,241,240};

// Gradient palette "fib53_18_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/fib53/tn/fib53-18.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( fib53_18_gp ) {
    0,  73,184, 31,
  179,  73,184, 31,
  179,   1,  1,  1,
  192,   1,  1,  1,
  193,   1, 56,  1,
  205,   1, 56,  1,
  205, 239,241,240,
  216, 239,241,240,
  217,   1,121,  1,
  229,   1,121,  1,
  230,   1,  1,  1,
  243,   1,  1,  1,
  243, 227,231,140,
  255, 227,231,140};

// Gradient palette "fib53_01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/fib53/tn/fib53-01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 80 bytes of program space.

DEFINE_PALETTE( fib53_01_gp ) {
    0, 239,241,240,
    2,   6, 88, 77,
   30,   6, 88, 77,
   30, 239,241,240,
   45, 239,241,240,
   46,  73, 88, 77,
   61,  73, 88, 77,
   62, 239,241,240,
   77, 239,241,240,
   79,   6, 88, 77,
  173,   6, 88, 77,
  174, 239,241,240,
  191, 239,241,240,
  192,  73, 88, 77,
  209,  73, 88, 77,
  210, 239,241,240,
  224, 239,241,240,
  225,   6, 88, 77,
  252,   6, 88, 77,
  255, 239,241,240};

// Gradient palette "mccahon_16_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/mccahon/tn/mccahon-16.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_PALETTE( mccahon_16_gp ) {
    0, 237, 95, 29,
   61, 247,233,190,
   63, 109, 73,  1,
  125, 247,233,190,
  127, 186, 20,  5,
  190, 247,233,190,
  191,   3,  1,  1,
  255, 237, 95, 29};

// Gradient palette "frizzell_09_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/frizzell/tn/frizzell-09.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 96 bytes of program space.

DEFINE_PALETTE( frizzell_09_gp ) {
    0, 242,142, 10,
    1, 137, 19,  0,
   63, 137, 19,  0,
   65, 210,189,119,
   76, 210,189,119,
   78,  79, 25,  1,
   88,  79, 25,  1,
   89, 210,189,119,
  102, 210,189,119,
  103,   1,  5,  6,
  115,   1,  5,  6,
  115,  45, 68, 64,
  137,  45, 68, 64,
  139,   1,  5,  6,
  152,   1,  5,  6,
  153, 210,189,119,
  163, 210,189,119,
  165,  79, 25,  1,
  175,  79, 25,  1,
  178, 210,189,119,
  188, 210,189,119,
  191, 137, 19,  0,
  252, 137, 19,  0,
  255, 242,142, 10};

// Gradient palette "frizzell_10_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/frizzell/tn/frizzell-10.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 56 bytes of program space.

DEFINE_PALETTE( frizzell_10_gp ) {
    0,  45, 68, 64,
   11,  45, 68, 64,
   11, 242,142, 10,
   25, 242,142, 10,
   25,   1,  5,  6,
   38,   1,  5,  6,
   39, 210,189,119,
   49, 210,189,119,
   49,  79, 25,  1,
   63,  79, 25,  1,
   65, 210,189,119,
   76, 210,189,119,
   77, 137, 19,  0,
  255, 137, 19,  0};

// Gradient palette "frizzell_12_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/frizzell/tn/frizzell-12.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 40 bytes of program space.

DEFINE_PALETTE( frizzell_12_gp ) {
    0,  45, 68, 64,
    2, 210,189,119,
   24, 210,189,119,
   25,   1,  5,  6,
  126,   1,  5,  6,
  126, 137, 19,  0,
  228, 137, 19,  0,
  230,  79, 25,  1,
  253,  79, 25,  1,
  255, 242,142, 10};

// Gradient palette "frizzell_05_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/frizzell/tn/frizzell-05.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 80 bytes of program space.

DEFINE_PALETTE( frizzell_05_gp ) {
    0, 222,133, 47,
    1,  27, 17,  4,
   28,  27, 17,  4,
   28, 247,146,178,
   53, 247,146,178,
   55,   5,  1,  1,
   84,   5,  1,  1,
   84, 247,146,178,
  112, 247,146,178,
  113,   5,  1,  1,
  139,   5,  1,  1,
  140, 247,146,178,
  166, 247,146,178,
  168,   5,  1,  1,
  195,   5,  1,  1,
  196, 247,146,178,
  223, 247,146,178,
  224,  27, 17,  4,
  253,  27, 17,  4,
  255, 222,133, 47};

// Gradient palette "haiyan_23_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/haiyan/tn/haiyan-23.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_PALETTE( haiyan_23_gp ) {
    0,  36,197,164,
  122,  36,197,164,
  124,   1,  1,  1,
  135,   1,  1,  1,
  136, 239,241,240,
  177, 239,241,240,
  178,   1,  1,  1,
  204,   1,  1,  1,
  205,  84,100, 88,
  229,  84,100, 88,
  230,   1,  1,  1,
  253,   1,  1,  1,
  255, 239,241,240};

// Gradient palette "janico_22_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gacruxa/janico/tn/janico-22.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 108 bytes of program space.

DEFINE_PALETTE( janico_22_gp ) {
    0, 112,109, 87,
    2, 126,109,115,
   37, 126,109,115,
   38,  83, 99,115,
   76,  83, 99,115,
   77, 148,127,115,
   89, 148,127,115,
   90, 112, 65, 73,
  127, 112, 65, 73,
  128, 148,127,115,
  141, 148,127,115,
  141,   4,  1,  1,
  151,   4,  1,  1,
  152, 112, 65, 53,
  165, 112, 65, 53,
  166,   4,  1,  1,
  178,   4,  1,  1,
  179,  69, 65, 53,
  202,  69, 65, 53,
  203,   4,  1,  1,
  214,   4,  1,  1,
  216, 194,225,255,
  230, 194,225,255,
  231,   4,  1,  1,
  252,   4,  1,  1,
  253,   4,  1,  1,
  255, 194,225,255};


// Gradient palette "Adrift_in_Dreams_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/colo/Skyblue2u/tn/Adrift_in_Dreams.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 40 bytes of program space.

DEFINE_PALETTE( Adrift_in_Dreams_gp ) {
    0, 148,223, 77,
   51, 148,223, 77,
   51,  86,182, 89,
  102,  86,182, 89,
  102,  36,131, 72,
  153,  36,131, 72,
  153,   5, 61, 51,
  204,   5, 61, 51,
  204,   1, 15, 29,
  255,   1, 15, 29};


// Gradient palette "Set3_03_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/cb/qual/tn/Set3_03.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_PALETTE( Set3_03_gp ) {
    0,  54,168,137,
   84,  54,168,137,
   84, 255,255,105,
  170, 255,255,105,
  170, 118,127,172,
  255, 118,127,172};

// Gradient palette "Pastel1_06_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/cb/qual/tn/Pastel1_06.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 48 bytes of program space.

DEFINE_PALETTE( Pastel1_06_gp ) {
    0, 244,118, 98,
   42, 244,118, 98,
   42, 101,157,190,
   84, 101,157,190,
   84, 142,213,133,
  127, 142,213,133,
  127, 177,154,192,
  170, 177,154,192,
  170, 252,178, 87,
  212, 252,178, 87,
  212, 255,255,145,
  255, 255,255,145};


// Gradient palette "es_rosa_55_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/rosa/tn/es_rosa_55.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( es_rosa_55_gp ) {
    0,   6,  1,  2,
  101,  54,  1, 10,
  170,  15, 29,  4,
  216,  95,124, 54,
  255, 213,233,158};

// Gradient palette "daybreak_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/pj/1/tn/daybreak.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_PALETTE( daybreak_gp ) {
    0,   1,  1,  1,
   91,   4, 11, 21,
  140,  11, 31,135,
  150, 255,255,125,
  165, 132, 18,123,
  198,  58, 92,221,
  232,  57,168,223,
  255, 255,241,242};


// Gradient palette "melancholiy_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/pj/3/tn/melancholiy.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( melancholiy_gp ) {
    0, 255,171,242,
   76,   1,  2,105,
  140, 121,136,125,
  211, 255,171,242,
  255,   1,  2,105};


// Gradient palette "xanidu_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/pj/3/tn/xanidu.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( xanidu_gp ) {
    0, 118,161,226,
    5, 255,255, 45,
   15, 252,203,156,
   53,  79,  1,162,
   94,  67,  1,  7,
  132,   1, 55,156,
  173,   1,127, 61,
  211,  39, 45, 72,
  255, 118,161,226};


// Gradient palette "air_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/pj/3/tn/air.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_PALETTE( air_gp ) {
    0, 252,246,103,
   84, 252,246,103,
  140,  14,  1, 91,
  155, 165,176,156,
  163, 252,246,103,
  170,  14,  1, 91,
  181, 165,176,156,
  193, 252,246,103,
  255, 252,246,103};


// Gradient palette "revolution2_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/pj/6/tn/revolution2.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 40 bytes of program space.

DEFINE_PALETTE( revolution2_gp ) {
    0, 112, 46, 21,
   33, 101, 69, 14,
   61, 194, 74, 29,
   91, 242,115, 52,
  119, 215,211,102,
  145,   2,  2,  1,
  163,   8, 28, 46,
  186,  17,  9,  1,
  229, 215,211,102,
  255, 242,115, 52};


// Gradient palette "sky_33_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rafi/tn/sky-33.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( sky_33_gp ) {
    0, 237,229,140,
   51, 227,107, 79,
   87, 155, 55, 54,
  178,  22, 28, 36,
  255,   5, 19, 31};

// Gradient palette "sky_45_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rafi/tn/sky-45.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_PALETTE( sky_45_gp ) {
    0, 249,205,  4,
   51, 255,239,123,
   87,   5,141, 85,
  178,   1, 26, 43,
  255,   0,  2, 23};

// Gradient palette "sky_05_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rafi/tn/sky-05.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( sky_05_gp ) {
    0, 252, 61,  2,
   25, 255,146,  4,
   63, 224,255,255,
  101,  46,114,226,
  127,   6, 40,127,
  191,   1,  3, 17,
  255,   1,  1,  4};


// Gradient palette "carousel_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rc/tn/carousel.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_PALETTE( carousel_gp ) {
    0,   2,  6, 37,
  101,   2,  6, 37,
  122, 177,121,  9,
  127, 217,149,  2,
  132, 177,121,  9,
  153,  84, 13, 36,
  255,  84, 13, 36};

// Gradient palette "nrwc_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/wkp/tubs/tn/nrwc.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_PALETTE( nrwc_gp ) {
    0,   1,  1,  1,
   25,   4,  8,  1,
   51,   1, 11,  2,
   76,   4, 36,  9,
  102,   6, 66, 18,
  127,  27, 95, 23,
  153,  82,127, 31,
  178, 197,171, 40,
  204, 133,100, 19,
  229,  97, 48,  6,
  255, 163, 55,  7};



// Single array of defined cpt-city color palettes.
// This will let us programmatically choose one based on
// a number, rather than having to activate each explicitly
// by name every time.
const byte* const gGradientPalettes[] PROGMEM = {
  // starts at #13:
  ib_jul01_gp,
  es_vintage_57_gp,
  es_vintage_01_gp,
  es_rivendell_15_gp,
  rgi_15_gp,
  retro2_16_gp,
  Analogous_1_gp,

  // 20
  es_pinksplash_08_gp,
  es_pinksplash_07_gp,
  Coral_reef_gp,
  es_ocean_breeze_068_gp,
  es_ocean_breeze_036_gp,
  departure_gp,
  es_landscape_64_gp,
  es_landscape_33_gp,
  rainbowsherbet_gp,
  gr65_hult_gp,

  // 30
  gr64_hult_gp,
  GMT_drywet_gp,
  ib15_gp,
  Fuschia_7_gp,
  es_emerald_dragon_08_gp,
  lava_gp,
  fire_gp,
  haiyan_23_gp,
  Colorfull_gp,
  Magenta_Evening_gp,

  // 40
  Pink_Purple_gp,
  Sunset_Real_gp,
  es_autumn_19_gp,
  BlacK_Blue_Magenta_White_gp,
  BlacK_Magenta_Red_gp,
  BlacK_Red_Magenta_Yellow_gp,
  Blue_Cyan_Yellow_gp,
  Sunset_Yellow_gp,
  cloud_gp,
  fireandice_gp,

  // 50
  bhw2_39_gp,
  rainfall_gp,
  tashangel_gp,
  butterflytalker_gp,
  os250k_metres_gp,
  Night_Midnight_gp,
  Afterdusk_gp,
  BlueSky_gp,
  Gold_Orange_gp,
  frizzell_10_gp,

  // 60
  frizzell_12_gp,
  fib53_18_gp,
  fib53_13_gp,
  fib53_17_gp,
  fib53_05_gp,
  Analogous_02_gp,
  Analogous_04a_gp,
  Cyan_Orange_Stripped_gp,
  Cyan_White_Green_gp,
  Wild_Orange_gp,

  // 70
  IKat_Radial_gp,
  Citrus_gp,
  Teal_Blue_gp,
  Ldby_Orange_gp,
  purple_orange_d07_gp,
  blue_tan_d08_gp,
  green_purple_d07_gp,
  knoza_00_gp,
  knoza_18_gp,
  calpan_18_gp,

  // 80
  calbayo_18_gp,
  fib53_15_gp,
  purple_orange_d08_gp,
  pmh_gp,
  konjo_08_gp,
  konkikyo_19_gp,
  mccahon_16_gp,
  Pills_2_gp,
  Pink_Yellow_Orange_1_gp,
  es_autumn_04_gp,

  // 90
  es_autumn_02_gp,
  es_candide_30_gp,
  es_chic_16_gp,
  es_coffee_01_gp,
  es_emerald_dragon_01_gp,
  es_landscape_57_gp,
  es_landscape_22_gp,
  es_landscape_47_gp,
  es_landscape_10_gp,
  es_landscape_76_gp,

  // 100
  es_landscape_61_gp,
  es_landscape_60_gp,
  es_landscape_51_gp,
  es_landscape_06_gp,
  es_ocean_breeze_049_gp,
  es_ocean_breeze_057_gp,
  es_ocean_breeze_074_gp,
  es_pinksplash_05_gp,
  es_pinksplash_10_gp,
  es_vintage_56_gp,

  // 110
  es_vintage_10_gp,
  gold_yellow_gp,
  radioactive_slime_gp,
  pastel_rainbow_gp,
  purple_sunset_gp,
  Adrift_in_Dreams_gp,
  Set3_03_gp,
  Pastel1_06_gp,
  es_rosa_55_gp,
  daybreak_gp,
  
  // 120
  melancholiy_gp,
  xanidu_gp,
  air_gp,
  revolution2_gp,
  sky_05_gp,
  sky_33_gp,
  sky_45_gp,
  carousel_gp,
  nrwc_gp,

/*
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
  candy2_gp                     //70-57 Candy2
*/
};
const uint8_t gGradientPaletteCount = ARRAY_SIZE(gGradientPalettes);


#endif
