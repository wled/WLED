#pragma once

#include "wled.h"

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
     libraries add I2Cdevlib-MPU6050@fbde122cc5 to lib_deps in the platformio.ini file.
  4. You also need to change lib_compat_mode from strict to soft in platformio.ini (This ignores that I2Cdevlib-MPU6050 doesn't list platform compatibility)
  5. Wire up the MPU6050 as detailed above.
*/

#include "I2Cdev.h"

#include "MPU6050_6Axis_MotionApps20.h"
//#include "MPU6050.h" // not necessary if using MotionApps include file

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
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
    bool enabled = true;

    // MPU control/status vars
    bool dmpReady = false;  // set true if DMP init was successful
    uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
    uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
    uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
    uint16_t fifoCount;     // count of all bytes currently in FIFO
    uint8_t fifoBuffer[64]; // FIFO storage buffer

    //NOTE: some of these can be removed to save memory, processing time
    //      if the measurement isn't needed
    Quaternion qat;         // [w, x, y, z]         quaternion container
    float euler[3];         // [psi, theta, phi]    Euler angle container
    float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container
    VectorInt16 aa;         // [x, y, z]            accel sensor measurements
    VectorInt16 gy;         // [x, y, z]            gyro sensor measurements
    VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
    VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
    VectorFloat gravity;    // [x, y, z]            gravity vector

    static const int INTERRUPT_PIN = 15; // use pin 15 on ESP8266

  public:
    //Functions called by WLED

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     */
    void setup() {
      if (i2c_scl<0 || i2c_sda<0) { enabled = false; return; }
      #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.setClock(400000U); // 400kHz I2C clock. Comment this line if having compilation difficulties
      #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
      #endif

      // initialize device
      DEBUG_PRINTLN(F("Initializing I2C devices..."));
      mpu.initialize();
      pinMode(INTERRUPT_PIN, INPUT);

      // verify connection
      DEBUG_PRINTLN(F("Testing device connections..."));
      DEBUG_PRINTLN(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

      // load and configure the DMP
      DEBUG_PRINTLN(F("Initializing DMP..."));
      devStatus = mpu.dmpInitialize();

      // supply your own gyro offsets here, scaled for min sensitivity
      mpu.setXGyroOffset(220);
      mpu.setYGyroOffset(76);
      mpu.setZGyroOffset(-85);
      mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

      // make sure it worked (returns 0 if so)
      if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        DEBUG_PRINTLN(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        DEBUG_PRINTLN(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        DEBUG_PRINTLN(F("DMP ready! Waiting for first interrupt..."));
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
      }
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() {
      //DEBUG_PRINTLN("Connected to WiFi!");
    }


    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     */
    void loop() {
      // if programming failed, don't try to do anything
      if (!enabled || !dmpReady || strip.isUpdating()) return;

      // wait for MPU interrupt or extra packet(s) available
      if (!mpuInterrupt && fifoCount < packetSize) return;

      // reset interrupt flag and get INT_STATUS byte
      mpuInterrupt = false;
      mpuIntStatus = mpu.getIntStatus();

      // get current FIFO count
      fifoCount = mpu.getFIFOCount();

      // check for overflow (this should never happen unless our code is too inefficient)
      if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        DEBUG_PRINTLN(F("FIFO overflow!"));

        // otherwise, check for DMP data ready interrupt (this should happen frequently)
      } else if (mpuIntStatus & 0x02) {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

        // read a packet from FIFO
        mpu.getFIFOBytes(fifoBuffer, packetSize);

        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;


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
      int reading = 20;
      //this code adds "u":{"Light":[20," lux"]} to the info object
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonObject imu_meas = user.createNestedObject("IMU");
      JsonArray quat_json = imu_meas.createNestedArray("Quat");
      quat_json.add(qat.w);
      quat_json.add(qat.x);
      quat_json.add(qat.y);
      quat_json.add(qat.z);
      JsonArray euler_json = imu_meas.createNestedArray("Euler");
      euler_json.add(euler[0]);
      euler_json.add(euler[1]);
      euler_json.add(euler[2]);
      JsonArray accel_json = imu_meas.createNestedArray("Accel");
      accel_json.add(aa.x);
      accel_json.add(aa.y);
      accel_json.add(aa.z);
      JsonArray gyro_json = imu_meas.createNestedArray("Gyro");
      gyro_json.add(gy.x);
      gyro_json.add(gy.y);
      gyro_json.add(gy.z);
      JsonArray world_json = imu_meas.createNestedArray("WorldAccel");
      world_json.add(aaWorld.x);
      world_json.add(aaWorld.y);
      world_json.add(aaWorld.z);
      JsonArray real_json = imu_meas.createNestedArray("RealAccel");
      real_json.add(aaReal.x);
      real_json.add(aaReal.y);
      real_json.add(aaReal.z);
      JsonArray grav_json = imu_meas.createNestedArray("Gravity");
      grav_json.add(gravity.x);
      grav_json.add(gravity.y);
      grav_json.add(gravity.z);
      JsonArray orient_json = imu_meas.createNestedArray("Orientation");
      orient_json.add(ypr[0]);
      orient_json.add(ypr[1]);
      orient_json.add(ypr[2]);
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void addToJsonState(JsonObject& root)
    //{
      //root["user0"] = userVar0;
    //}


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void readFromJsonState(JsonObject& root)
    //{
      //if (root["bri"] == 255) DEBUG_PRINTLN(F("Don't burn down your garage!"));
    //}


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
//    void addToConfig(JsonObject& root)
//    {
//      JsonObject top = root.createNestedObject("MPU6050_IMU");
//      JsonArray pins = top.createNestedArray("pin");
//      pins.add(HW_PIN_SCL);
//      pins.add(HW_PIN_SDA);
//    }

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     */
    uint16_t getId()
    {
      return USERMOD_ID_IMU;
    }

};
