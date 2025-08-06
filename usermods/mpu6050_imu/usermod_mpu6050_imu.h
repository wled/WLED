#pragma once

#include "wled.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define SDA_PIN 5
#define SCL_PIN 6
#define USERMOD_IMU_NAME "MPU6050"

class MPU6050Driver : public Usermod {
  private:
    Adafruit_MPU6050 mpu;

    // User-configurable settings
    bool blinkerEnabled = true;
    bool parkModeEnabled = true;
    bool directionSwapEnabled = false;
    bool invertRoll = false;
    bool debugMode = false;
    int turnDetection = 15;             // Degrees of roll for initial detection
    float yawThreshold = 25.0;          // deg/s for angular velocity
    float accelThreshold = 0.15;        // g's for lateral acceleration
    float forwardThreshold = 0.3;       // g's for forward/backward detection
    unsigned long blinkerDelay = 300;   // Milliseconds before triggering blinker
    unsigned long endBlinkerDelay = 2000; // Longer delay for better turn completion
    unsigned long turnMomentumTime = 1500; // Time to maintain turn signal momentum

    // Park FX settings
    uint8_t parkFXMode = FX_MODE_BLINK_RAINBOW;
    uint8_t parkFXSpeed = 200;
    uint32_t parkFXColor = 0x0000FF;

    // Enhanced state tracking
    bool wasBlinkerActive = false;
    bool isStationary = false;
    bool parkModeActive = false;
    bool isMovingForward = true; // Direction detection
    unsigned long turnStartTime = 0;
    unsigned long parkStartTime = 0;
    unsigned long endTurnStartTime = 0;
    unsigned long lastDirectionChange = 0;

    // Saved FX state
    uint8_t prevFXMode[2];
    uint8_t prevFXSpeed[2];
    uint32_t prevFXColor[2];

    // Blinker timing
    bool blinkState = false;
    unsigned long lastBlinkTime = 0;
    const unsigned long blinkInterval = 500; // ms

    const float stationaryGyroThreshold = 0.08; // deg/s
    const int parkDetection = 12;              // degrees of pitch

    // Enhanced sensor data with filtering
    float lastPitch = 0;
    float lastRoll = 0;
    float lastYaw = 0;
    float filteredRoll = 0;
    float filteredYaw = 0;
    float filteredAccelX = 0;
    float filteredAccelY = 0;
    
    // Turn detection intelligence
    float rollMomentum = 0;         // Track roll momentum
    float yawMomentum = 0;          // Track yaw momentum
    bool turnIntentDetected = false; // More intelligent turn detection
    float maxRollInTurn = 0;        // Peak roll during turn
    unsigned long turnPeakTime = 0; // When peak was reached
    
    // Direction detection
    float accelForwardSum = 0;      // Running sum for direction
    int directionSamples = 0;       // Sample count
    const int maxDirectionSamples = 20; // Samples for direction averaging
    
    // Sensor fusion and filtering
    const float FILTER_ALPHA = 0.8; // Low-pass filter coefficient
    const float MOMENTUM_DECAY = 0.95; // Momentum decay factor

    struct MPUData {
      float pitch;
      float roll;
      float accelX;
      float accelY;
      float accelZ;
      float gyroX;
      float gyroY;
      float gyroZ;
    };

    MPUData getMPUData() {
      sensors_event_t accel, gyro, temp;
      mpu.getEvent(&accel, &gyro, &temp);

      // Calculate pitch and roll from accelerometer
      float pitch = atan2(-accel.acceleration.x, sqrt(accel.acceleration.y * accel.acceleration.y + accel.acceleration.z * accel.acceleration.z)) * 180.0 / PI;
      float roll = atan2(accel.acceleration.y, accel.acceleration.z) * 180.0 / PI;
      
      if (invertRoll) {
        if (roll > 0) roll -= 180;
        else roll += 180;
      }

      // Apply low-pass filtering to reduce noise
      filteredRoll = FILTER_ALPHA * filteredRoll + (1.0 - FILTER_ALPHA) * roll;
      filteredYaw = FILTER_ALPHA * filteredYaw + (1.0 - FILTER_ALPHA) * (gyro.gyro.z * 180.0 / PI);
      filteredAccelX = FILTER_ALPHA * filteredAccelX + (1.0 - FILTER_ALPHA) * accel.acceleration.x;
      filteredAccelY = FILTER_ALPHA * filteredAccelY + (1.0 - FILTER_ALPHA) * accel.acceleration.y;

      // Calculate momentum for intelligent turn detection
      float rollDelta = roll - lastRoll;
      float yawDelta = gyro.gyro.z * 180.0 / PI;
      
      // Update momentum with decay
      rollMomentum = rollMomentum * MOMENTUM_DECAY + rollDelta;
      yawMomentum = yawMomentum * MOMENTUM_DECAY + abs(yawDelta);

      // Direction detection based on forward/backward acceleration
      updateDirectionDetection(accel.acceleration.x);

      lastPitch = pitch;
      lastRoll = roll;
      lastYaw = gyro.gyro.z * 180.0 / PI;

      return {
        pitch,
        roll,
        accel.acceleration.x,
        accel.acceleration.y,
        accel.acceleration.z,
        gyro.gyro.x * 180.0 / PI,
        gyro.gyro.y * 180.0 / PI,
        gyro.gyro.z * 180.0 / PI
      };
    }

    void updateDirectionDetection(float accelX) {
      // Accumulate forward/backward acceleration samples
      accelForwardSum += accelX;
      directionSamples++;
      
      if (directionSamples >= maxDirectionSamples) {
        float avgAccel = accelForwardSum / directionSamples;
        
        // Determine movement direction based on averaged acceleration
        bool newDirection = avgAccel > forwardThreshold ? true : 
                           avgAccel < -forwardThreshold ? false : isMovingForward;
        
        // Only change direction if we have a strong signal and enough time has passed
        if (newDirection != isMovingForward && millis() - lastDirectionChange > 2000) {
          if (debugMode) {
            Serial.print("Direction change detected: ");
            Serial.println(newDirection ? "Forward" : "Backward");
          }
          isMovingForward = newDirection;
          lastDirectionChange = millis();
          
          // Swap headlight/taillight if enabled
          if (directionSwapEnabled) {
            swapHeadTailLights();
          }
        }
        
        // Reset accumulator
        accelForwardSum = 0;
        directionSamples = 0;
      }
    }

    void swapHeadTailLights() {
      // Swap the segment roles based on movement direction
      if (!isMovingForward) {
        // Moving backward - swap lighting roles
        if (debugMode) {
          Serial.println("Swapping lights for backward movement");
        }
        // Implementation depends on your specific lighting setup
        // This could involve swapping segment effects or colors
      }
    }

    void intelligentTurnDetection(MPUData& data) {
      unsigned long currentTime = millis();
      
      // Multi-factor turn detection algorithm
      bool rollCondition = abs(data.roll) > turnDetection;
      bool yawCondition = abs(data.gyroZ) > yawThreshold;
      bool lateralAccelCondition = abs(data.accelY) > accelThreshold;
      bool momentumCondition = abs(rollMomentum) > 3.0 || yawMomentum > 15.0;
      
      // Determine turn direction more intelligently
      int currentDirection = 0;
      if (data.roll > 5 || data.gyroZ > 10) currentDirection = 1;  // Right
      if (data.roll < -5 || data.gyroZ < -10) currentDirection = -1; // Left
      
      // Turn intent detection - requires multiple indicators
      bool turnIntent = (rollCondition && lateralAccelCondition) ||
                       (yawCondition && rollCondition) ||
                       (momentumCondition && (rollCondition || yawCondition));
      
      if (turnIntent) {
        if (!turnIntentDetected) {
          turnIntentDetected = true;
          turnStartTime = currentTime;
          maxRollInTurn = abs(data.roll);
          if (debugMode) {
            Serial.println("Turn intent detected");
          }
        }
        
        // Track maximum roll during turn for momentum analysis
        if (abs(data.roll) > maxRollInTurn) {
          maxRollInTurn = abs(data.roll);
          turnPeakTime = currentTime;
        }
        
        // Activate blinker after delay
        if (!blinkerTriggered && currentTime - turnStartTime > blinkerDelay) {
          blinkerTriggered = true;
          blinkDirection = currentDirection;
          endTurnStartTime = 0; // Reset end time
          if (debugMode) {
            Serial.print("Blinker activated, direction: ");
            Serial.println(blinkDirection);
          }
        }
        
        // Update direction if we detect a stronger signal
        if (blinkerTriggered && currentDirection != 0 && 
            abs(currentDirection) > abs(blinkDirection)) {
          blinkDirection = currentDirection;
        }
        
      } else {
        // No turn intent detected
        if (turnIntentDetected) {
          turnIntentDetected = false;
          
          // If blinker is active, start the end sequence
          if (blinkerTriggered && endTurnStartTime == 0) {
            endTurnStartTime = currentTime;
            if (debugMode) {
              Serial.println("Turn ended, starting blinker timeout");
            }
          }
        }
      }
      
      // Enhanced blinker deactivation logic
      if (blinkerTriggered && endTurnStartTime > 0) {
        unsigned long timeSinceEnd = currentTime - endTurnStartTime;
        unsigned long timeSincePeak = currentTime - turnPeakTime;
        
        // Use adaptive timing based on turn characteristics
        unsigned long adaptiveDelay = endBlinkerDelay;
        if (maxRollInTurn > 25) {
          adaptiveDelay += 500; // Longer delay for sharper turns
        }
        if (timeSincePeak > turnMomentumTime) {
          adaptiveDelay = min(adaptiveDelay, 1000UL); // Faster timeout if turn peaked long ago
        }
        
        // Deactivate blinker
        if (timeSinceEnd > adaptiveDelay) {
          blinkerTriggered = false;
          blinkDirection = 0;
          endTurnStartTime = 0;
          maxRollInTurn = 0;
          if (debugMode) {
            Serial.println("Blinker deactivated");
          }
        }
      }
      
      // Reset turn start time if no intent
      if (!turnIntentDetected && !blinkerTriggered) {
        turnStartTime = 0;
      }
    }

    void showBlinkerEffect(int direction) {
      if (!blinkerEnabled) return;

      if (millis() - lastBlinkTime > blinkInterval) {
        blinkState = !blinkState;
        lastBlinkTime = millis();
      }

      const segment& frontSeg = strip.getSegment(0);
      const segment& rearSeg  = strip.getSegment(1);

      int frontStart = frontSeg.start;
      int frontEnd = frontSeg.stop;
      int frontLen = frontEnd - frontStart;

      int rearStart = rearSeg.start;
      int rearEnd = rearSeg.stop;
      int rearLen = rearEnd - rearStart;

      for (int i = frontStart; i < frontEnd; i++) {
        bool isBlinkHalf = (direction > 0 && i - frontStart < frontLen / 2) || (direction < 0 && i - frontStart >= frontLen / 2);
        CRGB color = (isBlinkHalf && blinkState) ? CRGB::Yellow : CRGB::White;
        strip.setPixelColor(i, color.r, color.g, color.b);
      }

      for (int i = rearStart; i < rearEnd; i++) {
        bool isBlinkHalf = (direction > 0 && i - rearStart < rearLen / 2) || (direction < 0 && i - rearStart >= rearLen / 2);
        CRGB color = (isBlinkHalf && blinkState) ? CRGB::Yellow : CRGB::Red;
        strip.setPixelColor(i, color.r, color.g, color.b);
      }

      strip.show();
    }

    void startParkFX() {
      if (!parkModeEnabled) return;
      // Save current state for both segments
      for (uint8_t i = 0; i < 2; i++) {
        segment& seg = strip.getSegment(i);
        prevFXMode[i] = seg.mode;
        prevFXSpeed[i] = seg.speed;
        prevFXColor[i] = seg.colors[0];

        // Apply parking effect to both headlight and taillight
        seg.setMode(parkFXMode);
        seg.speed = parkFXSpeed;
        seg.setColor(0, parkFXColor);
        seg.on = true; // Ensure segment is on
      }
      strip.trigger(); // Force update
    }

    void stopParkFX() {
      // Restore previous state for both segments
      for (uint8_t i = 0; i < 2; i++) {
        segment& seg = strip.getSegment(i);
        seg.setMode(prevFXMode[i]);
        seg.speed = prevFXSpeed[i];
        seg.setColor(0, prevFXColor[i]);
      }
      strip.trigger(); // Force update
    }

  public:
    void setup() {
      Wire.begin();
      if (!mpu.begin()) {
        Serial.println("MPU6050 not found!");
        return;
      }
      Serial.println("MPU6050 Initialized");
      mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
      mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    }

    bool blinkerTriggered = false;
    int blinkDirection = 0;

    void loop() {
      MPUData data = getMPUData();
      if (debugMode) {
        Serial.print("Pitch: "); Serial.print(data.pitch);
        Serial.print(" | Roll: "); Serial.print(data.roll);
        Serial.print(" | GyroZ: "); Serial.print(data.gyroZ);
        Serial.print(" | AccelX: "); Serial.print(data.accelX);
        Serial.print(" | RollMom: "); Serial.print(rollMomentum);
        Serial.print(" | YawMom: "); Serial.print(yawMomentum);
        Serial.print(" | Direction: "); Serial.println(isMovingForward ? "FWD" : "BWD");
      }

      if (blinkerEnabled) {
        intelligentTurnDetection(data);
        
        if (blinkerTriggered) {
          showBlinkerEffect(blinkDirection);
          wasBlinkerActive = true;
        } else if (wasBlinkerActive) {
          wasBlinkerActive = false;
        }
      }

      if (parkModeEnabled) {
        if (abs(data.pitch) > parkDetection &&
            abs(data.gyroX) < stationaryGyroThreshold &&
            abs(data.gyroY) < stationaryGyroThreshold &&
            abs(data.gyroZ) < stationaryGyroThreshold) {

          if (!isStationary) {
            isStationary = true;
            parkStartTime = millis();
          }

          if (!parkModeActive && millis() - parkStartTime > 1000) {
            Serial.println("Parking Mode Triggered");
            startParkFX();
            parkModeActive = true;
          }
        } else {
          isStationary = false;
          if (parkModeActive) {
            stopParkFX();
            parkModeActive = false;
          }
        }
      }
      if (parkModeActive && !parkModeEnabled) {
        Serial.println("Parking has been disabled while parked");
        stopParkFX();
        parkModeActive = false;
      }
    }

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(USERMOD_IMU_NAME));
      top["blinkerEnabled"] = blinkerEnabled;
      top["parkModeEnabled"] = parkModeEnabled;
      top["directionSwapEnabled"] = directionSwapEnabled;
      top["turnThreshold"] = turnDetection;
      top["yawThreshold"] = yawThreshold;
      top["accelThreshold"] = accelThreshold;
      top["forwardThreshold"] = forwardThreshold;
      top["blinkerDelay"] = blinkerDelay;
      top["endBlinkerDelay"] = endBlinkerDelay;
      top["turnMomentumTime"] = turnMomentumTime;
      top["invertRoll"] = invertRoll;
      top["debugMode"] = debugMode;
      top["parkFXMode"] = parkFXMode;
      top["parkFXSpeed"] = parkFXSpeed;
      top["parkFXColor"] = parkFXColor;
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(USERMOD_IMU_NAME)];
      if (!top.isNull()) {
        blinkerEnabled = top["blinkerEnabled"] | true;
        parkModeEnabled = top["parkModeEnabled"] | true;
        directionSwapEnabled = top["directionSwapEnabled"] | false;
        turnDetection = top["turnThreshold"] | 15;
        yawThreshold = top["yawThreshold"] | 25.0;
        accelThreshold = top["accelThreshold"] | 0.15;
        forwardThreshold = top["forwardThreshold"] | 0.3;
        blinkerDelay = top["blinkerDelay"] | 300;
        endBlinkerDelay = top["endBlinkerDelay"] | 2000;
        turnMomentumTime = top["turnMomentumTime"] | 1500;
        invertRoll = top["invertRoll"] | false;
        debugMode = top["debugMode"] | false;
        parkFXMode = top["parkFXMode"] | FX_MODE_BREATH;
        parkFXSpeed = top["parkFXSpeed"] | 128;
        parkFXColor = top["parkFXColor"] | 0x0000FF;
      }
      return true;
    }

    void addToJsonInfo(JsonObject &root) override {
      JsonObject imu = root.createNestedObject(FPSTR(USERMOD_IMU_NAME));
      imu["blinkerEnabled"] = blinkerEnabled;
      imu["parkModeEnabled"] = parkModeEnabled;
      imu["directionSwapEnabled"] = directionSwapEnabled;
      imu["blinkerActive"] = blinkerTriggered;
      imu["parkModeActive"] = parkModeActive;
      imu["turnIntentDetected"] = turnIntentDetected;
      imu["isMovingForward"] = isMovingForward;
      imu["turnThreshold"] = turnDetection;
      imu["yawThreshold"] = yawThreshold;
      imu["accelThreshold"] = accelThreshold;
      imu["forwardThreshold"] = forwardThreshold;
      imu["blinkerDelay"] = blinkerDelay;
      imu["endBlinkerDelay"] = endBlinkerDelay;
      imu["turnMomentumTime"] = turnMomentumTime;
      imu["invertRoll"] = invertRoll;
      imu["debugMode"] = debugMode;
      imu["pitch"] = lastPitch;
      imu["roll"] = lastRoll;
      imu["yaw"] = lastYaw;
      imu["filteredRoll"] = filteredRoll;
      imu["rollMomentum"] = rollMomentum;
      imu["yawMomentum"] = yawMomentum;
      imu["maxRollInTurn"] = maxRollInTurn;
      imu["parkFXMode"] = parkFXMode;
      imu["parkFXSpeed"] = parkFXSpeed;
      imu["parkFXColor"] = parkFXColor;
      imu["blinkDirection"] = blinkDirection;
      imu["connected"] = true;  // Indicates MPU is working
    }

    uint16_t getId() override {
      return USERMOD_ID_IMU;
    }
};
