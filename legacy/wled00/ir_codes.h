//Infrared codes

//Add your custom codes here
#define IRCUSTOM_ONOFF  0xA55AEA15 //Pioneer RC-975R "+FAV" button (example)
#define IRCUSTOM_MACRO1 0xFFFFFFFF //placeholder, will never be checked for

// Default IR codes for 6-key learning remote https://www.aliexpress.com/item/4000307837886.html
// This cheap remote has the advantage of being more powerful (longer range) than cheap credit-card remotes
#define IR6_POWER        0xFF0FF0
#define IR6_CHANNEL_UP   0xFF8F70
#define IR6_CHANNEL_DOWN 0xFF4FB0
#define IR6_VOLUME_UP    0xFFCF30
#define IR6_VOLUME_DOWN  0xFF2FD0
#define IR6_MUTE         0xFFAF50

#define IR9_POWER       0xFF629D
#define IR9_A           0xFF22DD
#define IR9_B           0xFF02FD
#define IR9_C           0xFFC23D
#define IR9_LEFT        0xFF30CF
#define IR9_RIGHT       0xFF7A85
#define IR9_UP          0xFF9867
#define IR9_DOWN        0xFF38C7
#define IR9_SELECT      0xFF18E7

//Infrared codes for 24-key remote from http://woodsgood.ca/projects/2015/02/13/rgb-led-strip-controllers-ir-codes/
#define IR24_BRIGHTER  0xF700FF
#define IR24_DARKER    0xF7807F
#define IR24_OFF       0xF740BF
#define IR24_ON        0xF7C03F
#define IR24_RED       0xF720DF
#define IR24_REDDISH   0xF710EF
#define IR24_ORANGE    0xF730CF
#define IR24_YELLOWISH 0xF708F7
#define IR24_YELLOW    0xF728D7
#define IR24_GREEN     0xF7A05F
#define IR24_GREENISH  0xF7906F
#define IR24_TURQUOISE 0xF7B04F
#define IR24_CYAN      0xF78877
#define IR24_AQUA      0xF7A857
#define IR24_BLUE      0xF7609F
#define IR24_DEEPBLUE  0xF750AF
#define IR24_PURPLE    0xF7708F
#define IR24_MAGENTA   0xF748B7
#define IR24_PINK      0xF76897
#define IR24_WHITE     0xF7E01F
#define IR24_FLASH     0xF7D02F
#define IR24_STROBE    0xF7F00F
#define IR24_FADE      0xF7C837
#define IR24_SMOOTH    0xF7E817

// 24-key defs for white remote control with CW / WW / CT+ and CT- keys (from ALDI LED pillar lamp)
#define IR24_CT_BRIGHTER   0xF700FF // BRI +
#define IR24_CT_DARKER     0xF7807F // BRI -
#define IR24_CT_OFF        0xF740BF // OFF
#define IR24_CT_ON         0xF7C03F // ON
#define IR24_CT_RED        0xF720DF // RED
#define IR24_CT_REDDISH    0xF710EF // REDDISH
#define IR24_CT_ORANGE     0xF730CF // ORANGE
#define IR24_CT_YELLOWISH  0xF708F7 // YELLOWISH
#define IR24_CT_YELLOW     0xF728D7 // YELLOW
#define IR24_CT_GREEN      0xF7A05F // GREEN
#define IR24_CT_GREENISH   0xF7906F // GREENISH
#define IR24_CT_TURQUOISE  0xF7B04F // TURQUOISE
#define IR24_CT_CYAN       0xF78877 // CYAN
#define IR24_CT_AQUA       0xF7A857 // AQUA
#define IR24_CT_BLUE       0xF7609F // BLUE
#define IR24_CT_DEEPBLUE   0xF750AF // DEEPBLUE
#define IR24_CT_PURPLE     0xF7708F // PURPLE
#define IR24_CT_MAGENTA    0xF748B7 // MAGENTA
#define IR24_CT_PINK       0xF76897 // PINK
#define IR24_CT_COLDWHITE  0xF7E01F // CW
#define IR24_CT_WARMWHITE  0xF7D02F // WW
#define IR24_CT_CTPLUS     0xF7F00F // CT+
#define IR24_CT_CTMINUS    0xF7C837 // CT-
#define IR24_CT_MEMORY     0xF7E817 // MEMORY

// 24-key defs for old remote control
#define IR24_OLD_BRIGHTER  0xFF906F // Brightness Up
#define IR24_OLD_DARKER    0xFFB847 // Brightness Down
#define IR24_OLD_OFF       0xFFF807 // Power OFF
#define IR24_OLD_ON        0xFFB04F // Power On
#define IR24_OLD_RED       0xFF9867 // RED
#define IR24_OLD_REDDISH   0xFFE817 // Light RED
#define IR24_OLD_ORANGE    0xFF02FD // Orange
#define IR24_OLD_YELLOWISH 0xFF50AF // Light Orange
#define IR24_OLD_YELLOW    0xFF38C7 // YELLOW
#define IR24_OLD_GREEN     0xFFD827 // GREEN
#define IR24_OLD_GREENISH  0xFF48B7 // Light GREEN
#define IR24_OLD_TURQUOISE 0xFF32CD // TURQUOISE
#define IR24_OLD_CYAN      0xFF7887 // CYAN
#define IR24_OLD_AQUA      0xFF28D7 // AQUA
#define IR24_OLD_BLUE      0xFF8877 // BLUE
#define IR24_OLD_DEEPBLUE  0xFF6897 // Dark BLUE
#define IR24_OLD_PURPLE    0xFF20DF // PURPLE
#define IR24_OLD_MAGENTA   0xFF708F // MAGENTA
#define IR24_OLD_PINK      0xFFF00F // PINK
#define IR24_OLD_WHITE     0xFFA857 // WHITE
#define IR24_OLD_FLASH     0xFFB24D // FLASH Mode
#define IR24_OLD_STROBE    0xFF00FF // STROBE Mode
#define IR24_OLD_FADE      0xFF58A7 // FADE Mode
#define IR24_OLD_SMOOTH    0xFF30CF // SMOOTH Mode

// 40-key defs for blue remote control
#define IR40_BPLUS         0xFF3AC5  //
#define IR40_BMINUS        0xFFBA45  //
#define IR40_OFF           0xFF827D  //
#define IR40_ON            0xFF02FD  //
#define IR40_RED           0xFF1AE5  //
#define IR40_GREEN         0xFF9A65  //
#define IR40_BLUE          0xFFA25D  //
#define IR40_WHITE         0xFF22DD  // natural white
#define IR40_REDDISH       0xFF2AD5  //
#define IR40_GREENISH      0xFFAA55  //
#define IR40_DEEPBLUE      0xFF926D  //
#define IR40_WARMWHITE2    0xFF12ED  // warmest white
#define IR40_ORANGE        0xFF0AF5  //
#define IR40_TURQUOISE     0xFF8A75  //
#define IR40_PURPLE        0xFFB24D  //
#define IR40_WARMWHITE     0xFF32CD  // warm white
#define IR40_YELLOWISH     0xFF38C7  //
#define IR40_CYAN          0xFFB847  //
#define IR40_MAGENTA       0xFF7887  //
#define IR40_COLDWHITE     0xFFF807  // cold white
#define IR40_YELLOW        0xFF18E7  //
#define IR40_AQUA          0xFF9867  //
#define IR40_PINK          0xFF58A7  //
#define IR40_COLDWHITE2    0xFFD827  // coldest white
#define IR40_WPLUS         0xFF28D7  // white chanel bright plus
#define IR40_WMINUS        0xFFA857  // white chanel bright minus
#define IR40_WOFF          0xFF6897  // white chanel on
#define IR40_WON           0xFFE817  // white chanel off
#define IR40_W25           0xFF08F7  // white chanel 25%
#define IR40_W50           0xFF8877  // white chanel 50%
#define IR40_W75           0xFF48B7  // white chanel 75%
#define IR40_W100          0xFFC837  // white chanel 100%
#define IR40_JUMP3         0xFF30CF  // JUMP3
#define IR40_FADE3         0xFFB04F  // FADE3
#define IR40_JUMP7         0xFF708F  // JUMP7
#define IR40_QUICK         0xFFF00F  // QUICK
#define IR40_FADE7         0xFF10EF  // FADE7
#define IR40_FLASH         0xFF906F  // FLASH
#define IR40_AUTO          0xFF50AF  // AUTO
#define IR40_SLOW          0xFFD02F  // SLOW

// 44-key defs
#define IR44_BPLUS         0xFF3AC5  //
#define IR44_BMINUS        0xFFBA45  //
#define IR44_OFF           0xFF827D  //
#define IR44_ON            0xFF02FD  //
#define IR44_RED           0xFF1AE5  //
#define IR44_GREEN         0xFF9A65  //
#define IR44_BLUE          0xFFA25D  //
#define IR44_WHITE         0xFF22DD  // natural white
#define IR44_REDDISH       0xFF2AD5  //
#define IR44_GREENISH      0xFFAA55  //
#define IR44_DEEPBLUE      0xFF926D  //
#define IR44_WARMWHITE2    0xFF12ED  // warmest white
#define IR44_ORANGE        0xFF0AF5  //
#define IR44_TURQUOISE     0xFF8A75  //
#define IR44_PURPLE        0xFFB24D  //
#define IR44_WARMWHITE     0xFF32CD  // warm white
#define IR44_YELLOWISH     0xFF38C7  //
#define IR44_CYAN          0xFFB847  //
#define IR44_MAGENTA       0xFF7887  //
#define IR44_COLDWHITE     0xFFF807  // cold white
#define IR44_YELLOW        0xFF18E7  //
#define IR44_AQUA          0xFF9867  //
#define IR44_PINK          0xFF58A7  //
#define IR44_COLDWHITE2    0xFFD827  // coldest white
#define IR44_REDPLUS       0xFF28D7  //
#define IR44_GREENPLUS     0xFFA857  //
#define IR44_BLUEPLUS      0xFF6897  //
#define IR44_QUICK         0xFFE817  //
#define IR44_REDMINUS      0xFF08F7  //
#define IR44_GREENMINUS    0xFF8877  //
#define IR44_BLUEMINUS     0xFF48B7  //
#define IR44_SLOW          0xFFC837  //
#define IR44_DIY1          0xFF30CF  //
#define IR44_DIY2          0xFFB04F  //
#define IR44_DIY3          0xFF708F  //
#define IR44_AUTO          0xFFF00F  //
#define IR44_DIY4          0xFF10EF  //
#define IR44_DIY5          0xFF906F  //
#define IR44_DIY6          0xFF50AF  //
#define IR44_FLASH         0xFFD02F  //
#define IR44_JUMP3         0xFF20DF  //
#define IR44_JUMP7         0xFFA05F  //
#define IR44_FADE3         0xFF609F  //
#define IR44_FADE7         0xFFE01F  //

//Infrared codes for 21-key remote https://images-na.ssl-images-amazon.com/images/I/51NMA0XucnL.jpg
#define IR21_BRIGHTER      0xFFE01F
#define IR21_DARKER        0xFFA857
#define IR21_OFF           0xFF629D
#define IR21_ON            0xFFA25D
#define IR21_RED           0xFF6897
#define IR21_REDDISH       0xFF30CF
#define IR21_ORANGE        0xFF10EF
#define IR21_YELLOWISH     0xFF42BD
#define IR21_GREEN         0xFF9867
#define IR21_GREENISH      0xFF18E7
#define IR21_TURQUOISE     0xFF38C7
#define IR21_CYAN          0xFF4AB5
#define IR21_BLUE          0xFFB04F
#define IR21_DEEPBLUE      0xFF7A85
#define IR21_PURPLE        0xFF5AA5
#define IR21_PINK          0xFF52AD
#define IR21_WHITE         0xFF906F
#define IR21_FLASH         0xFFE21D
#define IR21_STROBE        0xFF22DD
#define IR21_FADE          0xFF02FD
#define IR21_SMOOTH        0xFFC23D

#define COLOR_RED            0xFF0000
#define COLOR_REDDISH        0xFF7800
#define COLOR_ORANGE         0xFFA000
#define COLOR_YELLOWISH      0xFFC800
#define COLOR_YELLOW         0xFFFF00
#define COLOR_GREEN          0x00FF00
#define COLOR_GREENISH       0x00FF78
#define COLOR_TURQUOISE      0x00FFA0
#define COLOR_CYAN           0x00FFDC
#define COLOR_AQUA           0x00C8FF
#define COLOR_BLUE           0x00A0FF
#define COLOR_DEEPBLUE       0x0000FF
#define COLOR_PURPLE         0xAA00FF
#define COLOR_MAGENTA        0xFF00DC
#define COLOR_PINK           0xFF00A0
#define COLOR_WHITE          0xFFFFFFFF
#define COLOR_WARMWHITE2     0xFFFFAA69
#define COLOR_WARMWHITE      0xFFFFBF8E
#define COLOR_NEUTRALWHITE   0xFFFFD4B4
#define COLOR_COLDWHITE      0xFFFFE9D9
#define COLOR_COLDWHITE2     0xFFFFFFFF

#define ACTION_NONE             0
#define ACTION_BRIGHT_UP        1
#define ACTION_BRIGHT_DOWN      2
#define ACTION_SPEED_UP         3
#define ACTION_SPEED_DOWN       4
#define ACTION_INTENSITY_UP     5
#define ACTION_INTENSITY_DOWN   6
#define ACTION_POWER            7
