#pragma once

#include "wled.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// External variables to control effects
extern bool leftBlinkerActive;
extern bool rightBlinkerActive;
#define SDA_PIN 5
#define SCL_PIN 6

class MPU6050Driver : public Usermod {
  private:
    Adafruit_MPU6050 mpu;

    // State tracking
    bool wasBlinkerActive = false;
    bool isStationary = false;
    unsigned long turnStartTime = 0;
    unsigned long parkStartTime = 0;

    // Constants
    const float stationaryGyroThreshold = 0.1; // deg/s
    const int turnDetection = 20;              // degrees of roll
    const int parkDetection = 15;              // degrees of pitch

    // Restore state after blinkers
    int prevEffect = 0;

    struct MPUData {
      float pitch;
      float roll;
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
      Serial.print("Accel: ");
      Serial.print(accel.acceleration.x);
      Serial.print(", ");
      Serial.print(accel.acceleration.y);
      Serial.print(", ");
      Serial.print(accel.acceleration.z);
      
      Serial.print(" | Gyro: ");
      Serial.print(gyro.gyro.x);
      Serial.print(", ");
      Serial.print(gyro.gyro.y);
      Serial.print(", ");
      Serial.println(gyro.gyro.z);
      return {
        pitch,
        roll,
        accel.acceleration.z,
        gyro.gyro.x,
        gyro.gyro.y,
        gyro.gyro.z
      };
    }

  public:
    void setup() {
      Serial.begin(115200);
      while (!Serial) delay(10);  // Wait for Serial connection to initialize (especially needed on some USB setups)
      Wire.begin();  // SDA = GPIO5, SCL = GPIO6

      if (!mpu.begin()) {
        Serial.println("MPU6050 not found!");
        return;
      }
      Serial.println("MPU6050 Initialized");
      mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
      mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    }

    void loop() {
      MPUData data = getMPUData();

      
      // ---- Blinker Detection ----
      if (abs(data.roll) > turnDetection) {
        if (turnStartTime == 0) {
          turnStartTime = millis();
        }

        if (millis() - turnStartTime > 500) {
          leftBlinkerActive = (data.roll > 0);
          rightBlinkerActive = (data.roll < 0);

          if (!wasBlinkerActive) {
            Serial.println("Blinker Mode Triggered");
            wasBlinkerActive = true;
            prevEffect = strip.getMainSegment().mode;
            strip.getMainSegment().setMode(FX_MODE_BLINKER);
          }
        }
      } else {
        turnStartTime = 0;

        if (wasBlinkerActive) {
          wasBlinkerActive = false;
          leftBlinkerActive = false;
          rightBlinkerActive = false;
          strip.getMainSegment().setMode(prevEffect);
        }
      }

      // ---- Park Detection ----
      if (abs(data.pitch) > parkDetection &&
          abs(data.gyroX) < stationaryGyroThreshold &&
          abs(data.gyroY) < stationaryGyroThreshold &&
          abs(data.gyroZ) < stationaryGyroThreshold) {

        if (!isStationary) {
          isStationary = true;
          parkStartTime = millis();
        }

        if (millis() - parkStartTime > 1000) {
          Serial.println("Parking Mode Triggered");
          // You can add LED effect logic here if desired
        }
      } else {
        isStationary = false;
      }
    }

    uint16_t getId() {
      return USERMOD_ID_IMU;
    }
};
