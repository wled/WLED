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
    bool invertTurnDirection = false;  // For testing different orientations
    bool debugMode = true;
    
    // Calibration system
    bool calibrationMode = false;
    bool calibrationComplete = false;
    int calibrationStep = 0;
    unsigned long calibrationStartTime = 0;
    const unsigned long calibrationTimeout = 30000; // 30 seconds per step
    
    // Web UI calibration controls - WLED will auto-create UI elements for these
    bool startCalibrationButton = false;
    bool resetCalibrationButton = false;
    
    // Calibration data structure
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
    int turnDetection = 15;             // Degrees of roll for initial detection
    float yawThreshold = 25.0;          // deg/s for angular velocity
    float accelThreshold = 1.5;         // m/s² for lateral acceleration (2G range) - increased for less sensitivity
    float forwardThreshold = 2.0;      // m/s² for forward/backward detection (2G range) - increased for less sensitivity
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
      // With rotated IMU: swap roll and yaw deltas
      float rollDelta = gyro.gyro.z * 180.0 / PI - lastYaw;  // gyroZ is now roll
      float yawDelta = roll - lastRoll;                      // roll is now yaw
      
      // Update momentum with decay (swapped)
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

    void startCalibration() {
      calibrationMode = true;
      calibrationStep = 0;
      calibrationStartTime = millis();
      calibration.valid = false;
      calibrationComplete = false;
      Serial.println("=== MPU6050 CALIBRATION STARTED ===");
      Serial.println("Step 1: Hold device LEVEL and press any key...");
    }
    
    void resetCalibration() {
      calibrationComplete = false;
      calibration.valid = false;
      Serial.println("Calibration reset. Hold device still for 5 seconds to start calibration.");
    }

    void captureCalibrationStep(MPUData& data) {
      unsigned long currentTime = millis();
      
      // Check for timeout
      if (currentTime - calibrationStartTime > calibrationTimeout) {
        Serial.println("Calibration timeout! Restarting...");
        startCalibration();
        return;
      }

      switch(calibrationStep) {
        case 0: // Level
          calibration.levelAccelX = data.accelX;
          calibration.levelAccelY = data.accelY;
          calibration.levelAccelZ = data.accelZ;
          Serial.println("Level captured. Step 2: Tilt FORWARD and press any key...");
          break;
          
        case 1: // Forward
          calibration.forwardAccelX = data.accelX;
          calibration.forwardAccelY = data.accelY;
          calibration.forwardAccelZ = data.accelZ;
          Serial.println("Forward captured. Step 3: Tilt BACKWARD and press any key...");
          break;
          
        case 2: // Backward
          calibration.backwardAccelX = data.accelX;
          calibration.backwardAccelY = data.accelY;
          calibration.backwardAccelZ = data.accelZ;
          Serial.println("Backward captured. Step 4: Tilt LEFT and press any key...");
          break;
          
        case 3: // Left
          calibration.leftAccelX = data.accelX;
          calibration.leftAccelY = data.accelY;
          calibration.leftAccelZ = data.accelZ;
          Serial.println("Left captured. Step 5: Tilt RIGHT and press any key...");
          break;
          
        case 4: // Right
          calibration.rightAccelX = data.accelX;
          calibration.rightAccelY = data.accelY;
          calibration.rightAccelZ = data.accelZ;
          Serial.println("Right captured. Calculating orientation...");
          calculateOrientation();
          break;
      }
      
      calibrationStep++;
      calibrationStartTime = currentTime;
    }

    void calculateOrientation() {
      // Calculate differences for forward/backward axis
      float forwardBackwardX = abs(calibration.forwardAccelX - calibration.backwardAccelX);
      float forwardBackwardY = abs(calibration.forwardAccelY - calibration.backwardAccelY);
      float forwardBackwardZ = abs(calibration.forwardAccelZ - calibration.backwardAccelZ);
      
      // Calculate differences for left/right axis
      float leftRightX = abs(calibration.leftAccelX - calibration.rightAccelX);
      float leftRightY = abs(calibration.leftAccelY - calibration.rightAccelY);
      float leftRightZ = abs(calibration.leftAccelZ - calibration.rightAccelZ);
      
      // Find the axis with maximum change for forward/backward
      if (forwardBackwardX > forwardBackwardY && forwardBackwardX > forwardBackwardZ) {
        calibration.forwardAxis = 'X';
        calibration.forwardSign = (calibration.forwardAccelX > calibration.backwardAccelX) ? 1 : -1;
      } else if (forwardBackwardY > forwardBackwardZ) {
        calibration.forwardAxis = 'Y';
        calibration.forwardSign = (calibration.forwardAccelY > calibration.backwardAccelY) ? 1 : -1;
      } else {
        calibration.forwardAxis = 'Z';
        calibration.forwardSign = (calibration.forwardAccelZ > calibration.backwardAccelZ) ? 1 : -1;
      }
      
      // Find the axis with maximum change for left/right
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
      Serial.print("Forward axis: "); Serial.print(calibration.forwardAxis);
      Serial.print(" (sign: "); Serial.print(calibration.forwardSign); Serial.println(")");
      Serial.print("Left/Right axis: "); Serial.print(calibration.leftRightAxis);
      Serial.print(" (sign: "); Serial.print(calibration.leftRightSign); Serial.println(")");
      Serial.println("Calibration data saved!");
    }

    float getCalibratedForwardAccel(MPUData& data) {
      if (!calibration.valid) return data.accelX; // Fallback to default
      
      switch(calibration.forwardAxis) {
        case 'X': return data.accelX * calibration.forwardSign;
        case 'Y': return data.accelY * calibration.forwardSign;
        case 'Z': return data.accelZ * calibration.forwardSign;
        default: return data.accelX;
      }
    }

    float getCalibratedLeftRightAccel(MPUData& data) {
      if (!calibration.valid) return data.accelY; // Fallback to default
      
      switch(calibration.leftRightAxis) {
        case 'X': return data.accelX * calibration.leftRightSign;
        case 'Y': return data.accelY * calibration.leftRightSign;
        case 'Z': return data.accelZ * calibration.leftRightSign;
        default: return data.accelY;
      }
    }

    void intelligentTurnDetection(MPUData& data) {
      unsigned long currentTime = millis();
      
      // Use calibrated values for orientation-independent detection
      float calibratedLeftRight = getCalibratedLeftRightAccel(data);
      float calibratedForward = getCalibratedForwardAccel(data);
      
      // Simple roll-based turn detection using calibrated values
      bool rollCondition = abs(calibratedLeftRight) > accelThreshold;
      bool lateralAccelCondition = abs(calibratedLeftRight) > accelThreshold;
      
      // Determine turn direction based on calibrated left/right acceleration
      int currentDirection = 0;
      if (calibratedLeftRight > accelThreshold) currentDirection = 1;   // Right tilt
      if (calibratedLeftRight < -accelThreshold) currentDirection = -1; // Left tilt
      
      // Apply direction inversion if needed for orientation
      if (invertTurnDirection) {
        currentDirection = -currentDirection;
      }
      
      // OneWheel-optimized turn detection: requires both tilt AND lateral acceleration
      bool tiltCondition = abs(calibratedLeftRight) > 0.8;  // Lower tilt threshold
      bool lateralCondition = abs(calibratedLeftRight) > accelThreshold; // Higher lateral threshold
      bool turnIntent = tiltCondition && lateralCondition;  // Both required for OneWheel
      
      if (turnIntent) {
        if (!turnIntentDetected) {
          turnIntentDetected = true;
          turnStartTime = currentTime;
          endTurnStartTime = 0; // Reset end timer
          if (debugMode) {
            Serial.println("OneWheel turn intent detected");
          }
        }
        
        // Activate blinker after delay
        if (!blinkerTriggered && currentTime - turnStartTime > blinkerDelay) {
          blinkerTriggered = true;
          blinkDirection = currentDirection;
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
        // No turn intent detected - start turn end sequence
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
        if (maxRollInTurn > 25) {  // maxRollInTurn now tracks gyroZ (roll with rotated IMU)
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
      Wire.begin(SDA_PIN, SCL_PIN);
      if (!mpu.begin()) {
        Serial.println("MPU6050 not found!");
        return;
      }
      Serial.println("MPU6050 Initialized");
      mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
      mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    }

    bool blinkerTriggered = false;
    int blinkDirection = 0;

    void loop() {
      MPUData data = getMPUData();
      
      // Handle calibration mode
      if (calibrationMode) {
        // Check for serial input to advance calibration steps
        if (Serial.available()) {
          Serial.read(); // Clear the buffer
          captureCalibrationStep(data);
        }
        
        // Show calibration status
        if (debugMode) {
          Serial.print("CALIBRATION Step "); Serial.print(calibrationStep + 1);
          Serial.print("/5 | AccelX: "); Serial.print(data.accelX);
          Serial.print(" | AccelY: "); Serial.print(data.accelY);
          Serial.print(" | AccelZ: "); Serial.println(data.accelZ);
        }
        return; // Skip normal operation during calibration
      }
      
      // Check for manual calibration commands
      if (Serial.available()) {
        String command = Serial.readString();
        command.trim();
        command.toLowerCase();
        
        if (command == "calibrate" || command == "cal") {
          Serial.println("Manual calibration triggered!");
          startCalibration();
        } else if (command == "reset") {
          resetCalibration();
        } else if (command == "status") {
          Serial.print("Calibration Status - Complete: "); Serial.print(calibrationComplete ? "YES" : "NO");
          Serial.print(" | Valid: "); Serial.print(calibration.valid ? "YES" : "NO");
          Serial.print(" | Mode: "); Serial.println(calibrationMode ? "YES" : "NO");
        } else if (command == "disable") {
          blinkerEnabled = false;
          blinkerTriggered = false;
          Serial.println("Blinker disabled");
        } else if (command == "enable") {
          blinkerEnabled = true;
          Serial.println("Blinker enabled");
        } else if (command == "stop") {
          blinkerTriggered = false;
          blinkDirection = 0;
          Serial.println("Blinker stopped");
        }
      }
      
      // Check for web UI calibration button presses
      static bool lastCalibrationButtonState = false;
      static bool lastResetButtonState = false;
      
      if (startCalibrationButton && !lastCalibrationButtonState) {
        // Start calibration button was just pressed
        Serial.println("Calibration button pressed from web UI!");
        startCalibration();
      }
      lastCalibrationButtonState = startCalibrationButton;
      
      if (resetCalibrationButton && !lastResetButtonState) {
        // Reset calibration button was just pressed
        Serial.println("Reset calibration button pressed from web UI!");
        resetCalibration();
      }
      lastResetButtonState = resetCalibrationButton;
      
      if (debugMode) {
        float calibratedLR = getCalibratedLeftRightAccel(data);
        float calibratedFW = getCalibratedForwardAccel(data);
        Serial.print("Raw - X:"); Serial.print(data.accelX);
        Serial.print(" Y:"); Serial.print(data.accelY);
        Serial.print(" Z:"); Serial.print(data.accelZ);
        Serial.print(" | Calibrated - LR:"); Serial.print(calibratedLR);
        Serial.print(" FW:"); Serial.print(calibratedFW);
        Serial.print(" | Tilt:"); Serial.print(abs(calibratedLR) > 0.8 ? "YES" : "NO");
        Serial.print(" | Lateral:"); Serial.print(abs(calibratedLR) > accelThreshold ? "YES" : "NO");
        Serial.print(" | Triggered:"); Serial.print(blinkerTriggered ? "YES" : "NO");
        Serial.print(" | Direction:"); Serial.println(blinkDirection);
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
      top["invertTurnDirection"] = invertTurnDirection;
      top["debugMode"] = debugMode;
      top["parkFXMode"] = parkFXMode;
      top["parkFXSpeed"] = parkFXSpeed;
      top["parkFXColor"] = parkFXColor;
      
      // Add calibration buttons for web UI
      top["startCalibrationButton"] = startCalibrationButton;
      top["resetCalibrationButton"] = resetCalibrationButton;
      
      // Add calibration data
      JsonObject calib = top.createNestedObject("calibration");
      calib["valid"] = calibration.valid;
      calib["forwardAxis"] = String(calibration.forwardAxis);
      calib["leftRightAxis"] = String(calibration.leftRightAxis);
      calib["forwardSign"] = calibration.forwardSign;
      calib["leftRightSign"] = calibration.leftRightSign;
      calib["complete"] = calibrationComplete;
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
        invertTurnDirection = top["invertTurnDirection"] | false;
        debugMode = top["debugMode"] | false;
        parkFXMode = top["parkFXMode"] | FX_MODE_BREATH;
        parkFXSpeed = top["parkFXSpeed"] | 128;
        parkFXColor = top["parkFXColor"] | 0x0000FF;
        
        // Load calibration button states
        startCalibrationButton = top["startCalibrationButton"] | false;
        resetCalibrationButton = top["resetCalibrationButton"] | false;
        
        // Load calibration data
        JsonObject calib = top["calibration"];
        if (!calib.isNull()) {
          calibration.valid = calib["valid"] | false;
          calibration.forwardAxis = calib["forwardAxis"].as<String>().charAt(0);
          calibration.leftRightAxis = calib["leftRightAxis"].as<String>().charAt(0);
          calibration.forwardSign = calib["forwardSign"] | 1;
          calibration.leftRightSign = calib["leftRightSign"] | 1;
          calibrationComplete = calib["complete"] | false;
          
          if (calibration.valid) {
            Serial.println("Calibration data loaded from config");
          }
        }
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
      imu["invertTurnDirection"] = invertTurnDirection;
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
      
      // Add calibration status
      imu["calibrationComplete"] = calibrationComplete;
      imu["calibrationValid"] = calibration.valid;
      imu["calibrationMode"] = calibrationMode;
      imu["forwardAxis"] = String(calibration.forwardAxis);
      imu["leftRightAxis"] = String(calibration.leftRightAxis);
      imu["startCalibrationButton"] = startCalibrationButton;
      imu["resetCalibrationButton"] = resetCalibrationButton;
    }

    uint16_t getId() override {
      return USERMOD_ID_IMU;
    }
};
