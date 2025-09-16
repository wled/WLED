#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// CRGBW struct for RGBW LED support (from WLED)
struct CRGBW {
    union {
        uint32_t color32; // Access as a 32-bit value (0xWWRRGGBB)
        struct {
            uint8_t b;
            uint8_t g;
            uint8_t r;
            uint8_t w;
        };
        uint8_t raw[4];   // Access as an array in the order B, G, R, W
    };

    // Default constructor
    inline CRGBW() __attribute__((always_inline)) = default;

    // Constructor from a 32-bit color (0xWWRRGGBB)
    constexpr CRGBW(uint32_t color) __attribute__((always_inline)) : color32(color) {}

    // Constructor with r, g, b, w values
    constexpr CRGBW(uint8_t red, uint8_t green, uint8_t blue, uint8_t white = 0) __attribute__((always_inline)) : b(blue), g(green), r(red), w(white) {}

    // Constructor from CRGB
    constexpr CRGBW(CRGB rgb) __attribute__((always_inline)) : b(rgb.b), g(rgb.g), r(rgb.r), w(0) {}

    // Assignment from CRGB
    inline CRGBW& operator=(const CRGB& rgb) __attribute__((always_inline)) { b = rgb.b; g = rgb.g; r = rgb.r; w = 0; return *this; }

    // Conversion operator to uint32_t
    inline operator uint32_t() const __attribute__((always_inline)) {
      return color32;
    }
};

// ArkLights PEV Lighting System - Modular Version
// This is a clean, focused implementation for PEV devices

// Configuration for XIAO ESP32S3
#define HEADLIGHT_PIN 3
#define TAILLIGHT_PIN 2
#define HEADLIGHT_CLOCK_PIN 4  // For APA102/LPD8806
#define TAILLIGHT_CLOCK_PIN 5  // For APA102/LPD8806
#define DEFAULT_BRIGHTNESS 128

// LED Configuration (can be changed via web UI)
uint8_t headlightLedCount = 11;
uint8_t taillightLedCount = 11;
uint8_t headlightLedType = 0;  // 0=SK6812, 1=WS2812B, 2=APA102, 3=LPD8806
uint8_t taillightLedType = 0;
uint8_t headlightColorOrder = 0;  // 0=GRBW, 1=RGBW, 2=BGRW, 3=GRB, 4=RGB, 5=BGR
uint8_t taillightColorOrder = 0;

// LED Type Configuration (matching WLED setup)
// Can be overridden via build flags: -D LED_TYPE=SK6812 -D LED_COLOR_ORDER=GRB
#ifndef LED_TYPE
#define LED_TYPE SK6812  // Default to SK6812 (RGBW capable)
#endif
#ifndef LED_COLOR_ORDER
#define LED_COLOR_ORDER GRB  // Default color order
#endif

#define LED_TYPE_HEADLIGHT LED_TYPE
#define LED_TYPE_TAILLIGHT LED_TYPE
#define LED_COLOR_ORDER_HEADLIGHT LED_COLOR_ORDER
#define LED_COLOR_ORDER_TAILLIGHT LED_COLOR_ORDER

// WiFi AP Configuration
const char* AP_SSID = "ARKLIGHTS-AP";
const char* AP_PASSWORD = "float420";
const int AP_CHANNEL = 1;
const int MAX_CONNECTIONS = 4;

// Web Server
WebServer server(80);

// Effect IDs
#define FX_SOLID 0
#define FX_BREATH 1
#define FX_RAINBOW 2
#define FX_CHASE 3
#define FX_BLINK_RAINBOW 4
#define FX_TWINKLE 5
#define FX_FIRE 6
#define FX_METEOR 7
#define FX_WAVE 8
#define FX_COMET 9
#define FX_CANDLE 10
#define FX_STATIC_RAINBOW 11
#define FX_KNIGHT_RIDER 12
#define FX_POLICE 13
#define FX_STROBE 14
#define FX_LARSON_SCANNER 15
#define FX_COLOR_WIPE 16
#define FX_THEATER_CHASE 17
#define FX_RUNNING_LIGHTS 18
#define FX_COLOR_SWEEP 19

// Preset IDs
#define PRESET_STANDARD 0
#define PRESET_NIGHT 1
#define PRESET_PARTY 2
#define PRESET_STEALTH 3

// Startup Sequence IDs
#define STARTUP_NONE 0
#define STARTUP_POWER_ON 1
#define STARTUP_SCAN 2
#define STARTUP_WAVE 3
#define STARTUP_RACE 4
#define STARTUP_CUSTOM 5

// LED strips (dynamic size) - Use CRGB for all LED types
CRGB* headlight;
CRGB* taillight;

// System state
uint8_t globalBrightness = DEFAULT_BRIGHTNESS;
uint8_t currentPreset = PRESET_STANDARD;
uint8_t headlightEffect = FX_SOLID;
uint8_t taillightEffect = FX_SOLID;
CRGB headlightColor = CRGB::White;
CRGB taillightColor = CRGB::Red;
uint8_t effectSpeed = 64; // Speed control (0-255, higher = faster) - Default to slower speed

// Startup sequence settings
uint8_t startupSequence = STARTUP_POWER_ON;
bool startupEnabled = true;
uint16_t startupDuration = 3000; // Duration in milliseconds

// Effect state
unsigned long lastUpdate = 0;
uint16_t effectStep = 0;
unsigned long lastEffectUpdate = 0;

// Startup sequence state
bool startupActive = false;
unsigned long startupStartTime = 0;
uint16_t startupStep = 0;

// WiFi AP Configuration
String apName = "ARKLIGHTS-AP";
String apPassword = "float420";

// Filesystem-based persistent storage

// Function declarations
void updateEffects();
void effectBreath(CRGB* leds, uint8_t numLeds, CRGB color);
void effectRainbow(CRGB* leds, uint8_t numLeds);
void effectChase(CRGB* leds, uint8_t numLeds, CRGB color);
void effectBlinkRainbow(CRGB* leds, uint8_t numLeds);
void effectTwinkle(CRGB* leds, uint8_t numLeds, CRGB color);
void effectFire(CRGB* leds, uint8_t numLeds);
void effectMeteor(CRGB* leds, uint8_t numLeds, CRGB color);
void effectWave(CRGB* leds, uint8_t numLeds, CRGB color);
void effectComet(CRGB* leds, uint8_t numLeds, CRGB color);
void effectCandle(CRGB* leds, uint8_t numLeds);
void effectStaticRainbow(CRGB* leds, uint8_t numLeds);
void effectKnightRider(CRGB* leds, uint8_t numLeds, CRGB color);
void effectPolice(CRGB* leds, uint8_t numLeds);
void effectStrobe(CRGB* leds, uint8_t numLeds, CRGB color);
void effectLarsonScanner(CRGB* leds, uint8_t numLeds, CRGB color);
void effectColorWipe(CRGB* leds, uint8_t numLeds, CRGB color);
void effectTheaterChase(CRGB* leds, uint8_t numLeds, CRGB color);
void effectRunningLights(CRGB* leds, uint8_t numLeds, CRGB color);
void effectColorSweep(CRGB* leds, uint8_t numLeds, CRGB color);
void setPreset(uint8_t preset);
void handleSerialCommands();
void printStatus();
void printHelp();

// Startup sequence functions
void startStartupSequence();
void updateStartupSequence();
void startupPowerOn();
void startupScan();
void startupWave();
void startupRace();
void startupCustom();
String getStartupSequenceName(uint8_t sequence);

// LED Configuration functions
void initializeLEDs();
void testLEDConfiguration();
String getLEDTypeName(uint8_t type);
String getColorOrderName(uint8_t order);
// Filesystem functions
void initFilesystem();
bool saveSettings();
bool loadSettings();
void testFilesystem();

// Web server functions
void setupWiFiAP();
void setupWebServer();
void handleRoot();
void handleAPI();
void handleStatus();
void handleLEDConfig();
void handleLEDTest();
void sendJSONResponse(DynamicJsonDocument& doc);

void setup() {
    Serial.begin(115200);
    Serial.println("ArkLights PEV Lighting System");
    Serial.println("==============================");
    
    // Initialize filesystem
    initFilesystem();
    
    // Load saved settings
    loadSettings();
    
    // Test filesystem
    testFilesystem();
    
    // Initialize LED strips early for visual debugging
    
    initializeLEDs();
    
    // Start startup sequence if enabled
    Serial.printf("🔍 Startup check: enabled=%s, sequence=%d (%s)\n", 
                  startupEnabled ? "true" : "false", 
                  startupSequence, 
                  getStartupSequenceName(startupSequence).c_str());
    
    if (startupEnabled && startupSequence != STARTUP_NONE) {
        startStartupSequence();
    } else {
        // Show loaded colors immediately
        Serial.println("⚡ Skipping startup sequence, showing loaded colors");
        fill_solid(headlight, headlightLedCount, headlightColor);
        fill_solid(taillight, taillightLedCount, taillightColor);
        FastLED.show();
        delay(1000);
    }
    
    Serial.printf("Headlight: %d LEDs on GPIO %d (Type: %s, Order: %s)\n", 
                  headlightLedCount, HEADLIGHT_PIN, 
                  getLEDTypeName(headlightLedType).c_str(),
                  getColorOrderName(headlightColorOrder).c_str());
    Serial.printf("Taillight: %d LEDs on GPIO %d (Type: %s, Order: %s)\n", 
                  taillightLedCount, TAILLIGHT_PIN,
                  getLEDTypeName(taillightLedType).c_str(),
                  getColorOrderName(taillightColorOrder).c_str());
    
    // LED strips already initialized for visual debugging
    
    // Apply loaded brightness setting
    FastLED.setBrightness(globalBrightness);
    
    // Settings are already loaded from flash, no need to override with presets
    
    // Setup WiFi AP and Web Server
    setupWiFiAP();
    setupWebServer();
    
    Serial.println("System initialized successfully!");
    Serial.println("Web UI available at: http://192.168.4.1");
    
    // Debug: Final color check before main loop
    Serial.printf("🔍 Final colors before main loop - Headlight RGB(%d,%d,%d), Taillight RGB(%d,%d,%d)\n",
                  headlightColor.r, headlightColor.g, headlightColor.b,
                  taillightColor.r, taillightColor.g, taillightColor.b);
    
    printHelp();
}

void loop() {
    // Update startup sequence if active
    if (startupActive) {
        updateStartupSequence();
        FastLED.show();
        delay(50); // Slower update for startup sequences
        return;
    }
    
    // Update effects at 50 FPS
    if (millis() - lastUpdate >= 20) {
        updateEffects();
        FastLED.show();
        lastUpdate = millis();
    }
    
    // Handle serial commands
    handleSerialCommands();
    
    // Handle web server requests
    server.handleClient();
    
    delay(10);
}

void updateEffects() {
    // Calculate effect update rate based on speed (slower speed = less frequent updates)
    uint16_t updateInterval = map(effectSpeed, 0, 255, 200, 20); // 20ms to 200ms between updates
    
    if (millis() - lastEffectUpdate < updateInterval) {
        return; // Skip this update to slow down effects
    }
    lastEffectUpdate = millis();
    
    // Update headlight effect
    switch (headlightEffect) {
        case FX_SOLID:
            fill_solid(headlight, headlightLedCount, headlightColor);
            break;
        case FX_BREATH:
            effectBreath(headlight, headlightLedCount, headlightColor);
            break;
        case FX_RAINBOW:
            effectRainbow(headlight, headlightLedCount);
            break;
        case FX_CHASE:
            effectChase(headlight, headlightLedCount, headlightColor);
            break;
        case FX_BLINK_RAINBOW:
            effectBlinkRainbow(headlight, headlightLedCount);
            break;
        case FX_TWINKLE:
            effectTwinkle(headlight, headlightLedCount, headlightColor);
            break;
        case FX_FIRE:
            effectFire(headlight, headlightLedCount);
            break;
        case FX_METEOR:
            effectMeteor(headlight, headlightLedCount, headlightColor);
            break;
        case FX_WAVE:
            effectWave(headlight, headlightLedCount, headlightColor);
            break;
        case FX_COMET:
            effectComet(headlight, headlightLedCount, headlightColor);
            break;
        case FX_CANDLE:
            effectCandle(headlight, headlightLedCount);
            break;
        case FX_STATIC_RAINBOW:
            effectStaticRainbow(headlight, headlightLedCount);
            break;
        case FX_KNIGHT_RIDER:
            effectKnightRider(headlight, headlightLedCount, headlightColor);
            break;
        case FX_POLICE:
            effectPolice(headlight, headlightLedCount);
            break;
        case FX_STROBE:
            effectStrobe(headlight, headlightLedCount, headlightColor);
            break;
        case FX_LARSON_SCANNER:
            effectLarsonScanner(headlight, headlightLedCount, headlightColor);
            break;
        case FX_COLOR_WIPE:
            effectColorWipe(headlight, headlightLedCount, headlightColor);
            break;
        case FX_THEATER_CHASE:
            effectTheaterChase(headlight, headlightLedCount, headlightColor);
            break;
        case FX_RUNNING_LIGHTS:
            effectRunningLights(headlight, headlightLedCount, headlightColor);
            break;
        case FX_COLOR_SWEEP:
            effectColorSweep(headlight, headlightLedCount, headlightColor);
            break;
    }
    
    // Update taillight effect
    switch (taillightEffect) {
        case FX_SOLID:
            fill_solid(taillight, taillightLedCount, taillightColor);
            break;
        case FX_BREATH:
            effectBreath(taillight, taillightLedCount, taillightColor);
            break;
        case FX_RAINBOW:
            effectRainbow(taillight, taillightLedCount);
            break;
        case FX_CHASE:
            effectChase(taillight, taillightLedCount, taillightColor);
            break;
        case FX_BLINK_RAINBOW:
            effectBlinkRainbow(taillight, taillightLedCount);
            break;
        case FX_TWINKLE:
            effectTwinkle(taillight, taillightLedCount, taillightColor);
            break;
        case FX_FIRE:
            effectFire(taillight, taillightLedCount);
            break;
        case FX_METEOR:
            effectMeteor(taillight, taillightLedCount, taillightColor);
            break;
        case FX_WAVE:
            effectWave(taillight, taillightLedCount, taillightColor);
            break;
        case FX_COMET:
            effectComet(taillight, taillightLedCount, taillightColor);
            break;
        case FX_CANDLE:
            effectCandle(taillight, taillightLedCount);
            break;
        case FX_STATIC_RAINBOW:
            effectStaticRainbow(taillight, taillightLedCount);
            break;
        case FX_KNIGHT_RIDER:
            effectKnightRider(taillight, taillightLedCount, taillightColor);
            break;
        case FX_POLICE:
            effectPolice(taillight, taillightLedCount);
            break;
        case FX_STROBE:
            effectStrobe(taillight, taillightLedCount, taillightColor);
            break;
        case FX_LARSON_SCANNER:
            effectLarsonScanner(taillight, taillightLedCount, taillightColor);
            break;
        case FX_COLOR_WIPE:
            effectColorWipe(taillight, taillightLedCount, taillightColor);
            break;
        case FX_THEATER_CHASE:
            effectTheaterChase(taillight, taillightLedCount, taillightColor);
            break;
        case FX_RUNNING_LIGHTS:
            effectRunningLights(taillight, taillightLedCount, taillightColor);
            break;
        case FX_COLOR_SWEEP:
            effectColorSweep(taillight, taillightLedCount, taillightColor);
            break;
    }
}

void effectBreath(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Calculate breathing speed based on effectSpeed (0-255)
    // Higher speed = faster breathing
    uint16_t breathSpeed = map(effectSpeed, 0, 255, 15000, 1000); // 1s to 15s cycle (much slower)
    uint8_t breathe = (sin(millis() / (breathSpeed / 1000.0)) + 1) * 127;
    CRGB breatheColor = color;
    breatheColor.nscale8(breathe);
    fill_solid(leds, numLeds, breatheColor);
}

void effectRainbow(CRGB* leds, uint8_t numLeds) {
    // Calculate rainbow speed based on effectSpeed (0-255)
    uint16_t rainbowSpeed = map(effectSpeed, 0, 255, 1000, 50); // 50ms to 1000ms per step (much slower)
    uint16_t hue = (effectStep * 65536L / numLeds) + (millis() / rainbowSpeed);
    for (uint8_t i = 0; i < numLeds; i++) {
        uint16_t pixelHue = hue + (i * 65536L / numLeds);
        leds[i] = CHSV(pixelHue >> 8, 255, 255);
    }
    effectStep += 2;
}

void effectChase(CRGB* leds, uint8_t numLeds, CRGB color) {
    uint8_t pos = effectStep % numLeds;
    fill_solid(leds, numLeds, CRGB::Black);
    leds[pos] = color;
    // Speed control: higher speed = faster movement
    uint8_t stepSize = map(effectSpeed, 0, 255, 1, 2); // Even slower - max 2 steps (was 3)
    effectStep += stepSize;
}

void effectBlinkRainbow(CRGB* leds, uint8_t numLeds) {
    // Calculate blink speed based on effectSpeed (0-255)
    uint16_t blinkSpeed = map(effectSpeed, 0, 255, 10000, 800); // 800ms to 10s (much slower)l
    bool blinkState = (millis() / blinkSpeed) % 2;
    if (blinkState) {
        effectRainbow(leds, numLeds);
    } else {
        fill_solid(leds, numLeds, CRGB::Black);
    }
}

void effectTwinkle(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Set background
    fill_solid(leds, numLeds, CRGB::Black);
    
    // Calculate twinkle speed based on effectSpeed (0-255)
    uint8_t numTwinkles = map(effectSpeed, 0, 255, numLeds / 8, numLeds / 2);
    for (uint8_t i = 0; i < numTwinkles; i++) {
        uint8_t pos = random(numLeds);
        uint8_t brightness = random(128, 255);
        CRGB twinkleColor = color;
        twinkleColor.nscale8(brightness);
        leds[pos] = twinkleColor;
    }
}

// WLED-inspired Fire Effect
void effectFire(CRGB* leds, uint8_t numLeds) {
    // Fire simulation with heat and cooling
    static uint8_t heat[200]; // Max LEDs supported
    
    // Calculate fire speed based on effectSpeed
    uint8_t cooling = map(effectSpeed, 0, 255, 50, 200);
    uint8_t sparking = map(effectSpeed, 0, 255, 50, 120);
    
    // Cool down every cell a little
    for (uint8_t i = 0; i < numLeds; i++) {
        heat[i] = qsub8(heat[i], random(0, ((cooling * 10) / numLeds) + 2));
    }
    
    // Heat from each cell drifts 'up' and diffuses a little
    for (uint8_t k = numLeds - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    
    // Randomly ignite new 'sparks' near the bottom
    if (random(255) < sparking) {
        uint8_t y = random(7);
        heat[y] = qadd8(heat[y], random(160, 255));
    }
    
    // Convert heat to LED colors
    for (uint8_t j = 0; j < numLeds; j++) {
        CRGB color = HeatColor(heat[j]);
        leds[j] = color;
    }
}

// WLED-inspired Meteor Effect
void effectMeteor(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Fade all LEDs
    for (uint8_t i = 0; i < numLeds; i++) {
        leds[i].nscale8(192); // Fade by 25%
    }
    
    // Calculate meteor speed
    uint8_t meteorSize = map(effectSpeed, 0, 255, 1, 5);
    uint8_t meteorPos = (effectStep / 2) % (numLeds + meteorSize);
    
    // Draw meteor
    for (uint8_t i = 0; i < meteorSize; i++) {
        if (meteorPos - i >= 0 && meteorPos - i < numLeds) {
            uint8_t brightness = 255 - (i * 255 / meteorSize);
            CRGB meteorColor = color;
            meteorColor.nscale8(brightness);
            leds[meteorPos - i] = meteorColor;
        }
    }
    
    effectStep++;
}

// WLED-inspired Wave Effect
void effectWave(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Calculate wave speed
    uint16_t waveSpeed = map(effectSpeed, 0, 255, 1000, 100);
    uint8_t wavePos = (millis() / waveSpeed) % (numLeds * 2);
    
    // Create wave pattern
    for (uint8_t i = 0; i < numLeds; i++) {
        uint8_t distance = abs(i - wavePos);
        if (distance > numLeds) distance = (numLeds * 2) - distance;
        
        uint8_t brightness = 255 - (distance * 255 / numLeds);
        if (brightness > 0) {
            CRGB waveColor = color;
            waveColor.nscale8(brightness);
            leds[i] = waveColor;
        } else {
            leds[i] = CRGB::Black;
        }
    }
}

// WLED-inspired Comet Effect
void effectComet(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Fade all LEDs
    for (uint8_t i = 0; i < numLeds; i++) {
        leds[i].nscale8(200); // Fade by 22%
    }
    
    // Calculate comet speed and size
    uint8_t cometSize = map(effectSpeed, 0, 255, 2, 8);
    uint8_t cometPos = (effectStep / 3) % (numLeds + cometSize);
    
    // Draw comet with tail
    for (uint8_t i = 0; i < cometSize; i++) {
        if (cometPos - i >= 0 && cometPos - i < numLeds) {
            uint8_t brightness = 255 - (i * 255 / cometSize);
            CRGB cometColor = color;
            cometColor.nscale8(brightness);
            leds[cometPos - i] = cometColor;
        }
    }
    
    effectStep++;
}

// WLED-inspired Candle Effect
void effectCandle(CRGB* leds, uint8_t numLeds) {
    // Calculate candle flicker speed
    uint8_t flickerSpeed = map(effectSpeed, 0, 255, 50, 200);
    
    for (uint8_t i = 0; i < numLeds; i++) {
        // Base candle color (warm white/orange)
        CRGB baseColor = CRGB(255, 147, 41); // Warm orange
        
        // Add random flicker
        uint8_t flicker = random(0, flickerSpeed);
        uint8_t brightness = 200 + flicker;
        
        CRGB candleColor = baseColor;
        candleColor.nscale8(brightness);
        leds[i] = candleColor;
    }
}

// WLED-inspired Static Rainbow Effect
void effectStaticRainbow(CRGB* leds, uint8_t numLeds) {
    // Static rainbow - no movement, just rainbow colors across the strip
    for (uint8_t i = 0; i < numLeds; i++) {
        uint8_t hue = (i * 255) / numLeds;
        leds[i] = CHSV(hue, 255, 255);
    }
}

// Electrolyte-style Knight Rider Effect (KITT scanner)
void effectKnightRider(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Fade all LEDs
    for (uint8_t i = 0; i < numLeds; i++) {
        leds[i].nscale8(200); // Fade by 22%
    }
    
    // Calculate scanner speed and size
    uint8_t scannerSize = map(effectSpeed, 0, 255, 2, 6);
    uint8_t scannerPos = (effectStep / 2) % ((numLeds + scannerSize) * 2);
    
    // Determine direction (forward or backward)
    bool forward = (scannerPos < (numLeds + scannerSize));
    uint8_t actualPos = forward ? scannerPos : ((numLeds + scannerSize) * 2) - scannerPos - 1;
    
    // Draw scanner
    for (uint8_t i = 0; i < scannerSize; i++) {
        if (actualPos - i >= 0 && actualPos - i < numLeds) {
            uint8_t brightness = 255 - (i * 255 / scannerSize);
            CRGB scannerColor = color;
            scannerColor.nscale8(brightness);
            leds[actualPos - i] = scannerColor;
        }
    }
    
    effectStep++;
}

// Electrolyte-style Police Effect (red/blue alternating)
void effectPolice(CRGB* leds, uint8_t numLeds) {
    // Calculate police flash speed
    uint16_t flashSpeed = map(effectSpeed, 0, 255, 1000, 100);
    bool flashState = (millis() / flashSpeed) % 2;
    
    for (uint8_t i = 0; i < numLeds; i++) {
        if (flashState) {
            // Red on odd positions, blue on even
            leds[i] = (i % 2) ? CRGB::Red : CRGB::Blue;
        } else {
            // Blue on odd positions, red on even
            leds[i] = (i % 2) ? CRGB::Blue : CRGB::Red;
        }
    }
}

// Electrolyte-style Strobe Effect
void effectStrobe(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Calculate strobe speed
    uint16_t strobeSpeed = map(effectSpeed, 0, 255, 2000, 50);
    bool strobeState = (millis() / strobeSpeed) % 2;
    
    if (strobeState) {
        fill_solid(leds, numLeds, color);
    } else {
        fill_solid(leds, numLeds, CRGB::Black);
    }
}

// Electrolyte-style Larson Scanner Effect
void effectLarsonScanner(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Fade all LEDs
    for (uint8_t i = 0; i < numLeds; i++) {
        leds[i].nscale8(220); // Fade by 14%
    }
    
    // Calculate scanner speed and size
    uint8_t scannerSize = map(effectSpeed, 0, 255, 1, 4);
    uint8_t scannerPos = (effectStep / 3) % ((numLeds + scannerSize) * 2);
    
    // Determine direction
    bool forward = (scannerPos < (numLeds + scannerSize));
    uint8_t actualPos = forward ? scannerPos : ((numLeds + scannerSize) * 2) - scannerPos - 1;
    
    // Draw scanner with fade
    for (uint8_t i = 0; i < scannerSize; i++) {
        if (actualPos - i >= 0 && actualPos - i < numLeds) {
            uint8_t brightness = 255 - (i * 200 / scannerSize);
            CRGB scannerColor = color;
            scannerColor.nscale8(brightness);
            leds[actualPos - i] = scannerColor;
        }
    }
    
    effectStep++;
}

// Electrolyte-style Color Wipe Effect
void effectColorWipe(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Calculate wipe speed
    uint16_t wipeSpeed = map(effectSpeed, 0, 255, 2000, 200);
    uint8_t wipePos = (millis() / wipeSpeed) % (numLeds * 2);
    
    // Determine direction
    bool forward = (wipePos < numLeds);
    uint8_t actualPos = forward ? wipePos : (numLeds * 2) - wipePos - 1;
    
    // Clear all LEDs
    fill_solid(leds, numLeds, CRGB::Black);
    
    // Fill up to position
    for (uint8_t i = 0; i <= actualPos && i < numLeds; i++) {
        leds[i] = color;
    }
}

// Electrolyte-style Theater Chase Effect
void effectTheaterChase(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Calculate chase speed
    uint16_t chaseSpeed = map(effectSpeed, 0, 255, 1000, 100);
    uint8_t chaseStep = (millis() / chaseSpeed) % 3;
    
    for (uint8_t i = 0; i < numLeds; i++) {
        if ((i + chaseStep) % 3 == 0) {
            leds[i] = color;
        } else {
            leds[i] = CRGB::Black;
        }
    }
}

// Electrolyte-style Running Lights Effect
void effectRunningLights(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Calculate running speed
    uint16_t runSpeed = map(effectSpeed, 0, 255, 2000, 200);
    uint8_t runPos = (millis() / runSpeed) % numLeds;
    
    // Clear all LEDs
    fill_solid(leds, numLeds, CRGB::Black);
    
    // Create running light pattern
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t pos = (runPos + i) % numLeds;
        uint8_t brightness = 255 - (i * 85); // Fade each light
        CRGB runColor = color;
        runColor.nscale8(brightness);
        leds[pos] = runColor;
    }
}

// Electrolyte-style Color Sweep Effect
void effectColorSweep(CRGB* leds, uint8_t numLeds, CRGB color) {
    // Calculate sweep speed
    uint16_t sweepSpeed = map(effectSpeed, 0, 255, 3000, 300);
    uint8_t sweepPos = (millis() / sweepSpeed) % (numLeds * 2);
    
    // Determine direction
    bool forward = (sweepPos < numLeds);
    uint8_t actualPos = forward ? sweepPos : (numLeds * 2) - sweepPos - 1;
    
    // Create sweep pattern
    for (uint8_t i = 0; i < numLeds; i++) {
        uint8_t distance = abs(i - actualPos);
        if (distance < 5) {
            uint8_t brightness = 255 - (distance * 50);
            CRGB sweepColor = color;
            sweepColor.nscale8(brightness);
            leds[i] = sweepColor;
        } else {
            leds[i] = CRGB::Black;
        }
    }
}

void setPreset(uint8_t preset) {
    currentPreset = preset;
    
    switch (preset) {
        case PRESET_STANDARD:
            headlightColor = CRGB::White;
            taillightColor = CRGB::Red;
            headlightEffect = FX_SOLID;
            taillightEffect = FX_SOLID;
            globalBrightness = 200;
            Serial.println("Standard Mode");
            break;
            
        case PRESET_NIGHT:
            headlightColor = CRGB::White;
            taillightColor = CRGB::Red;
            headlightEffect = FX_SOLID;
            taillightEffect = FX_BREATH;
            globalBrightness = 255;
            Serial.println("Night Mode");
            break;
            
        case PRESET_PARTY:
            headlightColor = CRGB::White;
            taillightColor = CRGB::Black;
            headlightEffect = FX_SOLID;
            taillightEffect = FX_RAINBOW;
            globalBrightness = 180;
            Serial.println("Party Mode");
            break;
            
        case PRESET_STEALTH:
            headlightColor = CRGB(50, 50, 50);
            taillightColor = CRGB(20, 0, 0);
            headlightEffect = FX_SOLID;
            taillightEffect = FX_SOLID;
            globalBrightness = 50;
            Serial.println("Stealth Mode");
            break;
    }
    
    FastLED.setBrightness(globalBrightness);
}

// Startup Sequence Implementation
void startStartupSequence() {
    startupActive = true;
    startupStartTime = millis();
    startupStep = 0;
    Serial.printf("🎬 Starting %s sequence...\n", getStartupSequenceName(startupSequence).c_str());
}

void updateStartupSequence() {
    if (!startupActive) return;
    
    unsigned long elapsed = millis() - startupStartTime;
    
    // Check if sequence should end
    if (elapsed >= startupDuration) {
        startupActive = false;
        // Set final colors
        fill_solid(headlight, headlightLedCount, headlightColor);
        fill_solid(taillight, taillightLedCount, taillightColor);
        Serial.println("✅ Startup sequence complete!");
        return;
    }
    
    // Update sequence based on type
    switch (startupSequence) {
        case STARTUP_POWER_ON:
            startupPowerOn();
            break;
        case STARTUP_SCAN:
            startupScan();
            break;
        case STARTUP_WAVE:
            startupWave();
            break;
        case STARTUP_RACE:
            startupRace();
            break;
        case STARTUP_CUSTOM:
            startupCustom();
            break;
    }
    
    startupStep++;
}

void startupPowerOn() {
    // Progressive power-on effect - LEDs turn on from center outward
    uint8_t progress = map(millis() - startupStartTime, 0, startupDuration, 0, 255);
    
    // Headlight - center outward
    uint8_t headlightCenter = headlightLedCount / 2;
    uint8_t headlightRadius = map(progress, 0, 255, 0, headlightCenter);
    
    fill_solid(headlight, headlightLedCount, CRGB::Black);
    for (uint8_t i = 0; i < headlightLedCount; i++) {
        uint8_t distance = abs(i - headlightCenter);
        if (distance <= headlightRadius) {
            uint8_t brightness = map(distance, 0, headlightRadius, 255, 100);
            CRGB color = headlightColor;
            color.nscale8(brightness);
            headlight[i] = color;
        }
    }
    
    // Taillight - center outward
    uint8_t taillightCenter = taillightLedCount / 2;
    uint8_t taillightRadius = map(progress, 0, 255, 0, taillightCenter);
    
    fill_solid(taillight, taillightLedCount, CRGB::Black);
    for (uint8_t i = 0; i < taillightLedCount; i++) {
        uint8_t distance = abs(i - taillightCenter);
        if (distance <= taillightRadius) {
            uint8_t brightness = map(distance, 0, taillightRadius, 255, 100);
            CRGB color = taillightColor;
            color.nscale8(brightness);
            taillight[i] = color;
        }
    }
}

void startupScan() {
    // KITT-style scanner effect
    uint16_t scanSpeed = startupDuration / 4; // 4 scans total
    uint8_t scanPhase = (millis() - startupStartTime) / scanSpeed;
    uint8_t scanPos = (millis() - startupStartTime) % scanSpeed;
    
    // Headlight scanner
    fill_solid(headlight, headlightLedCount, CRGB::Black);
    uint8_t headlightPos = map(scanPos, 0, scanSpeed, 0, headlightLedCount * 2);
    if (headlightPos >= headlightLedCount) headlightPos = (headlightLedCount * 2) - headlightPos - 1;
    
    for (uint8_t i = 0; i < 3; i++) {
        if (headlightPos - i >= 0 && headlightPos - i < headlightLedCount) {
            uint8_t brightness = 255 - (i * 85);
            CRGB color = headlightColor;
            color.nscale8(brightness);
            headlight[headlightPos - i] = color;
        }
    }
    
    // Taillight scanner
    fill_solid(taillight, taillightLedCount, CRGB::Black);
    uint8_t taillightPos = map(scanPos, 0, scanSpeed, 0, taillightLedCount * 2);
    if (taillightPos >= taillightLedCount) taillightPos = (taillightLedCount * 2) - taillightPos - 1;
    
    for (uint8_t i = 0; i < 3; i++) {
        if (taillightPos - i >= 0 && taillightPos - i < taillightLedCount) {
            uint8_t brightness = 255 - (i * 85);
            CRGB color = taillightColor;
            color.nscale8(brightness);
            taillight[taillightPos - i] = color;
        }
    }
}

void startupWave() {
    // Wave effect that builds up
    uint8_t progress = map(millis() - startupStartTime, 0, startupDuration, 0, 255);
    uint8_t waveCount = map(progress, 0, 255, 1, 4);
    
    // Headlight wave
    fill_solid(headlight, headlightLedCount, CRGB::Black);
    for (uint8_t wave = 0; wave < waveCount; wave++) {
        uint16_t wavePos = (startupStep * 2 + wave * (headlightLedCount / waveCount)) % (headlightLedCount * 2);
        if (wavePos >= headlightLedCount) wavePos = (headlightLedCount * 2) - wavePos - 1;
        
        for (uint8_t i = 0; i < 5; i++) {
            if (wavePos - i >= 0 && wavePos - i < headlightLedCount) {
                uint8_t brightness = 255 - (i * 50);
                CRGB color = headlightColor;
                color.nscale8(brightness);
                headlight[wavePos - i] = color;
            }
        }
    }
    
    // Taillight wave
    fill_solid(taillight, taillightLedCount, CRGB::Black);
    for (uint8_t wave = 0; wave < waveCount; wave++) {
        uint16_t wavePos = (startupStep * 2 + wave * (taillightLedCount / waveCount)) % (taillightLedCount * 2);
        if (wavePos >= taillightLedCount) wavePos = (taillightLedCount * 2) - wavePos - 1;
        
        for (uint8_t i = 0; i < 5; i++) {
            if (wavePos - i >= 0 && wavePos - i < taillightLedCount) {
                uint8_t brightness = 255 - (i * 50);
                CRGB color = taillightColor;
                color.nscale8(brightness);
                taillight[wavePos - i] = color;
            }
        }
    }
}

void startupRace() {
    // Racing lights effect - LEDs chase around the strip
    uint16_t raceSpeed = startupDuration / 6; // 6 laps total
    uint8_t racePos = (millis() - startupStartTime) % raceSpeed;
    
    // Headlight race
    fill_solid(headlight, headlightLedCount, CRGB::Black);
    uint8_t headlightPos = map(racePos, 0, raceSpeed, 0, headlightLedCount);
    
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t pos = (headlightPos + i) % headlightLedCount;
        uint8_t brightness = 255 - (i * 60);
        CRGB color = headlightColor;
        color.nscale8(brightness);
        headlight[pos] = color;
    }
    
    // Taillight race
    fill_solid(taillight, taillightLedCount, CRGB::Black);
    uint8_t taillightPos = map(racePos, 0, raceSpeed, 0, taillightLedCount);
    
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t pos = (taillightPos + i) % taillightLedCount;
        uint8_t brightness = 255 - (i * 60);
        CRGB color = taillightColor;
        color.nscale8(brightness);
        taillight[pos] = color;
    }
}

void startupCustom() {
    // Custom sequence - rainbow fade-in with breathing effect
    uint8_t progress = map(millis() - startupStartTime, 0, startupDuration, 0, 255);
    
    // Breathing effect
    uint8_t breathe = (sin(millis() / 200.0) + 1) * 127;
    
    // Headlight - rainbow fade-in
    for (uint8_t i = 0; i < headlightLedCount; i++) {
        uint8_t hue = (i * 255 / headlightLedCount) + (startupStep * 2);
        CRGB color = CHSV(hue, 255, breathe);
        headlight[i] = color;
    }
    
    // Taillight - rainbow fade-in
    for (uint8_t i = 0; i < taillightLedCount; i++) {
        uint8_t hue = (i * 255 / taillightLedCount) + (startupStep * 2);
        CRGB color = CHSV(hue, 255, breathe);
        taillight[i] = color;
    }
}

String getStartupSequenceName(uint8_t sequence) {
    switch (sequence) {
        case STARTUP_NONE: return "None";
        case STARTUP_POWER_ON: return "Power On";
        case STARTUP_SCAN: return "Scanner";
        case STARTUP_WAVE: return "Wave";
        case STARTUP_RACE: return "Race";
        case STARTUP_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toLowerCase();
        
        if (command.startsWith("p")) {
            uint8_t preset = command.substring(1).toInt();
            if (preset <= 3) {
                setPreset(preset);
            }
        }
        else if (command.startsWith("b")) {
            uint8_t brightness = command.substring(1).toInt();
            globalBrightness = brightness;
            FastLED.setBrightness(brightness);
            Serial.printf("Brightness set to %d\n", brightness);
        }
        else if (command.startsWith("h")) {
            uint32_t colorHex = strtol(command.substring(1).c_str(), NULL, 16);
            headlightColor = CRGB((colorHex >> 16) & 0xFF, (colorHex >> 8) & 0xFF, colorHex & 0xFF);
            Serial.printf("Headlight color set to 0x%06X\n", colorHex);
        }
        else if (command.startsWith("t")) {
            uint32_t colorHex = strtol(command.substring(1).c_str(), NULL, 16);
            taillightColor = CRGB((colorHex >> 16) & 0xFF, (colorHex >> 8) & 0xFF, colorHex & 0xFF);
            Serial.printf("Taillight color set to 0x%06X\n", colorHex);
        }
        else if (command.startsWith("eh")) {
            uint8_t effect = command.substring(2).toInt();
            if (effect <= 5) {
                headlightEffect = effect;
                Serial.printf("Headlight effect set to %d\n", effect);
            }
        }
        else if (command.startsWith("et")) {
            uint8_t effect = command.substring(2).toInt();
            if (effect <= 5) {
                taillightEffect = effect;
                Serial.printf("Taillight effect set to %d\n", effect);
            }
        }
        else if (command.startsWith("startup")) {
            uint8_t sequence = command.substring(7).toInt();
            if (sequence <= 5) {
                startupSequence = sequence;
                startupEnabled = (sequence != STARTUP_NONE);
                Serial.printf("Startup sequence set to %d (%s)\n", sequence, getStartupSequenceName(sequence).c_str());
            }
        }
        else if (command == "test_startup") {
            startStartupSequence();
            Serial.println("Testing startup sequence...");
        }
        else if (command == "status") {
            printStatus();
        }
        else if (command == "help") {
            printHelp();
        }
        else {
            Serial.println("Unknown command. Type 'help' for available commands.");
        }
    }
}

void printStatus() {
    Serial.println("=== ArkLights Status ===");
    Serial.printf("Preset: %d\n", currentPreset);
    Serial.printf("Brightness: %d\n", globalBrightness);
    Serial.printf("Headlight: Effect %d, Color 0x%06X\n", headlightEffect, headlightColor);
    Serial.printf("Taillight: Effect %d, Color 0x%06X\n", taillightEffect, taillightColor);
    Serial.printf("Startup: %s (%d), Duration: %dms\n", getStartupSequenceName(startupSequence).c_str(), startupSequence, startupDuration);
}

void printHelp() {
    Serial.println("Available commands:");
    Serial.println("  p0-p3: Set preset (0=Standard, 1=Night, 2=Party, 3=Stealth)");
    Serial.println("  b<0-255>: Set brightness");
    Serial.println("  h<hex>: Set headlight color (e.g., hFF0000)");
    Serial.println("  t<hex>: Set taillight color (e.g., t00FF00)");
    Serial.println("  eh<0-19>: Set headlight effect");
    Serial.println("  et<0-19>: Set taillight effect");
    Serial.println("  startup<0-5>: Set startup sequence");
    Serial.println("  test_startup: Test current startup sequence");
    Serial.println("  status: Show current status");
    Serial.println("  help: Show this help");
    Serial.println("");
    Serial.println("Startup Sequences:");
    Serial.println("  0=None, 1=Power On, 2=Scanner, 3=Wave, 4=Race, 5=Custom");
    Serial.println("");
    Serial.println("Effects: 0=Solid, 1=Breath, 2=Rainbow, 3=Chase, 4=Blink Rainbow, 5=Twinkle");
    Serial.println("         6=Fire, 7=Meteor, 8=Wave, 9=Comet, 10=Candle, 11=Static Rainbow");
    Serial.println("         12=Knight Rider, 13=Police, 14=Strobe, 15=Larson Scanner");
    Serial.println("         16=Color Wipe, 17=Theater Chase, 18=Running Lights, 19=Color Sweep");
}

// Web Server Implementation
void setupWiFiAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str(), apPassword.c_str(), AP_CHANNEL, false, MAX_CONNECTIONS);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("AP IP address: %s\n", IP.toString().c_str());
    Serial.printf("Connect to WiFi: %s\n", apName.c_str());
    Serial.printf("Password: %s\n", apPassword.c_str());
}

void setupWebServer() {
    // Serve the main web page
    server.on("/", handleRoot);
    
    // API endpoints
    server.on("/api", HTTP_POST, handleAPI);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/led-config", HTTP_POST, handleLEDConfig);
    server.on("/api/led-test", HTTP_POST, handleLEDTest);
    
    // Handle CORS preflight requests
    server.on("/api", HTTP_OPTIONS, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server.send(200, "text/plain", "");
    });
    
    server.begin();
    Serial.println("Web server started");
}

void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ArkLights PEV Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a1a; color: #fff; }
        .container { max-width: 600px; margin: 0 auto; }
        .section { background: #2a2a2a; padding: 20px; margin: 10px 0; border-radius: 8px; }
        .preset-btn { background: #4CAF50; color: white; padding: 10px 20px; margin: 5px; border: none; border-radius: 4px; cursor: pointer; }
        .preset-btn:hover { background: #45a049; }
        .control-group { margin: 15px 0; }
        label { display: block; margin-bottom: 5px; }
        input[type="range"] { width: 100%; }
        input[type="color"] { width: 50px; height: 30px; }
        select { padding: 8px; border-radius: 4px; background: #333; color: #fff; border: 1px solid #555; }
        .status { background: #333; padding: 10px; border-radius: 4px; margin: 10px 0; }
        h1 { color: #4CAF50; text-align: center; }
        h2 { color: #81C784; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚗 ArkLights PEV Control</h1>
        
        <div class="section">
            <h2>Presets</h2>
            <button class="preset-btn" onclick="setPreset(0)">Standard</button>
            <button class="preset-btn" onclick="setPreset(1)">Night</button>
            <button class="preset-btn" onclick="setPreset(2)">Party</button>
            <button class="preset-btn" onclick="setPreset(3)">Stealth</button>
        </div>
        
        <div class="section">
            <h2>Brightness</h2>
            <div class="control-group">
                <label>Global Brightness: <span id="brightnessValue">128</span></label>
                <input type="range" id="brightness" min="0" max="255" value="128" onchange="setBrightness(this.value)">
            </div>
        </div>
        
        <div class="section">
            <h2>Effect Speed</h2>
            <div class="control-group">
                <label>Effect Speed: <span id="speedValue">64</span></label>
                <input type="range" id="effectSpeed" min="0" max="255" value="64" onchange="setEffectSpeed(this.value)">
                <small>Higher values = faster effects</small>
            </div>
        </div>
        
        <div class="section">
            <h2>Startup Sequence</h2>
            <div class="control-group">
                <label>Startup Animation:</label>
                <select id="startupSequence" onchange="setStartupSequence(this.value)">
                    <option value="0">None</option>
                    <option value="1">Power On</option>
                    <option value="2">Scanner</option>
                    <option value="3">Wave</option>
                    <option value="4">Race</option>
                    <option value="5">Custom</option>
                </select>
                <small>Animation shown when device powers on</small>
            </div>
            <div class="control-group">
                <label>Duration: <span id="startupDurationValue">3000</span>ms</label>
                <input type="range" id="startupDuration" min="1000" max="10000" value="3000" onchange="setStartupDuration(this.value)">
                <small>How long the startup sequence lasts</small>
            </div>
            <div class="control-group">
                <button onclick="testStartupSequence()" style="background: #ff9800;">Test Startup Sequence</button>
                <small>Preview the current startup animation</small>
            </div>
        </div>
        
        <div class="section">
            <h2>WiFi Configuration</h2>
            <div class="control-group">
                <label>Access Point Name:</label>
                <input type="text" id="apName" value="ARKLIGHTS-AP" maxlength="32" onchange="setAPName(this.value)">
                <small>WiFi network name (max 32 characters)</small>
            </div>
            <div class="control-group">
                <label>Password:</label>
                <input type="password" id="apPassword" value="float420" maxlength="63" onchange="setAPPassword(this.value)">
                <small>WiFi password (8-63 characters)</small>
            </div>
            <div class="control-group">
                <button onclick="applyWiFiConfig()" class="btn btn-primary">Apply WiFi Settings</button>
                <small>⚠️ Changes require restart to take effect</small>
            </div>
        </div>
        
        <div class="section">
            <h2>Headlight</h2>
            <div class="control-group">
                <label>Color:</label>
                <input type="color" id="headlightColor" value="#ffffff" onchange="setHeadlightColor(this.value)">
            </div>
            <div class="control-group">
                <label>Effect:</label>
                <select id="headlightEffect" onchange="setHeadlightEffect(this.value)">
                    <option value="0">Solid</option>
                    <option value="1">Breath</option>
                    <option value="2">Rainbow</option>
                    <option value="3">Chase</option>
                    <option value="4">Blink Rainbow</option>
                    <option value="5">Twinkle</option>
                    <option value="6">Fire</option>
                    <option value="7">Meteor</option>
                    <option value="8">Wave</option>
                    <option value="9">Comet</option>
                    <option value="10">Candle</option>
                    <option value="11">Static Rainbow</option>
                    <option value="12">Knight Rider</option>
                    <option value="13">Police</option>
                    <option value="14">Strobe</option>
                    <option value="15">Larson Scanner</option>
                    <option value="16">Color Wipe</option>
                    <option value="17">Theater Chase</option>
                    <option value="18">Running Lights</option>
                    <option value="19">Color Sweep</option>
                </select>
            </div>
        </div>
        
        <div class="section">
            <h2>Taillight</h2>
            <div class="control-group">
                <label>Color:</label>
                <input type="color" id="taillightColor" value="#ff0000" onchange="setTaillightColor(this.value)">
            </div>
            <div class="control-group">
                <label>Effect:</label>
                <select id="taillightEffect" onchange="setTaillightEffect(this.value)">
                    <option value="0">Solid</option>
                    <option value="1">Breath</option>
                    <option value="2">Rainbow</option>
                    <option value="3">Chase</option>
                    <option value="4">Blink Rainbow</option>
                    <option value="5">Twinkle</option>
                    <option value="6">Fire</option>
                    <option value="7">Meteor</option>
                    <option value="8">Wave</option>
                    <option value="9">Comet</option>
                    <option value="10">Candle</option>
                    <option value="11">Static Rainbow</option>
                    <option value="12">Knight Rider</option>
                    <option value="13">Police</option>
                    <option value="14">Strobe</option>
                    <option value="15">Larson Scanner</option>
                    <option value="16">Color Wipe</option>
                    <option value="17">Theater Chase</option>
                    <option value="18">Running Lights</option>
                    <option value="19">Color Sweep</option>
                </select>
            </div>
        </div>
        
        <div class="section">
            <h2>LED Configuration</h2>
            <div class="control-group">
                <label>Headlight LED Count:</label>
                <input type="number" id="headlightLedCount" min="1" max="200" value="20" onchange="updateLEDConfig()">
            </div>
            <div class="control-group">
                <label>Taillight LED Count:</label>
                <input type="number" id="taillightLedCount" min="1" max="200" value="20" onchange="updateLEDConfig()">
            </div>
            <div class="control-group">
                <label>Headlight LED Type:</label>
                <select id="headlightLedType" onchange="updateLEDConfig()">
                    <option value="0">SK6812 (RGBW)</option>
                    <option value="1">WS2812B (RGB)</option>
                    <option value="2">APA102 (RGB)</option>
                    <option value="3">LPD8806 (RGB)</option>
                </select>
            </div>
            <div class="control-group">
                <label>Taillight LED Type:</label>
                <select id="taillightLedType" onchange="updateLEDConfig()">
                    <option value="0">SK6812 (RGBW)</option>
                    <option value="1">WS2812B (RGB)</option>
                    <option value="2">APA102 (RGB)</option>
                    <option value="3">LPD8806 (RGB)</option>
                </select>
            </div>
            <div class="control-group">
                <label>Headlight Color Order:</label>
                <select id="headlightColorOrder" onchange="updateLEDConfig()">
                    <option value="0">GRB</option>
                    <option value="1">RGB</option>
                    <option value="2">BGR</option>
                </select>
            </div>
            <div class="control-group">
                <label>Taillight Color Order:</label>
                <select id="taillightColorOrder" onchange="updateLEDConfig()">
                    <option value="0">GRB</option>
                    <option value="1">RGB</option>
                    <option value="2">BGR</option>
                </select>
            </div>
            <div class="control-group">
                <button onclick="testLEDs()" style="background: #ff9800;">Test LEDs (Red→Green→Blue→White)</button>
                <button onclick="saveLEDConfig()" style="background: #4CAF50;">Save Configuration</button>
            </div>
        </div>
        
        <div class="section">
            <h2>Status</h2>
            <div class="status" id="status">Loading...</div>
            <button onclick="updateStatus()">Refresh Status</button>
        </div>
    </div>
    
    <script>
        function setPreset(preset) {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ preset: preset })
            }).then(() => updateStatus());
        }
        
        function setBrightness(value) {
            document.getElementById('brightnessValue').textContent = value;
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ brightness: parseInt(value) })
            });
        }
        
        function setEffectSpeed(value) {
            document.getElementById('speedValue').textContent = value;
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ effectSpeed: parseInt(value) })
            });
        }
        
        function setStartupSequence(sequence) {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ startup_sequence: parseInt(sequence) })
            });
        }
        
        function setStartupDuration(duration) {
            document.getElementById('startupDurationValue').textContent = duration;
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ startup_duration: parseInt(duration) })
            });
        }
        
        function testStartupSequence() {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ testStartup: true })
            }).then(() => {
                console.log('Startup sequence test started');
            });
        }
        
        function setAPName(name) {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ apName: name })
            });
        }
        
        function setAPPassword(password) {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ apPassword: password })
            });
        }
        
        function applyWiFiConfig() {
            const name = document.getElementById('apName').value;
            const password = document.getElementById('apPassword').value;
            
            if (name.length < 1 || name.length > 32) {
                alert('AP Name must be 1-32 characters');
                return;
            }
            
            if (password.length < 8 || password.length > 63) {
                alert('Password must be 8-63 characters');
                return;
            }
            
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ 
                    apName: name,
                    apPassword: password,
                    restart: true 
                })
            }).then(() => {
                alert('WiFi settings saved! Device will restart in 3 seconds...');
                setTimeout(() => {
                    window.location.reload();
                }, 3000);
            });
        }
        
        function setHeadlightColor(color) {
            const hex = color.replace('#', '');
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ headlightColor: hex })
            });
        }
        
        function setTaillightColor(color) {
            const hex = color.replace('#', '');
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ taillightColor: hex })
            });
        }
        
        function setHeadlightEffect(effect) {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ headlightEffect: parseInt(effect) })
            });
        }
        
        function setTaillightEffect(effect) {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ taillightEffect: parseInt(effect) })
            });
        }
        
        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').innerHTML = 
                        `Preset: ${data.preset}<br>` +
                        `Brightness: ${data.brightness}<br>` +
                        `Effect Speed: ${data.effectSpeed}<br>` +
                        `Startup: ${data.startup_sequence_name} (${data.startup_duration}ms)<br>` +
                        `WiFi AP: ${data.apName}<br>` +
                        `Headlight: Effect ${data.headlightEffect}, Color #${data.headlightColor}<br>` +
                        `Taillight: Effect ${data.taillightEffect}, Color #${data.taillightColor}<br>` +
                        `Headlight Config: ${data.headlightLedCount} LEDs, Type ${data.headlightLedType}, Order ${data.headlightColorOrder}<br>` +
                        `Taillight Config: ${data.taillightLedCount} LEDs, Type ${data.taillightLedType}, Order ${data.taillightColorOrder}`;

                    // Update UI elements
                    document.getElementById('brightness').value = data.brightness;
                    document.getElementById('brightnessValue').textContent = data.brightness;
                    document.getElementById('effectSpeed').value = data.effectSpeed;
                    document.getElementById('speedValue').textContent = data.effectSpeed;
                    document.getElementById('startupSequence').value = data.startup_sequence;
                    document.getElementById('startupDuration').value = data.startup_duration;
                    document.getElementById('startupDurationValue').textContent = data.startup_duration;
                    document.getElementById('apName').value = data.apName;
                    document.getElementById('apPassword').value = data.apPassword;
                    document.getElementById('headlightColor').value = '#' + data.headlightColor;
                    document.getElementById('taillightColor').value = '#' + data.taillightColor;
                    document.getElementById('headlightEffect').value = data.headlightEffect;
                    document.getElementById('taillightEffect').value = data.taillightEffect;
                    
                    // Update LED configuration elements
                    document.getElementById('headlightLedCount').value = data.headlightLedCount;
                    document.getElementById('taillightLedCount').value = data.taillightLedCount;
                    document.getElementById('headlightLedType').value = data.headlightLedType;
                    document.getElementById('taillightLedType').value = data.taillightLedType;
                    document.getElementById('headlightColorOrder').value = data.headlightColorOrder;
                    document.getElementById('taillightColorOrder').value = data.taillightColorOrder;
                });
        }
        
        function updateLEDConfig() {
            const config = {
                headlightLedCount: parseInt(document.getElementById('headlightLedCount').value),
                taillightLedCount: parseInt(document.getElementById('taillightLedCount').value),
                headlightLedType: parseInt(document.getElementById('headlightLedType').value),
                taillightLedType: parseInt(document.getElementById('taillightLedType').value),
                headlightColorOrder: parseInt(document.getElementById('headlightColorOrder').value),
                taillightColorOrder: parseInt(document.getElementById('taillightColorOrder').value)
            };
            
            fetch('/api/led-config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(config)
            }).then(() => {
                console.log('LED configuration updated');
                updateStatus();
            });
        }
        
        function testLEDs() {
            fetch('/api/led-test', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' }
            }).then(() => {
                console.log('LED test completed');
            });
        }
        
        function saveLEDConfig() {
            updateLEDConfig();
            alert('LED configuration saved!');
        }
        
        // Update status on page load
        updateStatus();
        
        // Auto-refresh status every 5 seconds
        setInterval(updateStatus, 5000);
    </script>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
}

void handleAPI() {
    if (server.hasArg("plain")) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, server.arg("plain"));
        
        if (doc.containsKey("preset")) {
            setPreset(doc["preset"]);
        }
        if (doc.containsKey("brightness")) {
            globalBrightness = doc["brightness"];
            FastLED.setBrightness(globalBrightness);
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("effectSpeed")) {
            effectSpeed = doc["effectSpeed"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("startup_sequence")) {
            startupSequence = doc["startup_sequence"];
            startupEnabled = (startupSequence != STARTUP_NONE);
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("startup_duration")) {
            startupDuration = doc["startup_duration"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("testStartup") && doc["testStartup"]) {
            startStartupSequence();
        }
        if (doc.containsKey("apName")) {
            apName = doc["apName"].as<String>();
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("apPassword")) {
            apPassword = doc["apPassword"].as<String>();
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("restart") && doc["restart"]) {
            // Restart the device after a delay
            server.send(200, "application/json", "{\"status\":\"restarting\"}");
            delay(1000);
            ESP.restart();
        }
                if (doc.containsKey("headlightColor")) {
                    String colorHex = doc["headlightColor"];
                    uint32_t color = strtol(colorHex.c_str(), NULL, 16);
                    headlightColor = CRGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                    saveSettings(); // Auto-save
                }
                if (doc.containsKey("taillightColor")) {
                    String colorHex = doc["taillightColor"];
                    uint32_t color = strtol(colorHex.c_str(), NULL, 16);
                    taillightColor = CRGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
                    saveSettings(); // Auto-save
                }
        if (doc.containsKey("headlightEffect")) {
            headlightEffect = doc["headlightEffect"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("taillightEffect")) {
            taillightEffect = doc["taillightEffect"];
            saveSettings(); // Auto-save
        }
        
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"No data\"}");
    }
}

void handleStatus() {
    DynamicJsonDocument doc(1024);
    doc["preset"] = currentPreset;
    doc["brightness"] = globalBrightness;
    doc["effectSpeed"] = effectSpeed;
    doc["startup_sequence"] = startupSequence;
    doc["startup_sequence_name"] = getStartupSequenceName(startupSequence);
    doc["startup_duration"] = startupDuration;
    doc["apName"] = apName;
    doc["apPassword"] = apPassword;
    doc["headlightColor"] = String(headlightColor.r, HEX) + String(headlightColor.g, HEX) + String(headlightColor.b, HEX);
    doc["taillightColor"] = String(taillightColor.r, HEX) + String(taillightColor.g, HEX) + String(taillightColor.b, HEX);
    doc["headlightEffect"] = headlightEffect;
    doc["taillightEffect"] = taillightEffect;
    
    // Add LED configuration to status
    doc["headlightLedCount"] = headlightLedCount;
    doc["taillightLedCount"] = taillightLedCount;
    doc["headlightLedType"] = headlightLedType;
    doc["taillightLedType"] = taillightLedType;
    doc["headlightColorOrder"] = headlightColorOrder;
    doc["taillightColorOrder"] = taillightColorOrder;
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    sendJSONResponse(doc);
}

void handleLEDConfig() {
    if (server.hasArg("plain")) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, server.arg("plain"));
        
        bool configChanged = false;
        
        if (doc.containsKey("headlightLedCount")) {
            headlightLedCount = doc["headlightLedCount"];
            configChanged = true;
        }
        if (doc.containsKey("taillightLedCount")) {
            taillightLedCount = doc["taillightLedCount"];
            configChanged = true;
        }
        if (doc.containsKey("headlightLedType")) {
            headlightLedType = doc["headlightLedType"];
            configChanged = true;
        }
        if (doc.containsKey("taillightLedType")) {
            taillightLedType = doc["taillightLedType"];
            configChanged = true;
        }
        if (doc.containsKey("headlightColorOrder")) {
            headlightColorOrder = doc["headlightColorOrder"];
            configChanged = true;
        }
        if (doc.containsKey("taillightColorOrder")) {
            taillightColorOrder = doc["taillightColorOrder"];
            configChanged = true;
        }
        
        if (configChanged) {
            saveSettings();
            initializeLEDs();
            Serial.println("LED configuration updated and applied!");
        }
        
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"No data\"}");
    }
}

void handleLEDTest() {
    testLEDConfiguration();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"test_complete\"}");
}

void sendJSONResponse(DynamicJsonDocument& doc) {
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// LED Configuration Implementation
// RGBW controllers for SK6812 RGBW LEDs
typedef WS2812<HEADLIGHT_PIN, RGB> HeadlightControllerT;
typedef WS2812<TAILLIGHT_PIN, RGB> TaillightControllerT;

static RGBWEmulatedController<HeadlightControllerT, GRB> headlightRGBWController(Rgbw(kRGBWDefaultColorTemp, kRGBWExactColors, W3));
static RGBWEmulatedController<TaillightControllerT, GRB> taillightRGBWController(Rgbw(kRGBWDefaultColorTemp, kRGBWExactColors, W3));

void initializeLEDs() {
    // Allocate memory for LED arrays
    headlight = new CRGB[headlightLedCount];
    taillight = new CRGB[taillightLedCount];
    
    // Clear FastLED
    FastLED.clear();
    
    // Add LED strips based on configuration
    switch (headlightLedType) {
        case 0: // SK6812 RGBW - Use RGBW emulation
            FastLED.addLeds(&headlightRGBWController, headlight, headlightLedCount);
            break;
        case 1: // WS2812B
            if (headlightColorOrder == 0) FastLED.addLeds<WS2812B, HEADLIGHT_PIN, GRB>(headlight, headlightLedCount);
            else if (headlightColorOrder == 1) FastLED.addLeds<WS2812B, HEADLIGHT_PIN, RGB>(headlight, headlightLedCount);
            else FastLED.addLeds<WS2812B, HEADLIGHT_PIN, BGR>(headlight, headlightLedCount);
            break;
        case 2: // APA102
            if (headlightColorOrder == 0) FastLED.addLeds<APA102, HEADLIGHT_PIN, HEADLIGHT_CLOCK_PIN, GRB>(headlight, headlightLedCount);
            else if (headlightColorOrder == 1) FastLED.addLeds<APA102, HEADLIGHT_PIN, HEADLIGHT_CLOCK_PIN, RGB>(headlight, headlightLedCount);
            else FastLED.addLeds<APA102, HEADLIGHT_PIN, HEADLIGHT_CLOCK_PIN, BGR>(headlight, headlightLedCount);
            break;
        case 3: // LPD8806
            if (headlightColorOrder == 0) FastLED.addLeds<LPD8806, HEADLIGHT_PIN, HEADLIGHT_CLOCK_PIN, GRB>(headlight, headlightLedCount);
            else if (headlightColorOrder == 1) FastLED.addLeds<LPD8806, HEADLIGHT_PIN, HEADLIGHT_CLOCK_PIN, RGB>(headlight, headlightLedCount);
            else FastLED.addLeds<LPD8806, HEADLIGHT_PIN, HEADLIGHT_CLOCK_PIN, BGR>(headlight, headlightLedCount);
            break;
    }
    
    switch (taillightLedType) {
        case 0: // SK6812 RGBW - Use RGBW emulation
            FastLED.addLeds(&taillightRGBWController, taillight, taillightLedCount);
            break;
        case 1: // WS2812B
            if (taillightColorOrder == 0) FastLED.addLeds<WS2812B, TAILLIGHT_PIN, GRB>(taillight, taillightLedCount);
            else if (taillightColorOrder == 1) FastLED.addLeds<WS2812B, TAILLIGHT_PIN, RGB>(taillight, taillightLedCount);
            else FastLED.addLeds<WS2812B, TAILLIGHT_PIN, BGR>(taillight, taillightLedCount);
            break;
        case 2: // APA102
            if (taillightColorOrder == 0) FastLED.addLeds<APA102, TAILLIGHT_PIN, TAILLIGHT_CLOCK_PIN, GRB>(taillight, taillightLedCount);
            else if (taillightColorOrder == 1) FastLED.addLeds<APA102, TAILLIGHT_PIN, TAILLIGHT_CLOCK_PIN, RGB>(taillight, taillightLedCount);
            else FastLED.addLeds<APA102, TAILLIGHT_PIN, TAILLIGHT_CLOCK_PIN, BGR>(taillight, taillightLedCount);
            break;
        case 3: // LPD8806
            if (taillightColorOrder == 0) FastLED.addLeds<LPD8806, TAILLIGHT_PIN, TAILLIGHT_CLOCK_PIN, GRB>(taillight, taillightLedCount);
            else if (taillightColorOrder == 1) FastLED.addLeds<LPD8806, TAILLIGHT_PIN, TAILLIGHT_CLOCK_PIN, RGB>(taillight, taillightLedCount);
            else FastLED.addLeds<LPD8806, TAILLIGHT_PIN, TAILLIGHT_CLOCK_PIN, BGR>(taillight, taillightLedCount);
            break;
    }
    
    FastLED.setBrightness(globalBrightness);
    Serial.println("LED strips initialized successfully!");
}

void testLEDConfiguration() {
    Serial.println("Testing LED configuration...");
    
    // Test red
    fill_solid(headlight, headlightLedCount, CRGB::Red);
    fill_solid(taillight, taillightLedCount, CRGB::Red);
    FastLED.show();
    delay(1000);
    
    // Test green
    fill_solid(headlight, headlightLedCount, CRGB::Green);
    fill_solid(taillight, taillightLedCount, CRGB::Green);
    FastLED.show();
    delay(1000);
    
    // Test blue
    fill_solid(headlight, headlightLedCount, CRGB::Blue);
    fill_solid(taillight, taillightLedCount, CRGB::Blue);
    FastLED.show();
    delay(1000);
    
    // Test white (using white channel for RGBW LEDs)
    fill_solid(headlight, headlightLedCount, CRGB::White);
    fill_solid(taillight, taillightLedCount, CRGB::White);
    FastLED.show();
    delay(1000);
    
    Serial.println("LED test complete!");
}

String getLEDTypeName(uint8_t type) {
    switch (type) {
        case 0: return "SK6812";
        case 1: return "WS2812B";
        case 2: return "APA102";
        case 3: return "LPD8806";
        default: return "Unknown";
    }
}

String getColorOrderName(uint8_t order) {
    switch (order) {
        case 0: return "GRB";
        case 1: return "RGB";
        case 2: return "BGR";
        default: return "Unknown";
    }
}

// Save all settings to persistent storage
// Initialize SPIFFS filesystem
void initFilesystem() {
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS Mount Failed");
        return;
    }
    Serial.println("✅ SPIFFS Mount Success");
    
    // List existing files for debugging
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    Serial.println("📁 Existing files:");
    while (file) {
        Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

// Save all settings to filesystem
bool saveSettings() {
    DynamicJsonDocument doc(1024);
    
    // Light settings
    doc["headlight_effect"] = headlightEffect;
    doc["taillight_effect"] = taillightEffect;
    doc["headlight_color_r"] = headlightColor.r;
    doc["headlight_color_g"] = headlightColor.g;
    doc["headlight_color_b"] = headlightColor.b;
    doc["taillight_color_r"] = taillightColor.r;
    doc["taillight_color_g"] = taillightColor.g;
    doc["taillight_color_b"] = taillightColor.b;
    doc["global_brightness"] = globalBrightness;
    doc["effect_speed"] = effectSpeed;
    doc["current_preset"] = currentPreset;
    
    // Startup sequence settings
    doc["startup_sequence"] = startupSequence;
    doc["startup_enabled"] = startupEnabled;
    doc["startup_duration"] = startupDuration;
    
    // LED configuration
    doc["headlight_count"] = headlightLedCount;
    doc["taillight_count"] = taillightLedCount;
    doc["headlight_type"] = headlightLedType;
    doc["taillight_type"] = taillightLedType;
    doc["headlight_order"] = headlightColorOrder;
    doc["taillight_order"] = taillightColorOrder;
    
    // WiFi settings
    doc["ap_name"] = apName;
    doc["ap_password"] = apPassword;
    
    // Save to file
    File file = SPIFFS.open("/settings.json", "w");
    if (!file) {
        Serial.println("❌ Failed to open settings.json for writing");
        return false;
    }
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    if (bytesWritten > 0) {
        Serial.printf("✅ Settings saved to filesystem (%d bytes)\n", bytesWritten);
        Serial.printf("Headlight: RGB(%d,%d,%d), Taillight: RGB(%d,%d,%d)\n", 
                      headlightColor.r, headlightColor.g, headlightColor.b,
                      taillightColor.r, taillightColor.g, taillightColor.b);
        return true;
    } else {
        Serial.println("❌ Failed to write settings to filesystem");
        return false;
    }
}

// Load all settings from filesystem
bool loadSettings() {
    File file = SPIFFS.open("/settings.json", "r");
    if (!file) {
        Serial.println("⚠️ No settings file found, using defaults");
        return false;
    }
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.printf("❌ Failed to parse settings.json: %s\n", error.c_str());
        return false;
    }
    
    // Load light settings
    headlightEffect = doc["headlight_effect"] | FX_SOLID;
    taillightEffect = doc["taillight_effect"] | FX_SOLID;
    headlightColor.r = doc["headlight_color_r"] | 0;   // Default: black
    headlightColor.g = doc["headlight_color_g"] | 0;   // Default: black
    headlightColor.b = doc["headlight_color_b"] | 0;   // Default: black
    taillightColor.r = doc["taillight_color_r"] | 0;   // Default: black
    taillightColor.g = doc["taillight_color_g"] | 0;   // Default: black
    taillightColor.b = doc["taillight_color_b"] | 0;   // Default: black
    globalBrightness = doc["global_brightness"] | DEFAULT_BRIGHTNESS;
    effectSpeed = doc["effect_speed"] | 64;
    currentPreset = doc["current_preset"] | PRESET_STANDARD;
    
    // Load startup sequence settings
    startupSequence = doc["startup_sequence"] | STARTUP_POWER_ON;
    startupEnabled = doc["startup_enabled"] | true;
    startupDuration = doc["startup_duration"] | 3000;
    
    // Load LED configuration
    headlightLedCount = doc["headlight_count"] | 11;
    taillightLedCount = doc["taillight_count"] | 11;
    headlightLedType = doc["headlight_type"] | 0;  // SK6812
    taillightLedType = doc["taillight_type"] | 0;  // SK6812
    headlightColorOrder = doc["headlight_order"] | 0;  // GRB
    taillightColorOrder = doc["taillight_order"] | 0;  // GRB
    
    // Load WiFi settings
    apName = doc["ap_name"] | "ARKLIGHTS-AP";
    apPassword = doc["ap_password"] | "float420";
    
    Serial.println("✅ Settings loaded from filesystem:");
    Serial.printf("Headlight: RGB(%d,%d,%d), Taillight: RGB(%d,%d,%d)\n", 
                  headlightColor.r, headlightColor.g, headlightColor.b,
                  taillightColor.r, taillightColor.g, taillightColor.b);
    Serial.printf("Brightness: %d, Speed: %d, Preset: %d\n", 
                  globalBrightness, effectSpeed, currentPreset);
    Serial.printf("Startup: %s (%dms), Enabled: %s\n", 
                  getStartupSequenceName(startupSequence).c_str(), startupDuration, startupEnabled ? "Yes" : "No");
    
    return true;
}

// Test filesystem functionality
void testFilesystem() {
    Serial.println("🧪 Testing Filesystem...");
    
    // Test write
    DynamicJsonDocument testDoc(128);
    testDoc["test_value"] = 123;
    testDoc["timestamp"] = millis();
    
    File file = SPIFFS.open("/test.json", "w");
    if (file) {
        serializeJson(testDoc, file);
        file.close();
        Serial.println("✅ Test file written");
        
        // Test read
        file = SPIFFS.open("/test.json", "r");
        if (file) {
            DynamicJsonDocument readDoc(128);
            DeserializationError error = deserializeJson(readDoc, file);
            file.close();
            
            if (!error) {
                int testValue = readDoc["test_value"];
                Serial.printf("✅ Test file read: %d\n", testValue);
                if (testValue == 123) {
                    Serial.println("✅ Filesystem working correctly!");
                } else {
                    Serial.println("❌ Data corruption detected!");
                }
            } else {
                Serial.println("❌ Failed to parse test file");
            }
        } else {
            Serial.println("❌ Failed to read test file");
        }
    } else {
        Serial.println("❌ Failed to write test file");
    }
}
