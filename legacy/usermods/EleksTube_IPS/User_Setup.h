/*
 * This is intended to over-ride `User_Setup.h` that comes with the TFT_eSPI library.
 * I hate having to modify the library code.
 */

// ST7789 135 x 240 display with no chip select line

#define ST7789_DRIVER     // Configure all registers

#define TFT_WIDTH  135
#define TFT_HEIGHT 240

#define CGRAM_OFFSET      // Library will add offsets required

//#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

//#define TFT_INVERSION_ON
//#define TFT_INVERSION_OFF

// EleksTube IPS
#define TFT_SDA_READ      // Read and write on the MOSI/SDA pin, no separate MISO pin
#define TFT_MOSI 23
#define TFT_SCLK 18
//#define TFT_CS    -1 // Not connected
#define TFT_DC   25  // Data Command, aka Register Select or RS
#define TFT_RST  26  // Connect reset to ensure display initialises

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
//#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
//#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
//#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
//#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
//#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
//#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

//#define SMOOTH_FONT


//#define SPI_FREQUENCY  27000000
#define SPI_FREQUENCY  40000000

/*
 * To make the Library not over-write all this:
 */
#define USER_SETUP_LOADED
