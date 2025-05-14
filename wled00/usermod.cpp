#include "wled.h"
#include <FastLED.h>
#include <PCA9634.h>

#define LED_PIN NULL
#define OUTPUT_PIN 16

// How many leds in your strip?
#define NUM_LEDS 1

// Define the array of leds
CRGB ledArray[NUM_LEDS];

// Define PCA9634 device --> Constuctor takes in I2C address of d115
PCA9634 ledDriver(0x70);

// function prototype
void updateLEDs();
/*
 * This v1 usermod file allows you to add own functionality to WLED more easily
 * See: https://github.com/wled-dev/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in const.h)
 * If you just need 8 bytes, use 2551-2559 (you do not need to increase EEPSIZE)
 *
 * Consider the v2 usermod API if you need a more advanced feature set!
 */

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

//gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup()
{
    // Set up our custom board and its peripherals
    // setup output pin
    pinMode(OUTPUT_PIN, OUTPUT);
    digitalWrite(OUTPUT_PIN, HIGH);
    // enable led
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);

    // Initialize serial communication at esp8266 native baud rate of 74880
    Serial.begin(74880);
    while (!Serial) {
        ; // Wait for serial port to connect
    }
    // Print a message to the Serial Monitor
    Serial.println("Serial communication initialized.");

    // Uncomment/edit one of the following lines for your leds arrangement.
    // ## Clockless types ##
    FastLED.addLeds<WS2812, LED_PIN, GRB>(ledArray, NUM_LEDS); // dummy setup

    // Start 2wire comm with the following pins
    Wire.begin(4, 5);

    ledDriver.begin(0x01, 0x14); // start pca control with register mode values
    ledDriver.setLedDriverMode(0x00, 0x01);  // MODE1
    ledDriver.setLedDriverMode(0x01, 0x14);  // MODE2
    ledDriver.setLedDriverMode(0x0C, 0xAA);  // LEDOUT0
    ledDriver.setLedDriverMode(0x0D, 0xAA);  // LEDOUT1
    ledDriver.setLedDriverMode(PCA963X_LEDPWM); // PWM MODE ALL LEDS

    // enable output pin
    digitalWrite(OUTPUT_PIN, LOW);

    Serial.println("Test - write1 - I");
    for (int channel = 0; channel < 4; channel++) {
        for (int pwm = 0; pwm < 256; pwm++) {
            ledDriver.write1(channel, pwm);
            delay(4);
        }
    }
    Serial.println("done...");

    Serial.println("setup loop initialized.");

    digitalWrite(2, LOW);

}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected()
{
   
}

//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop()
{

}


