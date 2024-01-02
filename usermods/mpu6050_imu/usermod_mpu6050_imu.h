#pragma once

#include <Arduino.h>   // WLEDMM: make sure that I2C drivers have the "right" Wire Object
#include <Wire.h>

#include "wled.h"

  // #define MPU6050_INT_GPIO 13  // WLEDMM - better choice on ESP32

#ifdef WLED_DEBUG
  #define DEBUG_PRINT_IMU(x) DEBUG_PRINT(x)
  #define DEBUG_PRINT_IMULN(x) DEBUG_PRINTLN(x)
  #define DEBUG_PRINT_IMUF(x...) DEBUG_PRINTF(x)
#else
  #define DEBUG_PRINT_IMU(x)
  #define DEBUG_PRINT_IMULN(x)
  #define DEBUG_PRINT_IMUF(x...)
#endif


/* This driver reads quaternion data from the MPU6060 and adds it to the JSON
   This example is adapted from:
   https://github.com/jrowberg/i2cdevlib/tree/master/Arduino/MPU6050/examples/MPU6050_DMP6_ESPWiFi

   Tested with a d1 mini esp-12f

  GY-521  NodeMCU
  MPU6050 devkit 1.0
  board   Lolin         Description
  ======= ==========    ====================================================
  VCC     VU (5V USB)   Not available on all boards so use 3.3V if needed.
  GND     G             Ground
  SCL     D1 (GPIO05)   I2C clock
  SDA     D2 (GPIO04)   I2C data
  XDA     not connected
  XCL     not connected
  AD0     not connected
  INT     D8 (GPIO15)   Interrupt pin
  
  Using usermod:
  1. Copy the usermod into the sketch folder (same folder as wled00.ino)
  2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
  3. I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h file 
     for both classes must be in the include path of your project. To install the
     libraries add ElectronicCats/MPU6050 @ 0.6.0 to lib_deps in the platformio.ini file.
  // 4. You also need to change lib_compat_mode from strict to soft in platformio.ini (This ignores that I2Cdevlib-MPU6050 doesn't list platform compatibility)
  5. Wire up the MPU6050 as detailed above.
*/

// WLEDMM: make sure that the "standard" Wire object is used
#define I2CDEV_IMPLEMENTATION       I2CDEV_ARDUINO_WIRE

// WLEDMM avoid stupid warnings
#undef DEBUG_PRINT
#undef DEBUG_PRINTLN
#undef DEBUG_PRINTF

#include <I2Cdev.h>

#include <MPU6050_6Axis_MotionApps20.h>

// WLEDMM - need to re-define WLED DEBUG_PRINT maros, because the were overwritten by MPU6050_6Axis_MotionApps20.h
#undef DEBUG_PRINT
#undef DEBUG_PRINTLN
#undef DEBUG_PRINTF

#ifdef WLED_DEBUG
  #define DEBUG_PRINT(x) DEBUGOUT(x)
  #define DEBUG_PRINTLN(x) DEBUGOUTLN(x)
  #define DEBUG_PRINTF(x...) DEBUGOUTF(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x...)
#endif

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    //#include "Wire.h"        // WLEDMM not necessary
#endif

// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void IRAM_ATTR dmpDataReady() {
    mpuInterrupt = true;
}


class MPU6050Driver : public Usermod {
  private:
    MPU6050 mpu;
    unsigned long lastUMRun = millis();

    // MPU control/status vars
    uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
    uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
    uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
    uint16_t fifoCount;     // count of all bytes currently in FIFO
    uint8_t fifoBuffer[64]; // FIFO storage buffer

    // strings to reduce flash memory usage (used more than twice)
    static const char _INT_pin[];

  public:
    MPU6050Driver(const char *name, bool enabled):Usermod(name, enabled) {} //WLEDMM: this shouldn't be necessary (passthrough of constructor), maybe because Usermod is an abstract class

    bool dmpReady = false;  // set true if DMP init was successful  // WLEDMM expose this info in public interface
    // orientation/motion vars
    Quaternion qat;         // [w, x, y, z]         quaternion container
    VectorInt16 aa;         // [x, y, z]            accel sensor measurements
    VectorInt16 gy;         // [x, y, z]            gyro sensor measurements
    VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
    VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
    VectorFloat gravity;    // [x, y, z]            gravity vector
    float euler[3] = {0.0f};// [psi, theta, phi]    Euler angle container
    float ypr[3]  = {0.0f}; // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

    #if !defined(ARDUINO_ARCH_ESP32) || !defined(MPU6050_INT_GPIO)
    static const int INTERRUPT_PIN = -1; // WLEDMM: not use pin 15 (on ESP8266) as can and will cause conflict with other pins
    #else
    static const int INTERRUPT_PIN = MPU6050_INT_GPIO;       // WLEDMM
    #endif

    void setup() {
      // WLEDMM begin
      if (!enabled) {
        dmpReady = false;
        return;
      }
      USER_PRINTLN(F("mpu setup"));
      PinManagerPinType pins[2] = { { i2c_scl, true }, { i2c_sda, true } };
      if ((i2c_scl < 0) || (i2c_sda < 0)) {
        //enabled = false;
        USER_PRINTF("mpu6050: warning - ivalid I2C pins: sda=%d scl=%d\n", i2c_sda, i2c_scl);
        //return;
      }

      if (pins[1].pin < 0 || pins[0].pin < 0)  { enabled=false; dmpReady = false; return; }  //WLEDMM bugfix - ensure that "final" GPIO are valid and no "-1" sneaks trough
      //if (!pinManager.allocateMultiplePins(pins, 2, PinOwner::HW_I2C)) {       

      // WLEDMM join I2C HW wire
      if (!pinManager.joinWire()) {
        enabled = false;
        dmpReady = false;
        USER_PRINTF("mpu6050: failed to allocate I2C sda=%d scl=%d\n", i2c_sda, i2c_scl);
        return;
      }
      // WLEDMM end

      // join I2C bus (I2Cdev library doesn't do this automatically)
      #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#if defined(ARDUINO_ARCH_ESP32)
      //Wire.begin(pins[1].pin, pins[0].pin);        // WLEDMM fix - need to use proper pins, in case that Wire was not started yet. Call will silently fail if Wire is initialized already.
#else
      //Wire.begin();  // WLEDMM - i2c pins on 8266 are fixed.
#endif

          Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
      #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
          Fastwire::setup(400, true);
      #endif

      // // initialize serial communication
      // // (115200 chosen because it is required for Teapot Demo output, but it's
      // // really up to you depending on your project)
      // Serial.begin(115200);
      // while (!Serial); // wait for Leonardo enumeration, others continue immediately

      // NOTE: 8MHz or slower host processors, like the Teensy @ 3.3V or Arduino
      // Pro Mini running at 3.3V, cannot handle this baud rate reliably due to
      // the baud timing being too misaligned with processor ticks. You must use
      // 38400 or slower in these cases, or use some kind of external separate
      // crystal solution for the UART timer.

      // initialize device
      DEBUG_PRINT_IMULN(F("Initializing I2C devices..."));
      // WLEDMM begin
      if ((INTERRUPT_PIN < 0) || (!pinManager.isPinINT(INTERRUPT_PIN))) {
        //enabled = false;
        USER_PRINTF("mpu6050: warning - interrupt GPIO %d does not support interrupts.\n", INTERRUPT_PIN);
        //INTERRUPT_PIN = -1;
        //return;
      }
      if ((INTERRUPT_PIN >= 0) && (pinManager.getPinOwner(INTERRUPT_PIN) != PinOwner::UM_IMU)  // only allocate pin if we don't own it already
         && !pinManager.allocatePin(INTERRUPT_PIN, false, PinOwner::UM_IMU))
      {
        //enabled = false;
        USER_PRINTF("mpu6050: warning - failed to allocate interrupt GPIO %d\n", INTERRUPT_PIN);
        //INTERRUPT_PIN = -1;
        //return;
      }
      // WLEDMM end

      mpu.initialize();
      if (INTERRUPT_PIN >= 0) {         // WLEDMM only if pin is valid
        pinMode(INTERRUPT_PIN, INPUT);
      }

      // verify connection
      DEBUG_PRINT_IMULN(F("Testing device connections..."));
      USER_PRINTLN(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

      // // wait for ready
      // DEBUG_PRINT_IMULN(F("\nSend any character to begin DMP programming and demo: "));
      // while (Serial.available() && Serial.read()); // empty buffer
      // while (!Serial.available());                 // wait for data
      // while (Serial.available() && Serial.read()); // empty buffer again

      // load and configure the DMP
      DEBUG_PRINT_IMULN(F("Initializing DMP..."));
      devStatus = mpu.dmpInitialize();

      // supply your own gyro offsets here, scaled for min sensitivity
      mpu.setXGyroOffset(220);
      mpu.setYGyroOffset(76);
      mpu.setZGyroOffset(-85);
      mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

      // make sure it worked (returns 0 if so)
      if (devStatus == 0) {
          // Calibration Time: generate offsets and calibrate our MPU6050
          mpu.CalibrateAccel(6);
          mpu.CalibrateGyro(6);
          #ifdef WLED_DEBUG
          mpu.PrintActiveOffsets();
          #endif
          // turn on the DMP, now that it's ready
          DEBUG_PRINT_IMULN(F("Enabling DMP..."));
          mpu.setDMPEnabled(true);

          if (INTERRUPT_PIN >= 0) {
            // enable Arduino interrupt detection
            DEBUG_PRINT_IMU(F("Enabling interrupt detection (Arduino external interrupt "));
            DEBUG_PRINT_IMU(digitalPinToInterrupt(INTERRUPT_PIN));
            DEBUG_PRINT_IMULN(F(")..."));
            attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
          }
          mpuIntStatus = mpu.getIntStatus();

          // set our DMP Ready flag so the main loop() function knows it's okay to use it
          DEBUG_PRINT_IMULN(F("DMP ready! Waiting for first interrupt..."));
          dmpReady = true;

          // get expected DMP packet size for later comparison
          packetSize = mpu.dmpGetFIFOPacketSize();
      } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        DEBUG_PRINT(F("DMP Initialization failed (code "));
        DEBUG_PRINT(devStatus);
        DEBUG_PRINTLN(")");
        dmpReady = false;
      }
      initDone = true;
    }

    void connected() {
    }


    void loop() {
      // if programming failed, don't try to do anything
      if (!initDone) return;
      if (!enabled || (strip.isUpdating() && (millis() - lastUMRun < 2))) return;   // be nice, but not too nice
      lastUMRun = millis();                    // update time keeping

      if (!dmpReady) return;
      // read a packet from FIFO
      if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) { // Get the Latest packet 
        //NOTE: some of these can be removed to save memory, processing time
        //      if the measurement isn't needed
        mpu.dmpGetQuaternion(&qat, fifoBuffer);
        mpu.dmpGetEuler(euler, &qat);
        mpu.dmpGetGravity(&gravity, &qat);
        mpu.dmpGetGyro(&gy, fifoBuffer);
        mpu.dmpGetAccel(&aa, fifoBuffer);
        mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &qat);
        mpu.dmpGetYawPitchRoll(ypr, &qat, &gravity);
      }
    }

    void addToJsonInfo(JsonObject& root)
    {
      if (!initDone) return;
      if (!enabled && !dmpReady) return;     // WLEDMM no info when usermod disabled
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      StaticJsonDocument<800> doc; //measured 528  // WLEDMM added some margin (was 600)

      JsonObject imu_meas = doc.createNestedObject("IMU");
      //JsonObject imu_meas = user.createNestedObject("IMU");
      #ifdef WLED_DEBUG
      JsonArray quat_json = imu_meas.createNestedArray("Quat");
      quat_json.add(qat.w);
      quat_json.add(qat.x);
      quat_json.add(qat.y);
      quat_json.add(qat.z);
      JsonArray euler_json = imu_meas.createNestedArray("Euler");
      euler_json.add(euler[0] * 180/M_PI);
      euler_json.add(euler[1] * 180/M_PI);
      euler_json.add(euler[2] * 180/M_PI);
      JsonArray accel_json = imu_meas.createNestedArray("Accel");
      accel_json.add(aa.x);
      accel_json.add(aa.y);
      accel_json.add(aa.z);
      JsonArray gyro_json = imu_meas.createNestedArray("Gyro");
      gyro_json.add(gy.x);
      gyro_json.add(gy.y);
      gyro_json.add(gy.z);
      JsonArray real_json = imu_meas.createNestedArray("RealAccel");
      real_json.add(aaReal.x);
      real_json.add(aaReal.y);
      real_json.add(aaReal.z);
      JsonArray grav_json = imu_meas.createNestedArray("Gravity");
      grav_json.add(gravity.x);
      grav_json.add(gravity.y);
      grav_json.add(gravity.z);
      #endif
      JsonArray world_json = imu_meas.createNestedArray("WorldAccel");
      world_json.add(aaWorld.x);
      world_json.add(aaWorld.y);
      world_json.add(aaWorld.z);
      JsonArray orient_json = imu_meas.createNestedArray("YPR");
      orient_json.add(ypr[0] * 180/M_PI);
      orient_json.add(ypr[1] * 180/M_PI);
      orient_json.add(ypr[2] * 180/M_PI);
      char stringBuffer[400]; // measured 266 // WLEDMM added some margin (was 300)
      serializeJson(imu_meas, stringBuffer);
      JsonArray mainObject = user.createNestedArray("IMU");
      if (!dmpReady || !enabled) {       // WLEDMM
        if (!dmpReady) mainObject.add(F("Sensor Not Found"));
        else if (!enabled) mainObject.add(F("usermod disabled"));
      } else {
        mainObject.add(stringBuffer);
      }
      // Serial.printf("imu_meas %u (%u %u) stringBuffer %u\n", (unsigned int)imu_meas.memoryUsage(), (unsigned int)imu_meas.size(), (unsigned int)imu_meas.nesting(), strlen(stringBuffer));

    }


    //void addToJsonState(JsonObject& root)
    //{
    //}

    //void readFromJsonState(JsonObject& root)
    //{
    //}

    // void addToConfig(JsonObject& root)
    // {
    //   Usermod::addToConfig(root);
    //   JsonObject top = root[FPSTR(_name)];
    // //   //JsonObject interruptPin = top.createNestedObject(FPSTR(_INT_pin));
    // //   //interruptPin["pin"] = INTERRUPT_PIN;
    // //   DEBUG_PRINTLN(F("MPU6050 IMU config saved."));
    // }

    //WLEDMM: add appendConfigData
    void appendConfigData()
    {
      oappend(SET_F("addHB('mpu6050-IMU');"));
  /*
      #ifdef MPU6050_INT_GPIO
        oappend(SET_F("xOpt('mpu6050-IMU:interrupt_pin',0,' ⎌',")); oappendi(MPU6050_INT_GPIO); oappend(");"); 
      #endif
      //WLEDMM add errorMessage to um settings
      if (strcmp(errorMessage, "") != 0) {
        oappend(SET_F("addInfo('errorMessage', 0, '<i>error: ")); oappend(errorMessage); oappend("! Correct and reboot</i>');");
      }
  */
    }

    bool readFromConfig(JsonObject& root)
    {
      bool configComplete = Usermod::readFromConfig(root);
      JsonObject top = root[FPSTR(_name)];

      if (top.isNull()) {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      //configComplete &= getJsonValue(top[FPSTR(_INT_pin)]["pin"], INTERRUPT_PIN);

      DEBUG_PRINT(FPSTR(_name));
      if (!initDone) {
        // first run: reading from cfg.json
        DEBUG_PRINTLN(F(" config loaded."));
      } else {
        DEBUG_PRINTLN(F(" config (re)loaded."));
        if (enabled || dmpReady) setup();   // re-run setup if user has checked "enabled"
        if (!enabled) dmpReady = false;     // not enabled inplies "no DMP data ready"
      }

      return configComplete;
      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      //return !top[FPSTR(_INT_pin)].isNull();
    }

    uint16_t getId()
    {
      return USERMOD_ID_IMU;
    }

};

// strings to reduce flash memory usage (used more than twice)
const char MPU6050Driver::_INT_pin[]     PROGMEM = "interrupt_pin";
