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
    bool invertRoll = false;
    bool debugMode = false;
    int turnDetection = 20;             // Degrees of roll
    float yawThreshold = 20.0;          // deg/s
    float accelThreshold = 0.2;         // g's
    unsigned long blinkerDelay = 500;   // Milliseconds before triggering blinker
    unsigned long endBlinkerDelay = 1500;

    // Park FX settings
    uint8_t parkFXMode = FX_MODE_BLINK_RAINBOW;
    uint8_t parkFXSpeed = 200;
    uint32_t parkFXColor = 0x0000FF;

    // State tracking
    bool wasBlinkerActive = false;
    bool isStationary = false;
    bool parkModeActive = false;
    unsigned long turnStartTime = 0;
    unsigned long parkStartTime = 0;
    unsigned long endTurnStartTime = 0;

    // Saved FX state
    uint8_t prevFXMode[2];
    uint8_t prevFXSpeed[2];
    uint32_t prevFXColor[2];

    // Blinker timing
    bool blinkState = false;
    unsigned long lastBlinkTime = 0;
    const unsigned long blinkInterval = 500; // ms

    const float stationaryGyroThreshold = 0.1; // deg/s
    const int parkDetection = 15;              // degrees of pitch

    float lastPitch = 0;
    float lastRoll = 0;

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

      float pitch = atan2(-accel.acceleration.x, sqrt(accel.acceleration.y * accel.acceleration.y + accel.acceleration.z * accel.acceleration.z)) * 180.0 / PI;
      float roll = atan2(accel.acceleration.y, accel.acceleration.z) * 180.0 / PI;
      if (invertRoll) {
        if (roll > 0) roll -= 180;
        else roll += 180;
      }

      lastPitch = pitch;
      lastRoll = roll;

      return {
        pitch,
        roll,
        accel.acceleration.x,
        accel.acceleration.y,
        accel.acceleration.z,
        gyro.gyro.x,
        gyro.gyro.y,
        gyro.gyro.z
      };
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
      for (uint8_t i = 0; i < 2; i++) {
        segment& seg = strip.getSegment(i);
        prevFXMode[i] = seg.mode;
        prevFXSpeed[i] = seg.speed;
        prevFXColor[i] = seg.colors[0];

        seg.setMode(parkFXMode);
        seg.setColor(0, parkFXColor);
      }
    }

    void stopParkFX() {
      for (uint8_t i = 0; i < 2; i++) {
        segment& seg = strip.getSegment(i);
        seg.setMode(prevFXMode[i]);
        seg.setColor(0, prevFXColor[i]);
      }
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
        Serial.print(" | AccelX: "); Serial.println(data.accelX);
      }

      if (blinkerEnabled) {
        bool turnDetected = false;
        if (fabs(data.accelX) > accelThreshold) {
          if (fabs(data.gyroZ) > yawThreshold) {
            turnDetected = true;
          } else if (fabs(data.roll) > turnDetection) {
            turnDetected = true;
          }
        }

        if (turnDetected) {
          endTurnStartTime = 0;
          if (turnStartTime == 0) turnStartTime = millis();
          if (millis() - turnStartTime > blinkerDelay) {
            blinkDirection = (data.roll > 0) ? 1 : -1;
            blinkerTriggered = true;
          }
        } else if (blinkerTriggered) {
          if (endTurnStartTime == 0) endTurnStartTime = millis();
          if (millis() - endTurnStartTime > endBlinkerDelay) {
            blinkerTriggered = false;
            blinkDirection = 0;
          }
        } else {
          turnStartTime = 0;
        }

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
      top["turnThreshold"] = turnDetection;
      top["yawThreshold"] = yawThreshold;
      top["accelThreshold"] = accelThreshold;
      top["blinkerDelay"] = blinkerDelay;
      top["endBlinkerDelay"] = endBlinkerDelay;
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
        turnDetection = top["turnThreshold"] | 20;
        yawThreshold = top["yawThreshold"] | 20.0;
        accelThreshold = top["accelThreshold"] | 0.2;
        blinkerDelay = top["blinkerDelay"] | 500;
        endBlinkerDelay = top["endBlinkerDelay"] | 1500;
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
      imu["turnThreshold"] = turnDetection;
      imu["yawThreshold"] = yawThreshold;
      imu["accelThreshold"] = accelThreshold;
      imu["blinkerDelay"] = blinkerDelay;
      imu["endBlinkerDelay"] = endBlinkerDelay;
      imu["invertRoll"] = invertRoll;
      imu["debugMode"] = debugMode;
      imu["pitch"] = lastPitch;
      imu["roll"] = lastRoll;
      imu["parkFXMode"] = parkFXMode;
      imu["parkFXSpeed"] = parkFXSpeed;
      imu["parkFXColor"] = parkFXColor;
    }

    uint16_t getId() override {
      return USERMOD_ID_IMU;
    }
};
