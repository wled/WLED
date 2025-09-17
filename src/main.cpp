#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <MPU6050.h>
#include <Update.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

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

// MPU6050 Motion Control Settings
#define MPU_SDA_PIN 5
#define MPU_SCL_PIN 6
MPU6050 mpu;

// Motion control settings
bool motionEnabled = true;
bool blinkerEnabled = true;
bool parkModeEnabled = true;
bool impactDetectionEnabled = true;
float motionSensitivity = 1.0; // 0.5 to 2.0
uint16_t blinkerDelay = 300; // ms before triggering blinker
uint16_t blinkerTimeout = 2000; // ms before turning off blinker
uint8_t parkDetectionAngle = 15; // degrees of tilt for park mode
uint8_t impactThreshold = 3; // G-force threshold for impact detection

// Park mode noise thresholds
float parkAccelNoiseThreshold = 0.05; // G-force deviation from gravity threshold (0.05G = very small movement)
float parkGyroNoiseThreshold = 2.5;  // deg/s threshold for gyro noise (increased to account for MPU drift)
uint16_t parkStationaryTime = 2000;  // ms of stationary time before park mode activates (back to 2 seconds)

// Park mode effect settings
uint8_t parkEffect = FX_BREATH;        // Effect to use in park mode
uint8_t parkEffectSpeed = 64;          // Speed for park mode effect
CRGB parkHeadlightColor = CRGB::Blue;  // Headlight color in park mode
CRGB parkTaillightColor = CRGB::Blue;  // Taillight color in park mode
uint8_t parkBrightness = 128;          // Brightness in park mode

// OTA Update settings
String otaUpdateURL = "";
bool otaInProgress = false;
uint8_t otaProgress = 0;
String otaStatus = "Ready";
String otaError = "";
unsigned long otaStartTime = 0;
String otaFileName = "";
size_t otaFileSize = 0;

// Motion state
bool blinkerActive = false;
int8_t blinkerDirection = 0; // -1 = left, 1 = right, 0 = none
bool parkModeActive = false;
unsigned long lastMotionUpdate = 0;
unsigned long blinkerStartTime = 0;
unsigned long parkStartTime = 0;
unsigned long lastImpactTime = 0;

// Calibration system
bool calibrationMode = false;
bool calibrationComplete = false;
uint8_t calibrationStep = 0;
unsigned long calibrationStartTime = 0;
const unsigned long calibrationTimeout = 30000; // 30 seconds per step

struct CalibrationData {
    float levelAccelX, levelAccelY, levelAccelZ;
    float forwardAccelX, forwardAccelY, forwardAccelZ;
    float backwardAccelX, backwardAccelY, backwardAccelZ;
    float leftAccelX, leftAccelY, leftAccelZ;
    float rightAccelX, rightAccelY, rightAccelZ;
    char forwardAxis = 'X';
    char leftRightAxis = 'Y';
    int forwardSign = 1;
    int leftRightSign = 1;
    bool valid = false;
} calibration;

// Motion data structure
struct MotionData {
    float pitch, roll, yaw;
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
};

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
void listSPIFFSFiles();
void showSettingsFile();
void cleanDuplicateFiles();

// Startup sequence functions
void startStartupSequence();
void updateStartupSequence();
void startupPowerOn();
void startupScan();
void startupWave();
void startupRace();
void startupCustom();
String getStartupSequenceName(uint8_t sequence);

// Motion control functions
void initMotionControl();
MotionData getMotionData();
void updateMotionControl();
void processBlinkers(MotionData& data);
void processParkMode(MotionData& data);
void processImpactDetection(MotionData& data);
void startCalibration();
void captureCalibrationStep(MotionData& data);
void resetCalibration();
void completeCalibration();
float getCalibratedForwardAccel(MotionData& data);
float getCalibratedLeftRightAccel(MotionData& data);
void showBlinkerEffect(int direction);
void showParkEffect();
void showImpactEffect();

// OTA Update functions
void handleOTAUpdate();
void handleOTAStatus();
void handleOTAUpload();
void startOTAUpdate(String url);
void startOTAUpdateFromFile(String filename);
void updateOTAProgress(unsigned int progress, unsigned int total);
void handleOTAError(int error);

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
void handleUI();
void handleUIUpdate();
bool processUIUpdate(const String& updatePath);
void serveEmbeddedUI();
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
    
    // Initialize motion control
    initMotionControl();
    
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
    
    // Update motion control at 20Hz
    if (motionEnabled && millis() - lastMotionUpdate >= 50) {
        updateMotionControl();
        lastMotionUpdate = millis();
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
    
    // Priority 1: Park mode (highest priority - overrides everything)
    if (parkModeActive) {
        showParkEffect();
        return; // Skip normal effects when in park mode
    }
    
    // Priority 2: Blinker effects (medium priority - overrides normal effects)
    if (blinkerActive) {
        showBlinkerEffect(blinkerDirection);
        return; // Skip normal effects when blinkers are active
    }
    
    // Priority 3: Normal effects (lowest priority - default behavior)
    // Restore normal brightness
    FastLED.setBrightness(globalBrightness);
    
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

// Motion Control Implementation
void initMotionControl() {
    Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
    
    mpu.initialize();
    
    if (!mpu.testConnection()) {
        Serial.println("❌ MPU6050 not found! Motion control disabled.");
        motionEnabled = false;
        return;
    }
    
    Serial.println("✅ MPU6050 initialized successfully!");
    
    // Configure MPU6050
    mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
    mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_500);
    mpu.setDLPFMode(MPU6050_DLPF_BW_20);
    
    Serial.println("🎯 Motion control features:");
    Serial.println("  - Auto blinkers based on lean angle");
    Serial.println("  - Park mode when stationary and tilted");
    Serial.println("  - Impact detection for crashes");
    Serial.println("  - Calibration system for orientation independence");
}

MotionData getMotionData() {
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    // Convert raw values to meaningful units
    float accelX = ax / 16384.0; // 2G range
    float accelY = ay / 16384.0;
    float accelZ = az / 16384.0;
    float gyroX = gx / 65.5; // 500 deg/s range
    float gyroY = gy / 65.5;
    float gyroZ = gz / 65.5;
    
    // Calculate pitch and roll from accelerometer
    float pitch = atan2(-accelX, sqrt(accelY * accelY + accelZ * accelZ)) * 180.0 / PI;
    float roll = atan2(accelY, accelZ) * 180.0 / PI;
    float yaw = gyroZ;
    
    return {
        pitch,
        roll,
        yaw,
        accelX,
        accelY,
        accelZ,
        gyroX,
        gyroY,
        gyroZ
    };
}

void updateMotionControl() {
    if (!motionEnabled) return;
    
    MotionData data = getMotionData();
    
    // Handle calibration mode - only show debug info, don't auto-capture
    if (calibrationMode) {
        // Show current motion data for debugging during calibration
        static unsigned long lastCalibrationDebug = 0;
        if (millis() - lastCalibrationDebug >= 1000) { // Update every second
            Serial.printf("Calibration Step %d - Accel: X=%.2f, Y=%.2f, Z=%.2f\n", 
                         calibrationStep + 1, data.accelX, data.accelY, data.accelZ);
            lastCalibrationDebug = millis();
        }
        return;
    }
    
    // Process motion features
    if (blinkerEnabled) {
        processBlinkers(data);
    }
    
    if (parkModeEnabled) {
        processParkMode(data);
    }
    
    if (impactDetectionEnabled) {
        processImpactDetection(data);
    }
}

void processBlinkers(MotionData& data) {
    unsigned long currentTime = millis();
    
    // Use calibrated values if available
    float leftRightAccel = calibration.valid ? getCalibratedLeftRightAccel(data) : data.accelY;
    
    // Detect turn intent based on lateral acceleration
    float turnThreshold = 1.5 * motionSensitivity;
    bool turnIntent = abs(leftRightAccel) > turnThreshold;
    
    if (turnIntent) {
        int direction = (leftRightAccel > 0) ? 1 : -1; // 1 = right, -1 = left
        
        if (!blinkerActive) {
            blinkerActive = true;
            blinkerDirection = direction;
            blinkerStartTime = currentTime;
            Serial.printf("🔄 Blinker activated: %s\n", direction > 0 ? "Right" : "Left");
        }
    } else if (blinkerActive && currentTime - blinkerStartTime > blinkerTimeout) {
        blinkerActive = false;
        blinkerDirection = 0;
        Serial.println("🔄 Blinker deactivated");
    }
    
    // Blinker effect is now handled in updateEffects() with proper priority
}

void processParkMode(MotionData& data) {
    unsigned long currentTime = millis();
    
    // Calculate motion magnitude from accelerometer and gyroscope
    // For acceleration, we want to detect changes from the baseline (gravity)
    // So we calculate the deviation from 1G (gravity)
    float accelMagnitude = sqrt(data.accelX * data.accelX + data.accelY * data.accelY + data.accelZ * data.accelZ);
    float accelDeviation = abs(accelMagnitude - 1.0); // Deviation from 1G (gravity)
    float gyroMagnitude = sqrt(data.gyroX * data.gyroX + data.gyroY * data.gyroY + data.gyroZ * data.gyroZ);
    
    // Convert gyro to deg/s for easier threshold setting
    float gyroDegPerSec = gyroMagnitude;
    
    // Define noise thresholds (adjustable via settings)
    float accelNoiseThreshold = parkAccelNoiseThreshold; // G-force threshold for acceleration noise
    float gyroNoiseThreshold = parkGyroNoiseThreshold;  // deg/s threshold for gyro noise
    
    // Debug output every 2 seconds
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime > 2000) {
        Serial.printf("🔍 Park Debug - Accel: %.3fG (dev: %.3f), Gyro: %.1f°/s, Thresholds: %.3fG, %.1f°/s\n", 
                     accelMagnitude, accelDeviation, gyroDegPerSec, accelNoiseThreshold, gyroNoiseThreshold);
        lastDebugTime = currentTime;
    }
    
    // Check if device is stationary (below noise thresholds)
    // Use acceleration deviation from gravity instead of total magnitude
    bool isStationary = (accelDeviation < accelNoiseThreshold) && (gyroDegPerSec < gyroNoiseThreshold);
    
    if (isStationary) {
        if (!parkModeActive) {
            if (parkStartTime == 0) {
                // Start park mode timer
                parkStartTime = currentTime;
                Serial.printf("🅿️ Starting park timer (stationary detected)\n");
            } else {
                // Check if we've been stationary long enough
                if (currentTime - parkStartTime > parkStationaryTime) {
                    parkModeActive = true;
                    Serial.printf("🅿️ Park mode activated (stationary for %dms)\n", parkStationaryTime);
                    showParkEffect();
                }
            }
        }
    } else {
        // Motion detected - deactivate park mode
        if (parkModeActive) {
            parkModeActive = false;
            parkStartTime = 0;
            Serial.println("🅿️ Park mode deactivated (motion detected)");
        } else if (parkStartTime > 0) {
            // Reset timer if we were counting down but motion detected
            parkStartTime = 0;
            Serial.println("🅿️ Park timer reset (motion detected)");
        }
    }
}

void processImpactDetection(MotionData& data) {
    unsigned long currentTime = millis();
    
    // Calculate total acceleration magnitude
    float accelMagnitude = sqrt(data.accelX * data.accelX + data.accelY * data.accelY + data.accelZ * data.accelZ);
    
    // Convert to G-force (assuming 9.8 m/s² = 1G)
    float gForce = accelMagnitude / 9.8;
    
    // Detect impact (sudden high acceleration)
    if (gForce > impactThreshold && currentTime - lastImpactTime > 1000) {
        lastImpactTime = currentTime;
        Serial.printf("💥 Impact detected! G-force: %.1f\n", gForce);
        showImpactEffect();
    }
}

void showBlinkerEffect(int direction) {
    static bool blinkState = false;
    static unsigned long lastBlinkTime = 0;
    
    if (millis() - lastBlinkTime > 500) { // 500ms blink interval
        blinkState = !blinkState;
        lastBlinkTime = millis();
    }
    
    if (!blinkState) return;
    
    // Calculate which half of each strip to blink
    uint8_t headlightHalf = headlightLedCount / 2;
    uint8_t taillightHalf = taillightLedCount / 2;
    
    // Blink the appropriate half based on direction
    if (direction > 0) { // Right turn
        // Blink right half of headlight, left half of taillight
        for (uint8_t i = headlightHalf; i < headlightLedCount; i++) {
            headlight[i] = CRGB::Yellow;
        }
        for (uint8_t i = 0; i < taillightHalf; i++) {
            taillight[i] = CRGB::Yellow;
        }
    } else { // Left turn
        // Blink left half of headlight, right half of taillight
        for (uint8_t i = 0; i < headlightHalf; i++) {
            headlight[i] = CRGB::Yellow;
        }
        for (uint8_t i = taillightHalf; i < taillightLedCount; i++) {
            taillight[i] = CRGB::Yellow;
        }
    }
}

void showParkEffect() {
    // Apply park mode brightness
    FastLED.setBrightness(parkBrightness);
    
    // Temporarily store original effect speed and set park speed
    uint8_t originalSpeed = effectSpeed;
    effectSpeed = parkEffectSpeed;
    
    // Show configurable park effect
    switch (parkEffect) {
        case FX_SOLID:
            fill_solid(headlight, headlightLedCount, parkHeadlightColor);
            fill_solid(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_BREATH:
            effectBreath(headlight, headlightLedCount, parkHeadlightColor);
            effectBreath(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_RAINBOW:
            effectRainbow(headlight, headlightLedCount);
            effectRainbow(taillight, taillightLedCount);
            break;
        case FX_CHASE:
            effectChase(headlight, headlightLedCount, parkHeadlightColor);
            effectChase(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_BLINK_RAINBOW:
            effectBlinkRainbow(headlight, headlightLedCount);
            effectBlinkRainbow(taillight, taillightLedCount);
            break;
        case FX_TWINKLE:
            effectTwinkle(headlight, headlightLedCount, parkHeadlightColor);
            effectTwinkle(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_FIRE:
            effectFire(headlight, headlightLedCount);
            effectFire(taillight, taillightLedCount);
            break;
        case FX_METEOR:
            effectMeteor(headlight, headlightLedCount, parkHeadlightColor);
            effectMeteor(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_WAVE:
            effectWave(headlight, headlightLedCount, parkHeadlightColor);
            effectWave(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_COMET:
            effectComet(headlight, headlightLedCount, parkHeadlightColor);
            effectComet(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_CANDLE:
            effectCandle(headlight, headlightLedCount);
            effectCandle(taillight, taillightLedCount);
            break;
        case FX_STATIC_RAINBOW:
            effectStaticRainbow(headlight, headlightLedCount);
            effectStaticRainbow(taillight, taillightLedCount);
            break;
        case FX_KNIGHT_RIDER:
            effectKnightRider(headlight, headlightLedCount, parkHeadlightColor);
            effectKnightRider(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_POLICE:
            effectPolice(headlight, headlightLedCount);
            effectPolice(taillight, taillightLedCount);
            break;
        case FX_STROBE:
            effectStrobe(headlight, headlightLedCount, parkHeadlightColor);
            effectStrobe(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_LARSON_SCANNER:
            effectLarsonScanner(headlight, headlightLedCount, parkHeadlightColor);
            effectLarsonScanner(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_COLOR_WIPE:
            effectColorWipe(headlight, headlightLedCount, parkHeadlightColor);
            effectColorWipe(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_THEATER_CHASE:
            effectTheaterChase(headlight, headlightLedCount, parkHeadlightColor);
            effectTheaterChase(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_RUNNING_LIGHTS:
            effectRunningLights(headlight, headlightLedCount, parkHeadlightColor);
            effectRunningLights(taillight, taillightLedCount, parkTaillightColor);
            break;
        case FX_COLOR_SWEEP:
            effectColorSweep(headlight, headlightLedCount, parkHeadlightColor);
            effectColorSweep(taillight, taillightLedCount, parkTaillightColor);
            break;
    }
    
    // Restore original effect speed
    effectSpeed = originalSpeed;
}

void showImpactEffect() {
    // Flash all lights white briefly
    fill_solid(headlight, headlightLedCount, CRGB::White);
    fill_solid(taillight, taillightLedCount, CRGB::White);
    FastLED.show();
    delay(200);
    
    // Restore normal colors
    fill_solid(headlight, headlightLedCount, headlightColor);
    fill_solid(taillight, taillightLedCount, taillightColor);
}

void startCalibration() {
    calibrationMode = true;
    calibrationStep = 0;
    calibrationStartTime = millis();
    calibration.valid = false;
    calibrationComplete = false;
    
    Serial.println("=== MPU6050 CALIBRATION STARTED ===");
    Serial.println("Step 1: Hold device LEVEL and click 'Next Step' button in UI...");
    Serial.println("(Calibration will wait for your input - no automatic progression)");
}

void captureCalibrationStep(MotionData& data) {
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - calibrationStartTime;
    
    // Check for timeout
    if (elapsed > calibrationTimeout) {
        Serial.println("Calibration timeout! Restarting...");
        startCalibration();
        return;
    }
    
    // Debug output
    Serial.printf("Capturing Step %d: Accel X=%.2f, Y=%.2f, Z=%.2f\n", 
                  calibrationStep + 1, data.accelX, data.accelY, data.accelZ);
    
    switch(calibrationStep) {
        case 0: // Level
            calibration.levelAccelX = data.accelX;
            calibration.levelAccelY = data.accelY;
            calibration.levelAccelZ = data.accelZ;
            Serial.println("✅ Level captured. Step 2: Tilt FORWARD and click Next Step...");
            break;
            
        case 1: // Forward
            calibration.forwardAccelX = data.accelX;
            calibration.forwardAccelY = data.accelY;
            calibration.forwardAccelZ = data.accelZ;
            Serial.println("✅ Forward captured. Step 3: Tilt BACKWARD and click Next Step...");
            break;
            
        case 2: // Backward
            calibration.backwardAccelX = data.accelX;
            calibration.backwardAccelY = data.accelY;
            calibration.backwardAccelZ = data.accelZ;
            Serial.println("✅ Backward captured. Step 4: Tilt LEFT and click Next Step...");
            break;
            
        case 3: // Left
            calibration.leftAccelX = data.accelX;
            calibration.leftAccelY = data.accelY;
            calibration.leftAccelZ = data.accelZ;
            Serial.println("✅ Left captured. Step 5: Tilt RIGHT and click Next Step...");
            break;
            
        case 4: // Right
            calibration.rightAccelX = data.accelX;
            calibration.rightAccelY = data.accelY;
            calibration.rightAccelZ = data.accelZ;
            Serial.println("✅ Right captured. Completing calibration...");
            completeCalibration();
            return;
    }
    
    calibrationStep++;
    calibrationStartTime = currentTime; // Reset timer for next step
}

void completeCalibration() {
    // Find the axis with maximum change for forward/backward
    float forwardX = abs(calibration.forwardAccelX - calibration.backwardAccelX);
    float forwardY = abs(calibration.forwardAccelY - calibration.backwardAccelY);
    float forwardZ = abs(calibration.forwardAccelZ - calibration.backwardAccelZ);
    
    if (forwardX > forwardY && forwardX > forwardZ) {
        calibration.forwardAxis = 'X';
        calibration.forwardSign = (calibration.forwardAccelX > calibration.backwardAccelX) ? 1 : -1;
    } else if (forwardY > forwardZ) {
        calibration.forwardAxis = 'Y';
        calibration.forwardSign = (calibration.forwardAccelY > calibration.backwardAccelY) ? 1 : -1;
    } else {
        calibration.forwardAxis = 'Z';
        calibration.forwardSign = (calibration.forwardAccelZ > calibration.backwardAccelZ) ? 1 : -1;
    }
    
    // Find the axis with maximum change for left/right
    float leftRightX = abs(calibration.leftAccelX - calibration.rightAccelX);
    float leftRightY = abs(calibration.leftAccelY - calibration.rightAccelY);
    float leftRightZ = abs(calibration.leftAccelZ - calibration.rightAccelZ);
    
    if (leftRightX > leftRightY && leftRightX > leftRightZ) {
        calibration.leftRightAxis = 'X';
        calibration.leftRightSign = (calibration.leftAccelX > calibration.rightAccelX) ? 1 : -1;
    } else if (leftRightY > leftRightZ) {
        calibration.leftRightAxis = 'Y';
        calibration.leftRightSign = (calibration.leftAccelY > calibration.rightAccelY) ? 1 : -1;
    } else {
        calibration.leftRightAxis = 'Z';
        calibration.leftRightSign = (calibration.leftAccelZ > calibration.rightAccelZ) ? 1 : -1;
    }
    
    calibration.valid = true;
    calibrationMode = false;
    calibrationComplete = true;
    
    Serial.println("=== CALIBRATION COMPLETE ===");
    Serial.printf("Forward axis: %c (sign: %d)\n", calibration.forwardAxis, calibration.forwardSign);
    Serial.printf("Left/Right axis: %c (sign: %d)\n", calibration.leftRightAxis, calibration.leftRightSign);
    
    // Save calibration data to persistent storage
    saveSettings();
    Serial.println("Calibration data saved to filesystem!");
}

void resetCalibration() {
    calibrationComplete = false;
    calibration.valid = false;
    calibrationMode = false;
    
    // Save the reset state to persistent storage
    saveSettings();
    Serial.println("Motion calibration reset and saved to filesystem.");
}

float getCalibratedForwardAccel(MotionData& data) {
    if (!calibration.valid) return data.accelX;
    
    switch(calibration.forwardAxis) {
        case 'X': return data.accelX * calibration.forwardSign;
        case 'Y': return data.accelY * calibration.forwardSign;
        case 'Z': return data.accelZ * calibration.forwardSign;
        default: return data.accelX;
    }
}

float getCalibratedLeftRightAccel(MotionData& data) {
    if (!calibration.valid) return data.accelY;
    
    switch(calibration.leftRightAxis) {
        case 'X': return data.accelX * calibration.leftRightSign;
        case 'Y': return data.accelY * calibration.leftRightSign;
        case 'Z': return data.accelZ * calibration.leftRightSign;
        default: return data.accelY;
    }
}

// OTA Update Implementation
void startOTAUpdate(String url) {
    if (url.isEmpty()) {
        Serial.println("❌ No update URL provided");
        otaStatus = "No URL";
        otaError = "No update URL provided";
        return;
    }
    
    Serial.printf("🔄 Starting OTA update from: %s\n", url.c_str());
    
    otaInProgress = true;
    otaProgress = 0;
    otaStatus = "Downloading";
    otaError = "";
    otaStartTime = millis();
    
    // Show downloading effect on LEDs
    fill_solid(headlight, headlightLedCount, CRGB::Blue);
    fill_solid(taillight, taillightLedCount, CRGB::Blue);
    FastLED.show();
    
    // Start the update process
    httpUpdate.setLedPin(-1); // Disable built-in LED
    httpUpdate.onProgress(updateOTAProgress);
    httpUpdate.onError(handleOTAError);
    
    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, url);
    
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("❌ OTA update failed: %s\n", httpUpdate.getLastErrorString().c_str());
            otaStatus = "Failed";
            otaError = httpUpdate.getLastErrorString();
            otaInProgress = false;
            break;
            
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("ℹ️ No updates available");
            otaStatus = "No Updates";
            otaInProgress = false;
            break;
            
        case HTTP_UPDATE_OK:
            Serial.println("✅ OTA update completed, restarting...");
            otaStatus = "Complete";
            otaProgress = 100;
            // Device will restart automatically
            break;
    }
}

void updateOTAProgress(unsigned int progress, unsigned int total) {
    otaProgress = (progress * 100) / total;
    Serial.printf("📥 OTA Progress: %d%% (%d/%d bytes)\n", otaProgress, progress, total);
    
    // Update LED progress indicator
    uint8_t ledProgress = (progress * headlightLedCount) / total;
    for (uint8_t i = 0; i < headlightLedCount; i++) {
        headlight[i] = (i < ledProgress) ? CRGB::Green : CRGB::Blue;
    }
    for (uint8_t i = 0; i < taillightLedCount; i++) {
        taillight[i] = (i < ledProgress) ? CRGB::Green : CRGB::Blue;
    }
    FastLED.show();
}

void handleOTAError(int error) {
    String errorMsg = "";
    switch (error) {
        case HTTP_UE_TOO_LESS_SPACE:
            errorMsg = "Not enough space";
            break;
        case HTTP_UE_SERVER_NOT_REPORT_SIZE:
            errorMsg = "Server did not report size";
            break;
        case HTTP_UE_SERVER_FILE_NOT_FOUND:
            errorMsg = "File not found on server";
            break;
        case HTTP_UE_SERVER_FORBIDDEN:
            errorMsg = "Server forbidden";
            break;
        case HTTP_UE_SERVER_WRONG_HTTP_CODE:
            errorMsg = "Wrong HTTP code";
            break;
        case HTTP_UE_SERVER_FAULTY_MD5:
            errorMsg = "Faulty MD5";
            break;
        case HTTP_UE_BIN_VERIFY_HEADER_FAILED:
            errorMsg = "Verify header failed";
            break;
        case HTTP_UE_BIN_FOR_WRONG_FLASH:
            errorMsg = "Wrong flash size";
            break;
        default:
            errorMsg = "Unknown error";
            break;
    }
    
    Serial.printf("❌ OTA Error: %s\n", errorMsg.c_str());
    otaStatus = "Error";
    otaError = errorMsg;
    otaInProgress = false;
    
    // Show error on LEDs
    fill_solid(headlight, headlightLedCount, CRGB::Red);
    fill_solid(taillight, taillightLedCount, CRGB::Red);
    FastLED.show();
}

// File Upload Handler for OTA - WLED Style Approach
void handleOTAUpload() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("📁 Starting firmware upload: %s\n", upload.filename.c_str());
        
        // Validate file extension
        if (!upload.filename.endsWith(".bin")) {
            Serial.println("❌ Invalid file type. Only .bin files are allowed.");
            return;
        }
        
        otaFileName = upload.filename;
        otaInProgress = true;
        otaProgress = 0;
        otaStatus = "Uploading";
        otaError = "";
        otaStartTime = millis();
        
        // Start OTA update (WLED style)
        Serial.println("🔄 Starting OTA update");
        size_t freeSpace = ESP.getFreeSketchSpace();
        Serial.printf("💾 Free sketch space: %d bytes\n", freeSpace);
        
        if (!Update.begin((freeSpace - 0x1000) & 0xFFFFF000)) {
            String errorMsg = Update.errorString();
            Serial.printf("❌ OTA begin failed: %s\n", errorMsg.c_str());
            otaStatus = "Begin Failed";
            otaError = errorMsg;
            otaInProgress = false;
            return;
        }
        
        Serial.println("✅ OTA update started successfully");
        
        // Show uploading effect on LEDs
        fill_solid(headlight, headlightLedCount, CRGB::Blue);
        fill_solid(taillight, taillightLedCount, CRGB::Blue);
        FastLED.show();
        
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write data (WLED style - no return value check)
        if (!Update.hasError()) {
            Update.write(upload.buf, upload.currentSize);
            
            // Update progress occasionally
            static size_t lastProgressUpdate = 0;
            if (upload.currentSize - lastProgressUpdate > 50000) { // Every 50KB
                otaProgress = (upload.currentSize * 100) / upload.totalSize;
                Serial.printf("📥 Upload Progress: %d%% (%d/%d bytes)\n", otaProgress, upload.currentSize, upload.totalSize);
                lastProgressUpdate = upload.currentSize;
                
                // Update LED progress indicator
                uint8_t ledProgress = (upload.currentSize * headlightLedCount) / upload.totalSize;
                for (uint8_t i = 0; i < headlightLedCount; i++) {
                    headlight[i] = (i < ledProgress) ? CRGB::Green : CRGB::Blue;
                }
                for (uint8_t i = 0; i < taillightLedCount; i++) {
                    taillight[i] = (i < ledProgress) ? CRGB::Green : CRGB::Blue;
                }
                FastLED.show();
            }
        }
        
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("✅ Upload complete: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
        
        if (Update.end(true)) {
            Serial.println("✅ OTA update completed, restarting...");
            otaStatus = "Complete";
            otaProgress = 100;
            
            // Show success on LEDs
            fill_solid(headlight, headlightLedCount, CRGB::Green);
            fill_solid(taillight, taillightLedCount, CRGB::Green);
            FastLED.show();
            
            // Send success response to client before restart
            server.send(200, "application/json", "{\"success\":true,\"message\":\"Update complete, restarting...\"}");
            
            // Give the client time to receive the response
            delay(1000);
            ESP.restart();
        } else {
            String errorMsg = Update.errorString();
            Serial.printf("❌ OTA end failed: %s\n", errorMsg.c_str());
            otaStatus = "End Failed";
            otaError = errorMsg;
            otaInProgress = false;
            
            // Show error on LEDs
            fill_solid(headlight, headlightLedCount, CRGB::Red);
            fill_solid(taillight, taillightLedCount, CRGB::Red);
            FastLED.show();
        }
    }
}

// Start OTA update from uploaded file
void startOTAUpdateFromFile(String filename) {
    Serial.printf("🔄 Starting OTA update from file: %s\n", filename.c_str());
    
    otaStatus = "Installing";
    otaProgress = 0;
    otaError = "";
    
    // Show installing effect on LEDs
    fill_solid(headlight, headlightLedCount, CRGB::Yellow);
    fill_solid(taillight, taillightLedCount, CRGB::Yellow);
    FastLED.show();
    
    // Start the update process from file
    Update.onProgress(updateOTAProgress);
    
    File file = SPIFFS.open(filename, "r");
    if (!file) {
        Serial.printf("❌ Failed to open file: %s\n", filename.c_str());
        otaStatus = "File Error";
        otaError = "Failed to open uploaded file";
        otaInProgress = false;
        return;
    }
    
    size_t fileSize = file.size();
    Serial.printf("📁 File size: %d bytes\n", fileSize);
    
    // Check if file size is valid
    if (fileSize == 0) {
        Serial.println("❌ File is empty");
        otaStatus = "File Error";
        otaError = "File is empty";
        otaInProgress = false;
        file.close();
        return;
    }
    
    // Check if file size is too large for available space
    size_t freeSpace = ESP.getFreeSketchSpace();
    Serial.printf("💾 Free sketch space: %d bytes\n", freeSpace);
    
    if (fileSize > freeSpace) {
        Serial.printf("❌ File too large: %d > %d\n", fileSize, freeSpace);
        otaStatus = "File Error";
        otaError = "File too large for available space";
        otaInProgress = false;
        file.close();
        return;
    }
    
    if (!Update.begin(fileSize, U_FLASH)) {
        Serial.printf("❌ OTA begin failed: %s\n", Update.errorString());
        otaStatus = "Begin Failed";
        otaError = Update.errorString();
        otaInProgress = false;
        file.close();
        return;
    }
    
    size_t written = Update.writeStream(file);
    file.close();
    
    if (written != fileSize) {
        Serial.printf("❌ OTA write failed: %s\n", Update.errorString());
        otaStatus = "Write Failed";
        otaError = Update.errorString();
        otaInProgress = false;
        return;
    }
    
    if (!Update.end()) {
        Serial.printf("❌ OTA end failed: %s\n", Update.errorString());
        otaStatus = "End Failed";
        otaError = Update.errorString();
        otaInProgress = false;
        return;
    }
    
    Serial.println("✅ OTA update completed, restarting...");
    otaStatus = "Complete";
    otaProgress = 100;
    
    // Show success on LEDs
    fill_solid(headlight, headlightLedCount, CRGB::Green);
    fill_solid(taillight, taillightLedCount, CRGB::Green);
    FastLED.show();
    
    delay(2000);
    ESP.restart();
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
        else if (command == "calibrate" || command == "cal") {
            startCalibration();
            Serial.println("Starting motion calibration...");
        }
        else if (command == "reset_cal") {
            resetCalibration();
            Serial.println("Motion calibration reset");
        }
        else if (command == "motion_on") {
            motionEnabled = true;
            Serial.println("Motion control enabled");
        }
        else if (command == "motion_off") {
            motionEnabled = false;
            Serial.println("Motion control disabled");
        }
        else if (command == "blinker_on") {
            blinkerEnabled = true;
            Serial.println("Auto blinkers enabled");
        }
        else if (command == "blinker_off") {
            blinkerEnabled = false;
            Serial.println("Auto blinkers disabled");
        }
        else if (command == "park_on") {
            parkModeEnabled = true;
            Serial.println("Park mode enabled");
        }
        else if (command == "park_off") {
            parkModeEnabled = false;
            Serial.println("Park mode disabled");
        }
        else if (command.startsWith("park_effect ")) {
            int effect = command.substring(12).toInt();
            if (effect >= 0 && effect <= 19) {
                parkEffect = effect;
                saveSettings();
                Serial.printf("Park effect set to %d\n", effect);
            } else {
                Serial.println("Invalid effect (0-19)");
            }
        }
        else if (command.startsWith("park_speed ")) {
            int speed = command.substring(11).toInt();
            if (speed >= 0 && speed <= 255) {
                parkEffectSpeed = speed;
                saveSettings();
                Serial.printf("Park effect speed set to %d\n", speed);
            } else {
                Serial.println("Invalid speed (0-255)");
            }
        }
        else if (command.startsWith("park_brightness ")) {
            int brightness = command.substring(16).toInt();
            if (brightness >= 0 && brightness <= 255) {
                parkBrightness = brightness;
                saveSettings();
                Serial.printf("Park brightness set to %d\n", brightness);
            } else {
                Serial.println("Invalid brightness (0-255)");
            }
        }
        else if (command.startsWith("park_color ")) {
            String colorStr = command.substring(11);
            if (colorStr.startsWith("headlight ")) {
                String rgbStr = colorStr.substring(10);
                int firstComma = rgbStr.indexOf(',');
                int secondComma = rgbStr.indexOf(',', firstComma + 1);
                if (firstComma > 0 && secondComma > firstComma) {
                    int r = rgbStr.substring(0, firstComma).toInt();
                    int g = rgbStr.substring(firstComma + 1, secondComma).toInt();
                    int b = rgbStr.substring(secondComma + 1).toInt();
                    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                        parkHeadlightColor = CRGB(r, g, b);
                        saveSettings();
                        Serial.printf("Park headlight color set to RGB(%d,%d,%d)\n", r, g, b);
                    } else {
                        Serial.println("Invalid RGB values (0-255)");
                    }
                } else {
                    Serial.println("Invalid format. Use: park_color headlight r,g,b");
                }
            } else if (colorStr.startsWith("taillight ")) {
                String rgbStr = colorStr.substring(10);
                int firstComma = rgbStr.indexOf(',');
                int secondComma = rgbStr.indexOf(',', firstComma + 1);
                if (firstComma > 0 && secondComma > firstComma) {
                    int r = rgbStr.substring(0, firstComma).toInt();
                    int g = rgbStr.substring(firstComma + 1, secondComma).toInt();
                    int b = rgbStr.substring(secondComma + 1).toInt();
                    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                        parkTaillightColor = CRGB(r, g, b);
                        saveSettings();
                        Serial.printf("Park taillight color set to RGB(%d,%d,%d)\n", r, g, b);
                    } else {
                        Serial.println("Invalid RGB values (0-255)");
                    }
                } else {
                    Serial.println("Invalid format. Use: park_color taillight r,g,b");
                }
            } else {
                Serial.println("Usage: park_color headlight r,g,b or park_color taillight r,g,b");
            }
        }
        else if (command == "ota_status") {
            Serial.printf("OTA Status: %s, Progress: %d%%, Error: %s\n", 
                         otaStatus.c_str(), otaProgress, otaError.c_str());
        }
        else if (command == "status") {
            printStatus();
        }
        else if (command == "list_files" || command == "ls") {
            Serial.println("📁 SPIFFS File Listing:");
            listSPIFFSFiles();
        }
        else if (command == "show_settings" || command == "cat_settings") {
            showSettingsFile();
        }
        else if (command == "clean_duplicates") {
            Serial.println("🧹 Cleaning duplicate files...");
            cleanDuplicateFiles();
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
    Serial.println("");
    Serial.println("Motion Control:");
    Serial.println("  calibrate/cal: Start motion calibration");
    Serial.println("  reset_cal: Reset motion calibration");
    Serial.println("  motion_on/off: Enable/disable motion control");
    Serial.println("  blinker_on/off: Enable/disable auto blinkers");
    Serial.println("  park_on/off: Enable/disable park mode");
    Serial.println("  park_effect <0-19>: Set park mode effect");
    Serial.println("  park_speed <0-255>: Set park mode effect speed");
    Serial.println("  park_brightness <0-255>: Set park mode brightness");
    Serial.println("  park_color headlight r,g,b: Set park headlight color");
    Serial.println("  park_color taillight r,g,b: Set park taillight color");
    Serial.println("");
    Serial.println("OTA Updates:");
    Serial.println("  ota_status: Show OTA update status");
    Serial.println("");
    Serial.println("System:");
    Serial.println("  status: Show current status");
    Serial.println("  list_files/ls: List SPIFFS files");
    Serial.println("  show_settings/cat_settings: Display settings.json contents");
    Serial.println("  clean_duplicates: Remove duplicate UI files");
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

void listSPIFFSFiles() {
    Serial.println("📁 SPIFFS File Listing:");
    Serial.println("========================");
    
    File root = SPIFFS.open("/");
    if (!root) {
        Serial.println("❌ Failed to open SPIFFS root directory");
        return;
    }
    
    if (!root.isDirectory()) {
        Serial.println("❌ Root is not a directory");
        root.close();
        return;
    }
    
    File file = root.openNextFile();
    int fileCount = 0;
    size_t totalSize = 0;
    
    while (file) {
        fileCount++;
        totalSize += file.size();
        
        Serial.printf("📄 %-20s %8d bytes", file.name(), file.size());
        
        // Add file type indicator
        String filename = String(file.name());
        if (filename.endsWith(".html") || filename.endsWith(".htm")) {
            Serial.print(" [HTML]");
        } else if (filename.endsWith(".css")) {
            Serial.print(" [CSS]");
        } else if (filename.endsWith(".js")) {
            Serial.print(" [JS]");
        } else if (filename.endsWith(".json")) {
            Serial.print(" [JSON]");
        } else if (filename.endsWith(".txt")) {
            Serial.print(" [TXT]");
        } else if (filename.endsWith(".zip")) {
            Serial.print(" [ZIP]");
        }
        
        Serial.println();
        file = root.openNextFile();
    }
    
    root.close();
    
    Serial.println("========================");
    Serial.printf("📊 Total: %d files, %d bytes\n", fileCount, totalSize);
    
    // Check for UI files specifically
    Serial.println("\n🎨 UI Files Check:");
    String uiFiles[] = {"/ui/index.html", "/ui/styles.css", "/ui/script.js"};
    String rootFiles[] = {"/index.html", "/styles.css", "/script.js"};
    bool allUIFilesExist = true;
    bool hasValidUIFiles = false;
    
    // Check /ui/ directory first
    Serial.println("📁 /ui/ directory:");
    for (int i = 0; i < 3; i++) {
        File uiFile = SPIFFS.open(uiFiles[i], "r");
        if (uiFile) {
            if (uiFile.size() > 0) {
                Serial.printf("✅ %s (%d bytes)\n", uiFiles[i].c_str(), uiFile.size());
                hasValidUIFiles = true;
            } else {
                Serial.printf("⚠️ %s (0 bytes - empty)\n", uiFiles[i].c_str());
            }
            uiFile.close();
        } else {
            Serial.printf("❌ %s (not found)\n", uiFiles[i].c_str());
            allUIFilesExist = false;
        }
    }
    
    // Check root directory as fallback
    Serial.println("📁 Root directory:");
    for (int i = 0; i < 3; i++) {
        File rootFile = SPIFFS.open(rootFiles[i], "r");
        if (rootFile) {
            if (rootFile.size() > 0) {
                Serial.printf("✅ %s (%d bytes)\n", rootFiles[i].c_str(), rootFile.size());
                hasValidUIFiles = true;
            } else {
                Serial.printf("⚠️ %s (0 bytes - empty)\n", rootFiles[i].c_str());
            }
            rootFile.close();
        } else {
            Serial.printf("❌ %s (not found)\n", rootFiles[i].c_str());
        }
    }
    
    if (hasValidUIFiles) {
        Serial.println("🎉 Valid UI files found - external UI should work");
    } else {
        Serial.println("⚠️ No valid UI files found - will use embedded UI fallback");
    }
}

void showSettingsFile() {
    Serial.println("⚙️ Settings.json Contents:");
    Serial.println("===========================");
    
    File file = SPIFFS.open("/settings.json", "r");
    if (!file) {
        Serial.println("❌ settings.json not found");
        return;
    }
    
    // Read and display the entire file
    while (file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println(line);
    }
    
    file.close();
    Serial.println("===========================");
}

void cleanDuplicateFiles() {
    Serial.println("🧹 Cleaning duplicate files...");
    
    // List of files to clean from root directory if they exist in /ui/
    String filesToClean[] = {"index.html", "styles.css", "script.js"};
    int cleanedCount = 0;
    
    for (int i = 0; i < 3; i++) {
        String rootFile = "/" + filesToClean[i];
        String uiFile = "/ui/" + filesToClean[i];
        
        // Check if both files exist
        File rootFileHandle = SPIFFS.open(rootFile, "r");
        File uiFileHandle = SPIFFS.open(uiFile, "r");
        
        if (rootFileHandle && uiFileHandle) {
            Serial.printf("🗑️ Removing duplicate: %s (keeping %s)\n", rootFile.c_str(), uiFile.c_str());
            rootFileHandle.close();
            uiFileHandle.close();
            
            if (SPIFFS.remove(rootFile)) {
                cleanedCount++;
                Serial.printf("✅ Removed: %s\n", rootFile.c_str());
            } else {
                Serial.printf("❌ Failed to remove: %s\n", rootFile.c_str());
            }
        } else {
            if (rootFileHandle) rootFileHandle.close();
            if (uiFileHandle) uiFileHandle.close();
        }
    }
    
    Serial.printf("🧹 Cleanup complete: %d duplicate files removed\n", cleanedCount);
    Serial.println("💡 Use 'ls' command to verify cleanup");
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
    server.on("/ui/styles.css", handleUI);
    server.on("/ui/script.js", handleUI);
    server.on("/styles.css", handleUI);
    server.on("/script.js", handleUI);
    server.on("/updateui", HTTP_GET, handleUIUpdate);
    server.on("/updateui", HTTP_POST, []() {
        server.send(200, "text/plain", "UI update endpoint ready");
    }, handleUIUpdate);
    
    // API endpoints
    server.on("/api", HTTP_POST, handleAPI);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/led-config", HTTP_POST, handleLEDConfig);
    server.on("/api/led-test", HTTP_POST, handleLEDTest);
    server.on("/api/ota-upload", HTTP_POST, []() {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Upload complete\"}");
    }, handleOTAUpload);
    
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

void handleUI() {
    String uri = server.uri();
    String contentType = "text/plain";
    
    Serial.printf("🎨 handleUI: Requesting file: %s\n", uri.c_str());
    
    if (uri.endsWith(".css")) {
        contentType = "text/css";
    } else if (uri.endsWith(".js")) {
        contentType = "application/javascript";
    }
    
    // Try requested path first, then fallback to other location
    String filePath = uri;
    File file = SPIFFS.open(filePath, "r");
    
    if (!file || file.size() == 0) {
        // Try alternative location as fallback
        String altPath;
        if (uri.startsWith("/ui/")) {
            // If requesting /ui/file, try root directory
            altPath = uri.substring(4); // Remove "/ui/" prefix
        } else {
            // If requesting root file, try /ui/ directory
            altPath = "/ui" + uri;
        }
        
        Serial.printf("⚠️ %s not found or empty, trying: %s\n", filePath.c_str(), altPath.c_str());
        file = SPIFFS.open(altPath, "r");
        filePath = altPath;
    }
    
    if (file && file.size() > 0) {
        Serial.printf("✅ handleUI: Found file %s (%d bytes), serving as %s\n", filePath.c_str(), file.size(), contentType.c_str());
        server.streamFile(file, contentType);
        file.close();
    } else {
        Serial.printf("❌ handleUI: File not found or empty: %s\n", filePath.c_str());
        server.send(404, "text/plain", "File not found: " + uri);
    }
}

void handleRoot() {
    // Try to serve external HTML file first, fallback to embedded if not found
    Serial.println("🔍 handleRoot: Attempting to serve /ui/index.html");
    File file = SPIFFS.open("/ui/index.html", "r");
    if (file && file.size() > 0) {
        Serial.printf("✅ Found external UI file (%d bytes), serving from SPIFFS\n", file.size());
        server.streamFile(file, "text/html");
        file.close();
    } else {
        // Try root directory as fallback
        Serial.println("⚠️ /ui/index.html not found or empty, trying /index.html");
        file = SPIFFS.open("/index.html", "r");
        if (file && file.size() > 0) {
            Serial.printf("✅ Found root UI file (%d bytes), serving from SPIFFS\n", file.size());
            server.streamFile(file, "text/html");
            file.close();
        } else {
            Serial.println("⚠️ No external UI file found, serving embedded UI");
            // Fallback to embedded UI if SPIFFS files not found
            serveEmbeddedUI();
        }
    }
}

void serveEmbeddedUI() {
    Serial.println("🎨 serveEmbeddedUI: Serving embedded UI fallback");
    // Embedded HTML as fallback - this will be updated with OTA
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ArkLights PEV Control v8.0 OTA</title>
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
        .warning { background: #ff9800; color: #000; padding: 10px; border-radius: 4px; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ArkLights PEV Control v8.0 OTA</h1>
        <div class="warning">
            ⚠️ Using embedded UI (fallback mode). Upload UI files to SPIFFS for better performance.
        </div>
        <div style="text-align: center; margin: 10px 0; padding: 10px; background: rgba(255,255,255,0.1); border-radius: 8px;">
            <strong>Firmware Version:</strong> v8.0 OTA | <strong>Build Date:</strong> <span id="buildDate">Loading...</span>
        </div>
        
        <!-- Simplified UI for embedded mode -->
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
            <h2>OTA Updates</h2>
            <div class="control-group">
                <label>Firmware File:</label>
                <input type="file" id="otaFileInput" accept=".bin" onchange="handleFileSelect(this)">
                <small>Select firmware binary file (.bin)</small>
            </div>
            <div class="control-group">
                <button onclick="startOTAUpdate()" id="startOTAButton" style="background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px;" disabled>
                    Upload & Install
                </button>
                <small>Upload and install firmware file</small>
            </div>
        </div>
        
        <div class="section">
            <h2>📐 Calibration</h2>
            <div id="calibrationStatus" class="status">
                Status: <span id="calibrationStatusText">Not calibrated</span>
            </div>
            
            <div id="calibrationProgress" style="display: none; margin: 15px 0;">
                <div style="width: 100%; height: 20px; background: #ddd; border-radius: 10px; overflow: hidden;">
                    <div id="calibrationProgressBar" style="height: 100%; background: #4CAF50; width: 0%; transition: width 0.3s;"></div>
                </div>
                <div id="calibrationStepText" style="margin-top: 10px; font-weight: bold;">Step 1: Hold device LEVEL</div>
            </div>
            
            <div style="margin-top: 15px;">
                <button onclick="startCalibration()" id="startCalibrationBtn" style="background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; margin-right: 10px;">Start Calibration</button>
                <button onclick="nextCalibrationStep()" id="nextCalibrationBtn" style="background: #2196F3; color: white; padding: 10px 20px; border: none; border-radius: 5px; margin-right: 10px; display: none;">Next Step</button>
                <button onclick="resetCalibration()" id="resetCalibrationBtn" style="background: #f44336; color: white; padding: 10px 20px; border: none; border-radius: 5px;">Reset Calibration</button>
            </div>
        </div>
        
        <div class="section">
            <h2>🎯 Motion Control</h2>
            <div class="control-group">
                <label>
                    <input type="checkbox" id="motionEnabled" onchange="updateMotionSettings()">
                    Enable Motion Control
                </label>
            </div>
            <div class="control-group">
                <label>
                    <input type="checkbox" id="blinkerEnabled" onchange="updateMotionSettings()">
                    Enable Auto Blinkers
                </label>
            </div>
            <div class="control-group">
                <label>
                    <input type="checkbox" id="parkModeEnabled" onchange="updateMotionSettings()">
                    Enable Park Mode
                </label>
            </div>
            <div class="control-group">
                <label>
                    <input type="checkbox" id="impactDetectionEnabled" onchange="updateMotionSettings()">
                    Enable Impact Detection
                </label>
            </div>
        </div>
        
        <div class="section">
            <h2>🅿️ Park Mode Settings</h2>
            <div class="control-group">
                <label>Park Effect: <span id="parkEffectValue">1</span></label>
                <select id="parkEffect" onchange="updateParkSettings()">
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
            <div class="control-group">
                <label>Park Effect Speed: <span id="parkSpeedValue">64</span></label>
                <input type="range" id="parkSpeed" min="0" max="255" value="64" onchange="updateParkSettings()">
            </div>
            <div class="control-group">
                <label>Park Brightness: <span id="parkBrightnessValue">128</span></label>
                <input type="range" id="parkBrightness" min="0" max="255" value="128" onchange="updateParkSettings()">
            </div>
            <div class="control-group">
                <label>Park Headlight Color:</label>
                <input type="color" id="parkHeadlightColor" value="#0000ff" onchange="updateParkSettings()">
            </div>
            <div class="control-group">
                <label>Park Taillight Color:</label>
                <input type="color" id="parkTaillightColor" value="#0000ff" onchange="updateParkSettings()">
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
        
        function handleFileSelect(input) {
            const file = input.files[0];
            const button = document.getElementById('startOTAButton');
            
            if (file) {
                if (file.name.endsWith('.bin')) {
                    button.disabled = false;
                    button.textContent = `Upload & Install (${(file.size / 1024 / 1024).toFixed(1)}MB)`;
                } else {
                    alert('Please select a .bin file');
                    input.value = '';
                    button.disabled = true;
                    button.textContent = 'Upload & Install';
                }
            } else {
                button.disabled = true;
                button.textContent = 'Upload & Install';
            }
        }
        
        function startOTAUpdate() {
            const fileInput = document.getElementById('otaFileInput');
            const file = fileInput.files[0];
            
            if (!file) {
                alert('Please select a firmware file first');
                return;
            }
            
            if (!confirm('This will restart the device. Continue?')) {
                return;
            }
            
            const formData = new FormData();
            formData.append('firmware', file);
            
            fetch('/api/ota-upload', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    if (data.message && data.message.includes('restarting')) {
                        alert('✅ Firmware update completed successfully! The device is restarting with the new firmware.');
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    }
                } else {
                    alert('Upload failed: ' + (data.error || 'Unknown error'));
                }
            })
            .catch(error => {
                alert('Upload error: ' + error);
            });
        }
        
        function startCalibration() {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ startCalibration: true })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Calibration started:', data);
                document.getElementById('calibrationProgress').style.display = 'block';
                document.getElementById('startCalibrationBtn').style.display = 'none';
                document.getElementById('nextCalibrationBtn').style.display = 'inline-block';
                document.getElementById('calibrationStatusText').textContent = 'In Progress';
                updateStatus();
            })
            .catch(error => {
                console.error('Error starting calibration:', error);
            });
        }
        
        function nextCalibrationStep() {
            const nextBtn = document.getElementById('nextCalibrationBtn');
            nextBtn.disabled = true;
            nextBtn.textContent = 'Capturing...';
            
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ nextCalibrationStep: true })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Next calibration step sent:', data);
                nextBtn.disabled = false;
                nextBtn.textContent = 'Next Step';
                updateStatus();
            })
            .catch(error => {
                console.error('Error sending next calibration step:', error);
                nextBtn.disabled = false;
                nextBtn.textContent = 'Next Step';
            });
        }
        
        function resetCalibration() {
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ resetCalibration: true })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Calibration reset:', data);
                document.getElementById('calibrationProgress').style.display = 'none';
                document.getElementById('startCalibrationBtn').style.display = 'inline-block';
                document.getElementById('nextCalibrationBtn').style.display = 'none';
                document.getElementById('calibrationStatusText').textContent = 'Not calibrated';
                document.getElementById('calibrationProgressBar').style.width = '0%';
                updateStatus();
            })
            .catch(error => {
                console.error('Error resetting calibration:', error);
            });
        }
        
        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').innerHTML = 
                        `Preset: ${data.preset}<br>` +
                        `Brightness: ${data.brightness}<br>` +
                        `Firmware: ${data.firmware_version}<br>` +
                        `Build Date: ${data.build_date}<br>` +
                        `Calibration: ${data.calibration_complete ? 'Complete' : 'Not calibrated'}`;
                    
                    document.getElementById('brightness').value = data.brightness;
                    document.getElementById('brightnessValue').textContent = data.brightness;
                    document.getElementById('buildDate').textContent = data.build_date || 'Unknown';
                    
                    // Update motion control settings
                    document.getElementById('motionEnabled').checked = data.motion_enabled;
                    document.getElementById('blinkerEnabled').checked = data.blinker_enabled;
                    document.getElementById('parkModeEnabled').checked = data.park_mode_enabled;
                    document.getElementById('impactDetectionEnabled').checked = data.impact_detection_enabled;
                    
                    // Update park mode settings
                    document.getElementById('parkEffect').value = data.park_effect;
                    document.getElementById('parkEffectValue').textContent = data.park_effect;
                    document.getElementById('parkSpeed').value = data.park_effect_speed;
                    document.getElementById('parkSpeedValue').textContent = data.park_effect_speed;
                    document.getElementById('parkBrightness').value = data.park_brightness;
                    document.getElementById('parkBrightnessValue').textContent = data.park_brightness;
                    document.getElementById('parkHeadlightColor').value = rgbToHex(data.park_headlight_color_r, data.park_headlight_color_g, data.park_headlight_color_b);
                    document.getElementById('parkTaillightColor').value = rgbToHex(data.park_taillight_color_r, data.park_taillight_color_g, data.park_taillight_color_b);
                    
                    // Update calibration UI
                    if (data.calibration_mode) {
                        console.log('Calibration mode active, step:', data.calibration_step);
                        document.getElementById('calibrationProgress').style.display = 'block';
                        document.getElementById('startCalibrationBtn').style.display = 'none';
                        document.getElementById('nextCalibrationBtn').style.display = 'inline-block';
                        document.getElementById('calibrationStatusText').textContent = 'In Progress';
                        
                        const currentStep = data.calibration_step;
                        const progress = ((currentStep + 1) / 5) * 100;
                        document.getElementById('calibrationProgressBar').style.width = progress + '%';
                        
                        const stepTexts = [
                            'Hold device LEVEL',
                            'Tilt FORWARD', 
                            'Tilt BACKWARD',
                            'Tilt LEFT',
                            'Tilt RIGHT'
                        ];
                        const currentStepNumber = data.calibration_step + 1;
                        const stepIndex = Math.min(data.calibration_step, 4);
                        const stepDescription = stepTexts[stepIndex] || 'Calibrating...';
                        
                        document.getElementById('calibrationStepText').textContent = `Step ${currentStepNumber}/5: ${stepDescription}`;
                        console.log('Updated UI - Step:', currentStepNumber, 'Progress:', progress + '%');
                    } else {
                        document.getElementById('calibrationProgress').style.display = 'none';
                        document.getElementById('startCalibrationBtn').style.display = 'inline-block';
                        document.getElementById('nextCalibrationBtn').style.display = 'none';
                        document.getElementById('calibrationStatusText').textContent = data.calibration_complete ? 'Complete' : 'Not calibrated';
                    }
                });
        }
        
        function updateMotionSettings() {
            const motionEnabled = document.getElementById('motionEnabled').checked;
            const blinkerEnabled = document.getElementById('blinkerEnabled').checked;
            const parkModeEnabled = document.getElementById('parkModeEnabled').checked;
            const impactDetectionEnabled = document.getElementById('impactDetectionEnabled').checked;
            
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    motion_enabled: motionEnabled,
                    blinker_enabled: blinkerEnabled,
                    park_mode_enabled: parkModeEnabled,
                    impact_detection_enabled: impactDetectionEnabled
                })
            });
        }
        
        function updateParkSettings() {
            const parkEffect = document.getElementById('parkEffect').value;
            const parkSpeed = document.getElementById('parkSpeed').value;
            const parkBrightness = document.getElementById('parkBrightness').value;
            const parkHeadlightColor = document.getElementById('parkHeadlightColor').value;
            const parkTaillightColor = document.getElementById('parkTaillightColor').value;
            
            // Update display values
            document.getElementById('parkEffectValue').textContent = parkEffect;
            document.getElementById('parkSpeedValue').textContent = parkSpeed;
            document.getElementById('parkBrightnessValue').textContent = parkBrightness;
            
            // Convert hex colors to RGB
            const headlightRGB = hexToRgb(parkHeadlightColor);
            const taillightRGB = hexToRgb(parkTaillightColor);
            
            fetch('/api', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    park_effect: parseInt(parkEffect),
                    park_effect_speed: parseInt(parkSpeed),
                    park_brightness: parseInt(parkBrightness),
                    park_headlight_color_r: headlightRGB.r,
                    park_headlight_color_g: headlightRGB.g,
                    park_headlight_color_b: headlightRGB.b,
                    park_taillight_color_r: taillightRGB.r,
                    park_taillight_color_g: taillightRGB.g,
                    park_taillight_color_b: taillightRGB.b
                })
            });
        }
        
        function hexToRgb(hex) {
            const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
            return result ? {
                r: parseInt(result[1], 16),
                g: parseInt(result[2], 16),
                b: parseInt(result[3], 16)
            } : {r: 0, g: 0, b: 0};
        }
        
        function rgbToHex(r, g, b) {
            return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
        }
        
        updateStatus();
        setInterval(updateStatus, 5000);
    </script>
</body>
</html>
)rawliteral";
    
    Serial.printf("📤 serveEmbeddedUI: Sending HTML response (%d bytes)\n", html.length());
    server.send(200, "text/html", html);
    Serial.println("✅ serveEmbeddedUI: Response sent successfully");
}

void handleUIUpdate() {
    if (server.method() == HTTP_GET) {
        // Serve UI update page
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ArkLights UI Update</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a2e; color: white; }
        .container { max-width: 600px; margin: 0 auto; }
        .control { margin: 20px 0; padding: 20px; background: rgba(255,255,255,0.1); border-radius: 8px; }
        button { padding: 12px 24px; margin: 8px; background: #667eea; color: white; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #764ba2; }
        .status { padding: 15px; margin: 15px 0; border-radius: 8px; }
        .success { background: rgba(76,175,80,0.2); border: 1px solid rgba(76,175,80,0.5); }
        .error { background: rgba(244,67,54,0.2); border: 1px solid rgba(244,67,54,0.5); }
        .info { background: rgba(33,150,243,0.2); border: 1px solid rgba(33,150,243,0.5); }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎨 ArkLights UI Update</h1>
        <div class="control">
            <h3>Update Interface Files</h3>
            <p>Upload a ZIP file containing updated UI files. This will update the web interface without requiring a full firmware update.</p>
            
            <form id="updateForm" enctype="multipart/form-data">
                <input type="file" id="uiFile" accept=".zip,.txt" required>
                <button type="submit">Update UI</button>
            </form>
            
            <div id="status" style="display: none;"></div>
        </div>
        
        <div class="control">
            <h3>Current UI Files</h3>
            <p>Files that can be updated:</p>
            <ul>
                <li><strong>Main Interface:</strong></li>
                <li>ui/index.html - Main ArkLights interface</li>
                <li>ui/styles.css - ArkLights stylesheet</li>
                <li>ui/script.js - ArkLights JavaScript</li>
                <li><strong>Custom Files:</strong></li>
                <li>Any custom CSS/JS/HTML files</li>
            </ul>
            <p><em>Note: Filesystem versions override embedded versions. If a file doesn't exist in filesystem, the embedded version is used.</em></p>
        </div>
        
        <div class="control">
            <button onclick="window.location.href='/'">Back to Main Interface</button>
        </div>
    </div>

    <script>
        document.getElementById('updateForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const fileInput = document.getElementById('uiFile');
            const statusDiv = document.getElementById('status');
            
            if (!fileInput.files[0]) {
                showStatus('Please select a ZIP or TXT file', 'error');
                return;
            }
            
            const formData = new FormData();
            formData.append('uiupdate', fileInput.files[0]);
            
            showStatus('Uploading and updating UI files...', 'info');
            
            fetch('/updateui', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(data => {
                if (data.includes('success')) {
                    showStatus('UI update successful! The interface has been updated.', 'success');
                } else {
                    showStatus('Update failed: ' + data, 'error');
                }
            })
            .catch(error => {
                showStatus('Upload failed: ' + error.message, 'error');
            });
        });
        
        function showStatus(message, type) {
            const statusDiv = document.getElementById('status');
            statusDiv.innerHTML = message;
            statusDiv.className = 'status ' + type;
            statusDiv.style.display = 'block';
        }
    </script>
</body>
</html>
        )rawliteral";
        
        server.send(200, "text/html", html);
    } else if (server.method() == HTTP_POST) {
        // Handle file upload
        HTTPUpload& upload = server.upload();
        
        static File updateFile;
        static String updatePath;
        
        if (upload.status == UPLOAD_FILE_START) {
            updatePath = "/ui_update_" + String(millis()) + ".zip";
            updateFile = SPIFFS.open(updatePath, "w");
            Serial.printf("Starting UI update: %s\n", updatePath.c_str());
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (updateFile) {
                updateFile.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (updateFile) {
                updateFile.close();
                Serial.println("UI update file received, processing...");
                
                if (processUIUpdate(updatePath)) {
                    server.send(200, "text/plain", "UI update successful!");
                    SPIFFS.remove(updatePath);
                } else {
                    server.send(500, "text/plain", "UI update failed - could not process files");
                    SPIFFS.remove(updatePath);
                }
            }
        }
    }
}

bool processUIUpdate(const String& updatePath) {
    Serial.printf("Processing UI update: %s\n", updatePath.c_str());
    
    File updateFile = SPIFFS.open(updatePath, "r");
    if (!updateFile) {
        Serial.println("Failed to open update file");
        return false;
    }
    
    String content = updateFile.readString();
    updateFile.close();
    
    // Process text-based update format: FILENAME:CONTENT:ENDFILE
    if (content.indexOf("FILENAME:") == 0) {
        Serial.println("Processing text-based UI update");
        
        int pos = 0;
        while (pos < content.length()) {
            int filenameStart = content.indexOf("FILENAME:", pos);
            if (filenameStart == -1) break;
            
            int filenameEnd = content.indexOf(":", filenameStart + 9);
            if (filenameEnd == -1) break;
            
            String filename = content.substring(filenameStart + 9, filenameEnd);
            
            int contentStart = filenameEnd + 1;
            int contentEnd = content.indexOf(":ENDFILE", contentStart);
            if (contentEnd == -1) break;
            
            String fileContent = content.substring(contentStart, contentEnd);
            
            // Ensure filename starts with / (avoid double slashes)
            if (filename.charAt(0) != '/') {
                filename = "/" + filename;
            }
            
            Serial.printf("Updating file: %s\n", filename.c_str());
            
            // Write the file
            File targetFile = SPIFFS.open(filename, "w");
            if (targetFile) {
                targetFile.print(fileContent);
                targetFile.close();
                Serial.printf("Successfully updated: %s\n", filename.c_str());
            } else {
                Serial.printf("Failed to write file: %s\n", filename.c_str());
            }
            
            pos = contentEnd + 8; // Move past :ENDFILE
        }
        
        Serial.println("UI update completed successfully");
        return true;
    }
    
    // For ZIP files, we'd need a ZIP library - for now just return success
    Serial.println("ZIP update format not yet implemented - use text format");
    return false;
}

// Old handleRoot function removed - now using external UI files
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
        if (doc.containsKey("testParkMode") && doc["testParkMode"]) {
            // Temporarily activate park mode for testing
            parkModeActive = true;
            parkStartTime = millis(); // Reset timer
            Serial.println("🅿️ Test park mode activated");
        }
        
        // Motion control API
        if (doc.containsKey("motion_enabled")) {
            motionEnabled = doc["motion_enabled"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("blinker_enabled")) {
            blinkerEnabled = doc["blinker_enabled"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_mode_enabled")) {
            parkModeEnabled = doc["park_mode_enabled"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("impact_detection_enabled")) {
            impactDetectionEnabled = doc["impact_detection_enabled"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("motion_sensitivity")) {
            motionSensitivity = doc["motion_sensitivity"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("blinker_delay")) {
            blinkerDelay = doc["blinker_delay"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("blinker_timeout")) {
            blinkerTimeout = doc["blinker_timeout"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_detection_angle")) {
            parkDetectionAngle = doc["park_detection_angle"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_stationary_time")) {
            parkStationaryTime = doc["park_stationary_time"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_accel_noise_threshold")) {
            parkAccelNoiseThreshold = doc["park_accel_noise_threshold"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_gyro_noise_threshold")) {
            parkGyroNoiseThreshold = doc["park_gyro_noise_threshold"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_effect")) {
            parkEffect = doc["park_effect"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_effect_speed")) {
            parkEffectSpeed = doc["park_effect_speed"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_headlight_color_r")) {
            parkHeadlightColor.r = doc["park_headlight_color_r"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_headlight_color_g")) {
            parkHeadlightColor.g = doc["park_headlight_color_g"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_headlight_color_b")) {
            parkHeadlightColor.b = doc["park_headlight_color_b"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_taillight_color_r")) {
            parkTaillightColor.r = doc["park_taillight_color_r"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_taillight_color_g")) {
            parkTaillightColor.g = doc["park_taillight_color_g"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_taillight_color_b")) {
            parkTaillightColor.b = doc["park_taillight_color_b"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("park_brightness")) {
            parkBrightness = doc["park_brightness"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("impact_threshold")) {
            impactThreshold = doc["impact_threshold"];
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("startCalibration") && doc["startCalibration"]) {
            startCalibration();
        }
        if (doc.containsKey("resetCalibration") && doc["resetCalibration"]) {
            resetCalibration();
        }
        if (doc.containsKey("nextCalibrationStep") && doc["nextCalibrationStep"]) {
            if (calibrationMode) {
                MotionData data = getMotionData();
                captureCalibrationStep(data);
            }
        }
        
        // OTA Update API
        if (doc.containsKey("otaUpdateURL")) {
            otaUpdateURL = doc["otaUpdateURL"].as<String>();
            saveSettings(); // Auto-save
        }
        if (doc.containsKey("startOTAUpdate") && doc["startOTAUpdate"]) {
            if (!otaUpdateURL.isEmpty()) {
                startOTAUpdate(otaUpdateURL);
            }
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
    
    // Motion control status
    doc["motion_enabled"] = motionEnabled;
    doc["blinker_enabled"] = blinkerEnabled;
    doc["park_mode_enabled"] = parkModeEnabled;
    doc["impact_detection_enabled"] = impactDetectionEnabled;
    doc["motion_sensitivity"] = motionSensitivity;
    doc["blinker_delay"] = blinkerDelay;
    doc["blinker_timeout"] = blinkerTimeout;
    doc["park_detection_angle"] = parkDetectionAngle;
    doc["impact_threshold"] = impactThreshold;
    doc["park_accel_noise_threshold"] = parkAccelNoiseThreshold;
    doc["park_gyro_noise_threshold"] = parkGyroNoiseThreshold;
    doc["park_stationary_time"] = parkStationaryTime;
    doc["park_effect"] = parkEffect;
    doc["park_effect_speed"] = parkEffectSpeed;
    doc["park_headlight_color_r"] = parkHeadlightColor.r;
    doc["park_headlight_color_g"] = parkHeadlightColor.g;
    doc["park_headlight_color_b"] = parkHeadlightColor.b;
    doc["park_taillight_color_r"] = parkTaillightColor.r;
    doc["park_taillight_color_g"] = parkTaillightColor.g;
    doc["park_taillight_color_b"] = parkTaillightColor.b;
    doc["park_brightness"] = parkBrightness;
    doc["blinker_active"] = blinkerActive;
    doc["blinker_direction"] = blinkerDirection;
    doc["park_mode_active"] = parkModeActive;
    doc["calibration_complete"] = calibrationComplete;
    doc["calibration_mode"] = calibrationMode;
    doc["calibration_step"] = calibrationStep;
    
    // OTA Update status
    doc["ota_update_url"] = otaUpdateURL;
    doc["ota_in_progress"] = otaInProgress;
    doc["ota_progress"] = otaProgress;
    doc["ota_status"] = otaStatus;
    doc["ota_error"] = otaError;
    doc["ota_file_name"] = otaFileName;
    doc["ota_file_size"] = otaFileSize;
    doc["firmware_version"] = "v8.0 OTA";
    doc["build_date"] = __DATE__ " " __TIME__;
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
    
    // Motion control settings
    doc["motion_enabled"] = motionEnabled;
    doc["blinker_enabled"] = blinkerEnabled;
    doc["park_mode_enabled"] = parkModeEnabled;
    doc["impact_detection_enabled"] = impactDetectionEnabled;
    doc["motion_sensitivity"] = motionSensitivity;
    doc["blinker_delay"] = blinkerDelay;
    doc["blinker_timeout"] = blinkerTimeout;
    doc["park_detection_angle"] = parkDetectionAngle;
    doc["impact_threshold"] = impactThreshold;
    doc["park_accel_noise_threshold"] = parkAccelNoiseThreshold;
    doc["park_gyro_noise_threshold"] = parkGyroNoiseThreshold;
    doc["park_stationary_time"] = parkStationaryTime;
    
    // Park mode effect settings
    doc["park_effect"] = parkEffect;
    doc["park_effect_speed"] = parkEffectSpeed;
    doc["park_headlight_color_r"] = parkHeadlightColor.r;
    doc["park_headlight_color_g"] = parkHeadlightColor.g;
    doc["park_headlight_color_b"] = parkHeadlightColor.b;
    doc["park_taillight_color_r"] = parkTaillightColor.r;
    doc["park_taillight_color_g"] = parkTaillightColor.g;
    doc["park_taillight_color_b"] = parkTaillightColor.b;
    doc["park_brightness"] = parkBrightness;
    
    // OTA Update settings
    doc["ota_update_url"] = otaUpdateURL;
    
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
    
    // Calibration data
    doc["calibration_complete"] = calibrationComplete;
    doc["calibration_valid"] = calibration.valid;
    doc["calibration_forward_axis"] = String(calibration.forwardAxis);
    doc["calibration_forward_sign"] = calibration.forwardSign;
    doc["calibration_leftright_axis"] = String(calibration.leftRightAxis);
    doc["calibration_leftright_sign"] = calibration.leftRightSign;
    doc["calibration_level_x"] = calibration.levelAccelX;
    doc["calibration_level_y"] = calibration.levelAccelY;
    doc["calibration_level_z"] = calibration.levelAccelZ;
    doc["calibration_forward_x"] = calibration.forwardAccelX;
    doc["calibration_forward_y"] = calibration.forwardAccelY;
    doc["calibration_forward_z"] = calibration.forwardAccelZ;
    doc["calibration_backward_x"] = calibration.backwardAccelX;
    doc["calibration_backward_y"] = calibration.backwardAccelY;
    doc["calibration_backward_z"] = calibration.backwardAccelZ;
    doc["calibration_left_x"] = calibration.leftAccelX;
    doc["calibration_left_y"] = calibration.leftAccelY;
    doc["calibration_left_z"] = calibration.leftAccelZ;
    doc["calibration_right_x"] = calibration.rightAccelX;
    doc["calibration_right_y"] = calibration.rightAccelY;
    doc["calibration_right_z"] = calibration.rightAccelZ;
    
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
    
    // Load motion control settings
    motionEnabled = doc["motion_enabled"] | true;
    blinkerEnabled = doc["blinker_enabled"] | true;
    parkModeEnabled = doc["park_mode_enabled"] | true;
    impactDetectionEnabled = doc["impact_detection_enabled"] | true;
    motionSensitivity = doc["motion_sensitivity"] | 1.0;
    blinkerDelay = doc["blinker_delay"] | 300;
    blinkerTimeout = doc["blinker_timeout"] | 2000;
    parkDetectionAngle = doc["park_detection_angle"] | 15;
    impactThreshold = doc["impact_threshold"] | 3;
    parkAccelNoiseThreshold = doc["park_accel_noise_threshold"] | 0.05;
    parkGyroNoiseThreshold = doc["park_gyro_noise_threshold"] | 2.5;
    parkStationaryTime = doc["park_stationary_time"] | 2000;
    
    // Load park mode effect settings
    parkEffect = doc["park_effect"] | FX_BREATH;
    parkEffectSpeed = doc["park_effect_speed"] | 64;
    parkHeadlightColor.r = doc["park_headlight_color_r"] | 0;
    parkHeadlightColor.g = doc["park_headlight_color_g"] | 0;
    parkHeadlightColor.b = doc["park_headlight_color_b"] | 255;
    parkTaillightColor.r = doc["park_taillight_color_r"] | 0;
    parkTaillightColor.g = doc["park_taillight_color_g"] | 0;
    parkTaillightColor.b = doc["park_taillight_color_b"] | 255;
    parkBrightness = doc["park_brightness"] | 128;
    
    // Load OTA Update settings
    otaUpdateURL = doc["ota_update_url"] | "";
    
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
    
    // Load calibration data
    calibrationComplete = doc["calibration_complete"] | false;
    calibration.valid = doc["calibration_valid"] | false;
    if (calibration.valid) {
        String forwardAxisStr = doc["calibration_forward_axis"] | "X";
        calibration.forwardAxis = forwardAxisStr.charAt(0);
        calibration.forwardSign = doc["calibration_forward_sign"] | 1;
        String leftRightAxisStr = doc["calibration_leftright_axis"] | "Y";
        calibration.leftRightAxis = leftRightAxisStr.charAt(0);
        calibration.leftRightSign = doc["calibration_leftright_sign"] | 1;
        calibration.levelAccelX = doc["calibration_level_x"] | 0.0;
        calibration.levelAccelY = doc["calibration_level_y"] | 0.0;
        calibration.levelAccelZ = doc["calibration_level_z"] | 1.0;
        calibration.forwardAccelX = doc["calibration_forward_x"] | 0.0;
        calibration.forwardAccelY = doc["calibration_forward_y"] | 0.0;
        calibration.forwardAccelZ = doc["calibration_forward_z"] | 1.0;
        calibration.backwardAccelX = doc["calibration_backward_x"] | 0.0;
        calibration.backwardAccelY = doc["calibration_backward_y"] | 0.0;
        calibration.backwardAccelZ = doc["calibration_backward_z"] | 1.0;
        calibration.leftAccelX = doc["calibration_left_x"] | 0.0;
        calibration.leftAccelY = doc["calibration_left_y"] | 0.0;
        calibration.leftAccelZ = doc["calibration_left_z"] | 1.0;
        calibration.rightAccelX = doc["calibration_right_x"] | 0.0;
        calibration.rightAccelY = doc["calibration_right_y"] | 0.0;
        calibration.rightAccelZ = doc["calibration_right_z"] | 1.0;
        
        Serial.println("✅ Calibration data loaded from filesystem:");
        Serial.printf("Forward axis: %c (sign: %d)\n", calibration.forwardAxis, calibration.forwardSign);
        Serial.printf("Left/Right axis: %c (sign: %d)\n", calibration.leftRightAxis, calibration.leftRightSign);
    }
    
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
    
    // List all files in SPIFFS
    Serial.println("📁 SPIFFS file listing:");
    File root = SPIFFS.open("/");
    if (root) {
        File file = root.openNextFile();
        while (file) {
            Serial.printf("  📄 %s (%d bytes)\n", file.name(), file.size());
            file = root.openNextFile();
        }
        root.close();
    }
    
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
