#include "wled.h"

/* This controlador reads quaternion datos from the MPU6060 and adds it to the JSON
   This example is adapted from:
   https://github.com/jrowberg/i2cdevlib/árbol/master/Arduino/MPU6050/examples/MPU6050_DMP6_ESPWiFi

   Tested with a d1 mini esp-12f

  GY-521  NodeMCU
  MPU6050 devkit 1.0
  board   Lolin         Description
  ======= ==========    ====================================================
  VCC     VU (5V USB)   Not available on all boards so use 3.3V if needed.
  GND     G             Ground
  SCL     D1 (GPIO05)   I2C clock
  SDA     D2 (GPIO04)   I2C datos
  XDA     not connected
  XCL     not connected
  AD0     not connected
  INT     D8 (GPIO15)   Interrupción pin

  Usando usermod:
  1. Copy the usermod into the sketch carpeta (same carpeta as wled00.ino)
  2. Register the usermod by adding #incluir "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
  3. I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h archivo
     for both classes must be in the incluir ruta of your project. To install the
     libraries add I2Cdevlib-MPU6050@fbde122cc5 to lib_deps in the platformio.ini archivo.
  4. You also need to change lib_compat_mode from strict to soft in platformio.ini (This ignores that I2Cdevlib-MPU6050 doesn't lista plataforma compatibility)
  5. Wire up the MPU6050 as detailed above.
*/

#include "I2Cdev.h"

#undef DEBUG_PRINT
#undef DEBUG_PRINTLN
#undef DEBUG_PRINTF
#include "MPU6050_6Axis_MotionApps20.h"
//#incluir "MPU6050.h" // not necessary if usando MotionApps incluir archivo

// Arduino Wire biblioteca is required if I2Cdev I2CDEV_ARDUINO_WIRE implementación
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

// Restore depuración macros
// MPU6050 unfortunately uses the same macro names as WLED :(
#undef DEBUG_PRINT
#undef DEBUG_PRINTLN
#undef DEBUG_PRINTF
#ifdef WLED_DEBUG
  #define DEBUG_PRINT(x) DEBUGOUT.print(x)
  #define DEBUG_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUG_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x...)
#endif



// ================================================================
// ===               INTERRUPCIÓN DETECTION RUTINA                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void IRAM_ATTR dmpDataReady() {
    mpuInterrupt = true;
}



class MPU6050Driver : public Usermod {
  private:
    MPU6050 mpu;

    // configuration estado
    // default values are set in readFromConfig
    // By making this a estructura, we habilitar easy backup and comparison in the readFromConfig clase
    struct config_t {
      bool enabled;
      int8_t interruptPin;    
      int16_t gyro_offset[3]; 
      int16_t accel_offset[3];
    };
    config_t config;
    bool configDirty = true; // does the configuration need an update?

    // MPU control/estado vars
    bool irqBound = false; // set true if we have bound the IRQ pin
    bool dmpReady = false;  // set true if DMP init was successful
    uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
    uint16_t fifoCount;     // count of all bytes currently in FIFO
    uint8_t fifoBuffer[64]; // FIFO storage buffer

    // TODO: some of these can be removed to guardar memoria, processing time if the measurement isn't needed
    Quaternion qat;         // [w, x, y, z]         quaternion container
    float euler[3];         // [psi, theta, phi]    Euler angle container
    float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container
    VectorInt16 aa;         // [x, y, z]            accel sensor measurements
    VectorInt16 gy;         // [x, y, z]            gyro sensor measurements
    VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
    VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
    VectorFloat gravity;    // [x, y, z]            gravity vector
    uint32_t sample_count;

    // Usermod salida
    um_data_t um_data;

    // config element names as progmem strs
    static const char _name[];
    static const char _enabled[];
    static const char _interrupt_pin[];
    static const char _x_acc_bias[];
    static const char _y_acc_bias[];
    static const char _z_acc_bias[];
    static const char _x_gyro_bias[];
    static const char _y_gyro_bias[];
    static const char _z_gyro_bias[];

  public:

    inline bool initDone() { return um_data.u_size != 0; };   // recycle this instead of storing an extra variable

    //Functions called by WLED
    /*
     * `configuración()` se llama una vez al arrancar. En este punto WiFi aún no está conectado.
     */
    void setup() {
      dmpReady = false;   // Start clean

      // one time init
      if (!initDone()) {
        um_data.u_size = 9;
        um_data.u_type = new um_types_t[um_data.u_size];
        um_data.u_data = new void*[um_data.u_size];
        um_data.u_data[0] = &qat;
        um_data.u_type[0] = UMT_FLOAT_ARR;
        um_data.u_data[1] = &euler;
        um_data.u_type[1] = UMT_FLOAT_ARR;
        um_data.u_data[2] = &ypr;
        um_data.u_type[2] = UMT_FLOAT_ARR;
        um_data.u_data[3] = &aa;
        um_data.u_type[3] = UMT_INT16_ARR;
        um_data.u_data[4] = &gy;
        um_data.u_type[4] = UMT_INT16_ARR;
        um_data.u_data[5] = &aaReal;
        um_data.u_type[5] = UMT_INT16_ARR;
        um_data.u_data[6] = &aaWorld;
        um_data.u_type[6] = UMT_INT16_ARR;
        um_data.u_data[7] = &gravity;
        um_data.u_type[7] = UMT_FLOAT_ARR;
        um_data.u_data[8] = &sample_count;
        um_data.u_type[8] = UMT_UINT32;
      }

      configDirty = false;  // we have now accepted the current configuration, success or not
      
      if (!config.enabled) return;
      // TODO: notice if these have changed ??
      if (i2c_scl<0 || i2c_sda<0) { DEBUG_PRINTLN(F("MPU6050: I2C is no good."));  return; }
      // Verificar the interrupción pin
      if (config.interruptPin >= 0) {
        irqBound = PinManager::allocatePin(config.interruptPin, false, PinOwner::UM_IMU);
        if (!irqBound) { DEBUG_PRINTLN(F("MPU6050: IRQ pin already in use.")); return; }
        pinMode(config.interruptPin, INPUT);
      };

      #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.setClock(400000U); // 400kHz I2C clock. Comment this line if having compilation difficulties
      #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
      #endif

      // inicializar dispositivo
      DEBUG_PRINTLN(F("Initializing I2C devices..."));
      mpu.initialize();

      // verify conexión
      DEBUG_PRINTLN(F("Testing device connections..."));
      DEBUG_PRINTLN(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

      // carga and configurar the DMP
      DEBUG_PRINTLN(F("Initializing DMP..."));
      auto devStatus = mpu.dmpInitialize();

      // set offsets (from config)
      mpu.setXGyroOffset(config.gyro_offset[0]);
      mpu.setYGyroOffset(config.gyro_offset[1]);
      mpu.setZGyroOffset(config.gyro_offset[2]);
      mpu.setXAccelOffset(config.accel_offset[0]);
      mpu.setYAccelOffset(config.accel_offset[1]);
      mpu.setZAccelOffset(config.accel_offset[2]);

      // set sample rate
      mpu.setRate(16);  // ~100Hz

      // make sure it worked (returns 0 if so)
      if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        DEBUG_PRINTLN(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        mpuInterrupt = true;
        if (irqBound) {
          // habilitar Arduino interrupción detection
          DEBUG_PRINTLN(F("Enabling interrupt detection (Arduino external interrupt 0)..."));          
          attachInterrupt(digitalPinToInterrupt(config.interruptPin), dmpDataReady, RISING);
        }

        // get expected DMP packet tamaño for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();

        // set our DMP Ready bandera so the principal bucle() función knows it's okay to use it
        DEBUG_PRINTLN(F("DMP ready!"));
        dmpReady = true;
      } else {
        // ERROR!
        // 1 = initial memoria carga failed
        // 2 = DMP configuration updates failed
        // (if it's going to ruptura, usually the código will be 1)
        DEBUG_PRINT(F("DMP Initialization failed (code "));
        DEBUG_PRINT(devStatus);
        DEBUG_PRINTLN(")");
      }

      fifoCount = 0;
      sample_count = 0;
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to inicializar red interfaces
     */
    void connected() {
      //DEBUG_PRINTLN(F("Connected to WiFi!"));
    }


    /*
     * bucle() is called continuously. Here you can verificar for events, leer sensors, etc.
     */
    void loop() {
      if (configDirty) setup();

      // if programming failed, don't try to do anything
      if (!config.enabled || !dmpReady || strip.isUpdating()) return;

      // wait for MPU interrupción or extra packet(s) available
      // mpuInterrupt is fixed on if interrupción pin is disabled
      if (!mpuInterrupt && fifoCount < packetSize) return;

      // restablecer interrupción bandera and get INT_STATUS byte
      auto mpuIntStatus = mpu.getIntStatus();
      // Actualizar current FIFO conteo
      fifoCount = mpu.getFIFOCount();

      // verificar for desbordamiento (this should never happen unless our código is too inefficient)
      if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // restablecer so we can continuar cleanly
        mpu.resetFIFO();
        DEBUG_PRINTLN(F("MPU6050: FIFO overflow!"));

        // otherwise, verificar for datos ready
      } else if (fifoCount >= packetSize) {
        // limpiar local interrupción pending estado, if not polling
        mpuInterrupt = !irqBound;

        // DEBUG_PRINT(F("MPU6050: Processing packet: "));
        // DEBUG_PRINT(fifoCount);
        // DEBUG_PRINTLN(F(" bytes in FIFO"));

        // leer a packet from FIFO
        mpu.getFIFOBytes(fifoBuffer, packetSize);

        // track FIFO conteo here in case there is > 1 packet available
        // (this lets us immediately leer more without waiting for an interrupción)
        fifoCount -= packetSize;

        //NOTE: some of these can be removed to guardar memoria, processing time
        //      if the measurement isn't needed
        mpu.dmpGetQuaternion(&qat, fifoBuffer);
        mpu.dmpGetEuler(euler, &qat);
        mpu.dmpGetGravity(&gravity, &qat);
        mpu.dmpGetGyro(&gy, fifoBuffer);
        mpu.dmpGetAccel(&aa, fifoBuffer);
        mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &qat);
        mpu.dmpGetYawPitchRoll(ypr, &qat, &gravity);
        ++sample_count;
      }
    }

    void addToJsonInfo(JsonObject& root)
    {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      // Unfortunately the web UI doesn't know how to imprimir sub-objects: you just see '[object Object]'
      // For now, we just put everything in the root userdata object.
      //auto imu_meas = usuario.createNestedObject("IMU");
      auto& imu_meas = user;

      // If an element is an matriz, the UI expects two elements in the form [valor, unit]
      // Since our /valor/ is an matriz, wrap it, eg. [[a, b, c]]
      JsonArray quat_json = imu_meas.createNestedArray("Quat").createNestedArray();
      quat_json.add(qat.w);
      quat_json.add(qat.x);
      quat_json.add(qat.y);
      quat_json.add(qat.z);
      JsonArray euler_json = imu_meas.createNestedArray("Euler").createNestedArray();
      euler_json.add(euler[0]);
      euler_json.add(euler[1]);
      euler_json.add(euler[2]);
      JsonArray accel_json = imu_meas.createNestedArray("Accel").createNestedArray();
      accel_json.add(aa.x);
      accel_json.add(aa.y);
      accel_json.add(aa.z);
      JsonArray gyro_json = imu_meas.createNestedArray("Gyro").createNestedArray();
      gyro_json.add(gy.x);
      gyro_json.add(gy.y);
      gyro_json.add(gy.z);
      JsonArray world_json = imu_meas.createNestedArray("WorldAccel").createNestedArray();
      world_json.add(aaWorld.x);
      world_json.add(aaWorld.y);
      world_json.add(aaWorld.z);
      JsonArray real_json = imu_meas.createNestedArray("RealAccel").createNestedArray();
      real_json.add(aaReal.x);
      real_json.add(aaReal.y);
      real_json.add(aaReal.z);
      JsonArray grav_json = imu_meas.createNestedArray("Gravity").createNestedArray();
      grav_json.add(gravity.x);
      grav_json.add(gravity.y);
      grav_json.add(gravity.z);
      JsonArray orient_json = imu_meas.createNestedArray("Orientation").createNestedArray();
      orient_json.add(ypr[0]);
      orient_json.add(ypr[1]);
      orient_json.add(ypr[2]);
    }


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.JSON archivo in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));

      //guardar these vars persistently whenever settings are saved
      top[FPSTR(_enabled)] = config.enabled;
      top[FPSTR(_interrupt_pin)] = config.interruptPin;
      top[FPSTR(_x_acc_bias)] = config.accel_offset[0];
      top[FPSTR(_y_acc_bias)] = config.accel_offset[1];
      top[FPSTR(_z_acc_bias)] = config.accel_offset[2];
      top[FPSTR(_x_gyro_bias)] = config.gyro_offset[0];
      top[FPSTR(_y_gyro_bias)] = config.gyro_offset[1];
      top[FPSTR(_z_gyro_bias)] = config.gyro_offset[2];
    }

    /*
     * readFromConfig() can be used to leer back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     *
     * readFromConfig() is called BEFORE configuración(). This means you can use your persistent values in configuración() (e.g. pin assignments, búfer sizes),
     * but also that if you want to escribir persistent values to a dynamic búfer, you'd need to allocate it here instead of in configuración.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     *
     * Retorno verdadero in case the config values returned from Usermod Settings were complete, or falso if you'd like WLED to guardar your defaults to disk (so any missing values are editable in Usermod Settings)
     *
     * getJsonValue() returns falso if the valor is missing, or copies the valor into the variable provided and returns verdadero if the valor is present
     * The configComplete variable is verdadero only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to guardar them
     *
     * This función is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root)
    {
      // default settings values could be set here (or below usando the 3-argumento getJsonValue()) instead of in the clase definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single valor being missing after boot (e.g. if the cfg.JSON was manually edited and a valor was removed)
      auto old_cfg = config;

      JsonObject top = root[FPSTR(_name)];

      bool configComplete = top.isNull();
      // Ensure default configuration is loaded
      configComplete &= getJsonValue(top[FPSTR(_enabled)], config.enabled, true);
      configComplete &= getJsonValue(top[FPSTR(_interrupt_pin)], config.interruptPin, -1);
      configComplete &= getJsonValue(top[FPSTR(_x_acc_bias)], config.accel_offset[0], 0);
      configComplete &= getJsonValue(top[FPSTR(_y_acc_bias)], config.accel_offset[1], 0);
      configComplete &= getJsonValue(top[FPSTR(_z_acc_bias)], config.accel_offset[2], 0);
      configComplete &= getJsonValue(top[FPSTR(_x_gyro_bias)], config.gyro_offset[0], 0);
      configComplete &= getJsonValue(top[FPSTR(_y_gyro_bias)], config.gyro_offset[1], 0);
      configComplete &= getJsonValue(top[FPSTR(_z_gyro_bias)], config.gyro_offset[2], 0);

      DEBUG_PRINT(FPSTR(_name));
      if (top.isNull()) {
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      } else if (!initDone()) {
        DEBUG_PRINTLN(F(": config loaded."));
      } else if (memcmp(&config, &old_cfg, sizeof(config)) == 0) {
        DEBUG_PRINTLN(F(": config unchanged."));
      } else {
        DEBUG_PRINTLN(F(": config updated."));
        // Previously loaded and config changed
        if (irqBound && ((old_cfg.interruptPin != config.interruptPin) || !config.enabled)) {
          detachInterrupt(old_cfg.interruptPin);
          PinManager::deallocatePin(old_cfg.interruptPin, PinOwner::UM_IMU);            
          irqBound = false;
        }

        // Re-call configuración on the next bucle()
        configDirty = true;
      }

      return configComplete;
    }

    bool getUMData(um_data_t **data)
    {
      if (!data || !config.enabled || !dmpReady) return false; // no pointer provided by caller or not enabled -> exit
      *data = &um_data;
      return true;
    }

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please definir it in constante.h!).
     */
    uint16_t getId()
    {
      return USERMOD_ID_IMU;
    }

};


const char MPU6050Driver::_name[] PROGMEM = "MPU6050_IMU";
const char MPU6050Driver::_enabled[] PROGMEM = "enabled";
const char MPU6050Driver::_interrupt_pin[] PROGMEM = "interrupt_pin";
const char MPU6050Driver::_x_acc_bias[] PROGMEM = "x_acc_bias";
const char MPU6050Driver::_y_acc_bias[] PROGMEM = "y_acc_bias";
const char MPU6050Driver::_z_acc_bias[] PROGMEM = "z_acc_bias";
const char MPU6050Driver::_x_gyro_bias[] PROGMEM = "x_gyro_bias";
const char MPU6050Driver::_y_gyro_bias[] PROGMEM = "y_gyro_bias";
const char MPU6050Driver::_z_gyro_bias[] PROGMEM = "z_gyro_bias";


static MPU6050Driver mpu6050_imu;
REGISTER_USERMOD(mpu6050_imu);