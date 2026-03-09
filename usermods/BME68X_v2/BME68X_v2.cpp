/**
 * @file usermod_BMW68X.cpp
 * @author Gabriel A. Sieben  (GeoGab)
 * @brief Usermod for WLED to implement the BME680/BME688 sensor
 * @version 1.0.2
 * @date 28 March 2025
 */

 #define UMOD_DEVICE "ESP32"                 			 // NOTE - Set your hardware here
 #define HARDWARE_VERSION "1.0"              			 // NOTE - Set your hardware version here
 #define UMOD_BME680X_SW_VERSION "1.0.2"     			 // NOTE - Version of the User Mod
 #define CALIB_FILE_NAME "/BME680X-Calib.hex"			 // NOTE - Calibration file name
 #define UMOD_NAME "BME680X"                 			 // NOTE - User module name
 #define UMOD_DEBUG_NAME "UM-BME680X: "      			 // NOTE - Debug print module name addon
 
 #define ESC "\033"
 #define ESC_CSI ESC "["
 #define ESC_STYLE_RESET ESC_CSI "0m"
 #define ESC_CURSOR_COLUMN(n) ESC_CSI #n "G"
 
 #define ESC_FGCOLOR_BLACK ESC_CSI "30m"
 #define ESC_FGCOLOR_RED ESC_CSI "31m"
 #define ESC_FGCOLOR_GREEN ESC_CSI "32m"
 #define ESC_FGCOLOR_YELLOW ESC_CSI "33m"
 #define ESC_FGCOLOR_BLUE ESC_CSI "34m"
 #define ESC_FGCOLOR_MAGENTA ESC_CSI "35m"
 #define ESC_FGCOLOR_CYAN ESC_CSI "36m"
 #define ESC_FGCOLOR_WHITE ESC_CSI "37m"
 #define ESC_FGCOLOR_DEFAULT ESC_CSI "39m"
 
 /* Debug Print Special Text */
 #define INFO_COLUMN ESC_CURSOR_COLUMN(60)
 #define GOGAB_OK INFO_COLUMN "[" ESC_FGCOLOR_GREEN "OK" ESC_STYLE_RESET "]"
 #define GOGAB_FAIL INFO_COLUMN "[" ESC_FGCOLOR_RED "FAIL" ESC_STYLE_RESET "]"
 #define GOGAB_WARN INFO_COLUMN "[" ESC_FGCOLOR_YELLOW "WARN" ESC_STYLE_RESET "]"
 #define GOGAB_DONE INFO_COLUMN "[" ESC_FGCOLOR_CYAN "DONE" ESC_STYLE_RESET "]"
 
 #include "bsec.h" // Bosch sensor library
 #include "wled.h"
 #include <Arduino.h>
 
 /* UsermodBME68X class definition */
 class UsermodBME68X : public Usermod {
 
	 public:
	 /* Public: Functions */
	 uint16_t getId();
	 void loop();								// Loop of the user module called by wled main in loop
	 void setup();								// Setup of the user module called by wled main
	 void addToConfig(JsonObject& root);			// Extends the settings/user module settings page to include the user module requirements. The settings are written from the wled core to the configuration file.
	 void appendConfigData();					// Adds extra info to the config page of weld
	 bool readFromConfig(JsonObject& root);		// Reads config values
	 void addToJsonInfo(JsonObject& root);		// Adds user module info to the weld info page
 
	 /* Wled internal functions which can be used by the core or other user mods */
	 inline float getTemperature();				// Get Temperature in the selected scale of °C or °F
	 inline float getHumidity();					// ...
	 inline float getPressure();
	 inline float getGasResistance();
	 inline float getAbsoluteHumidity();
	 inline float getDewPoint();
	 inline float getIaq();
	 inline float getStaticIaq();
	 inline float getCo2();
	 inline float getVoc();
	 inline float getGasPerc();
	 inline uint8_t getIaqAccuracy();
	 inline uint8_t getStaticIaqAccuracy();
	 inline uint8_t getCo2Accuracy();
	 inline uint8_t getVocAccuracy();
	 inline uint8_t getGasPercAccuracy();
	 inline bool getStabStatus();
	 inline bool getRunInStatus();
 
	 private:
	 /* Private: Functions */
	 void HomeAssistantDiscovery();
	 void MQTT_PublishHASensor(const String& name, const String& deviceClass, const String& unitOfMeasurement, const int8_t& digs, const uint8_t& option = 0);
	 void MQTT_publish(const char* topic, const float& value, const int8_t& dig);
	 void onMqttConnect(bool sessionPresent);
	 void checkIaqSensorStatus();
	 void InfoHelper(JsonObject& root, const char* name, const float& sensorvalue, const int8_t& decimals, const char* unit);
	 void InfoHelper(JsonObject& root, const char* name, const String& sensorvalue, const bool& status);
	 void loadState();
	 void saveState();
	 void getValues();
 
	 /*** V A R I A B L E s  &  C O N S T A N T s ***/
	 /* Private: Settings of Usermod BME68X */
	 struct settings_t {
		 bool enabled;               					// true if user module is active
		 byte I2cadress; 	            				// Depending on the manufacturer, the BME680 has the address 0x76 or 0x77
		 uint8_t Interval; 	             				// Interval of reading sensor data in seconds
		 uint16_t MaxAge;	            				// Force the publication of the value of a sensor after these defined seconds at the latest
		 bool pubAcc; 	        						// Publish the accuracy values
		 bool publishSensorState;  	     				// Publisch the sensor calibration state
		 bool publishAfterCalibration ;  				// The IAQ/CO2/VOC/GAS value are only valid after the sensor has been calibrated. If this switch is active, the values are only sent after calibration
		 bool PublischChange;		       				// Publish values even when they have not changed
		 bool PublishIAQVerbal; 		     				// Publish Index of Air Quality (IAQ) classification Verbal
		 bool PublishStaticIAQVerbal;					// Publish Static Index of Air Quality (Static IAQ) Verbal
		 byte tempScale; 	               				// 0 -> Use Celsius, 1-> Use Fahrenheit
		 float tempOffset; 	             				// Temperature Offset
		 bool HomeAssistantDiscovery; 					// Publish Home Assistant Device Information
		 bool pauseOnActiveWled ;						// If this is set to true, the user mod ist not executed while wled is active
		 
		 /* Decimal Places (-1 means inactive) */
		 struct decimals_t {
			 int8_t temperature;
			 int8_t humidity;
			 int8_t pressure;
			 int8_t gasResistance;
			 int8_t absHumidity;
			 int8_t drewPoint;
			 int8_t iaq;
			 int8_t staticIaq;
			 int8_t co2;
			 int8_t Voc;
			 int8_t gasPerc;
		 } decimals;
	 } settings;
 
	 /* Private: Flags */
	 struct flags_t {
		 bool InitSuccessful = false;  					// Initialation was un-/successful
		 bool MqttInitialized = false; 					// MQTT Initialation done flag (first MQTT Connect)
		 bool SaveState = false;       					// Save the calibration data flag
		 bool DeleteCaibration = false;					// If set the calib file will be deleted on the next round
	 } flags;
 
	 /* Private: Measurement timers */
	 struct timer_t {
		 long actual;  									// Actual time stamp
		 long lastRun; 									// Last measurement time stamp
	 } timer;
 
	 /* Private: Various variables */
	 String stringbuff;                           		// General string stringbuff buffer
	 char charbuffer[128];                        		// General char stringbuff buffer
	 String InfoPageStatusLine;                   		// Shown on the info page of WLED
	 String tempScale;                            		// °C or °F
	 uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE]; 		// Calibration data array
	 uint16_t stateUpdateCounter; 	            		// Save state couter
	 static const uint8_t bsec_config_iaq[];				// Calibration Buffer
	 Bsec iaqSensor;										// Sensor variable
 
	 /* Private: Sensor values */
	 struct values_t {
		 float temperature;   							// Temp [°C] (Sensor-compensated)
		 float humidity;     							// Relative humidity [%] (Sensor-compensated)
		 float pressure;      							// raw pressure [hPa]
		 float gasResistance; 							// raw gas restistance [Ohm]
		 float absHumidity; 								// UserMod calculated: Absolute Humidity [g/m³]
		 float drewPoint;        						// UserMod calculated: drew point [°C/°F]
		 float iaq;           							// IAQ (Indoor Air Quallity)
		 float staticIaq;								// Satic IAQ
		 float co2;										// CO2 [PPM]
		 float Voc;										// VOC in [PPM]
		 float gasPerc;									// Gas Percentage in [%]
		 uint8_t iaqAccuracy; 							// IAQ accuracy -  IAQ Accuracy = 1 means value is inaccurate, IAQ Accuracy = 2 means sensor is being calibrated, IAQ Accuracy = 3 means sensor successfully calibrated.
		 uint8_t staticIaqAccuracy;						// Static IAQ accuracy
		 uint8_t co2Accuracy;							// co2 accuracy
		 uint8_t VocAccuracy;							// voc accuracy
		 uint8_t gasPercAccuracy;						// Gas percentage accuracy
		 bool stabStatus;  								// Indicates if the sensor is undergoing initial stabilization during its first use after production
		 bool runInStatus; 								// Indicates when the sensor is ready after after switch-on
	 } valuesA, valuesB, *ValuesPtr, *PrevValuesPtr, *swap; 	// Data Scructur A, Data Structur B, Pointers to switch between data channel A & B
 
	 struct cvalues_t {
		 String iaqVerbal;    							// IAQ verbal
		 String staticIaqVerbal; 						// Static IAQ verbal
 
	 } cvalues;
	 
	 /* Private: Sensor settings */
	 bsec_virtual_sensor_t sensorList[13] = {
		 BSEC_OUTPUT_IAQ,                                 // Index for Air Quality estimate [0-500] Index for Air Quality (IAQ) gives an indication of the relative change in ambient TVOCs detected by BME680.
		 BSEC_OUTPUT_STATIC_IAQ,                          // Unscaled Index for Air Quality estimate
		 BSEC_OUTPUT_CO2_EQUIVALENT,                      // CO2 equivalent estimate [ppm]
		 BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,               // Breath VOC concentration estimate [ppm]
		 BSEC_OUTPUT_RAW_TEMPERATURE,                     // Temperature sensor signal [degrees Celsius] Temperature directly measured by BME680 in degree Celsius. This value is cross-influenced by the sensor heating and device specific heating.
		 BSEC_OUTPUT_RAW_PRESSURE,                        // Pressure sensor signal [Pa] Pressure directly measured by the BME680 in Pa.
		 BSEC_OUTPUT_RAW_HUMIDITY,                        // Relative humidity sensor signal [%] Relative humidity directly measured by the BME680 in %. This value is cross-influenced by the sensor heating and device specific heating.
		 BSEC_OUTPUT_RAW_GAS,                             // Gas sensor signal [Ohm] Gas resistance measured directly by the BME680 in Ohm.The resistance value changes due to varying VOC concentrations (the higher the concentration of reducing VOCs, the lower the resistance and vice versa).
		 BSEC_OUTPUT_STABILIZATION_STATUS,                // Gas sensor stabilization status [boolean] Indicates initial stabilization status of the gas sensor element: stabilization is ongoing (0) or stabilization is finished (1).
		 BSEC_OUTPUT_RUN_IN_STATUS,                       // Gas sensor run-in status [boolean] Indicates power-on stabilization status of the gas sensor element: stabilization is ongoing (0) or stabilization is finished (1)
		 BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, // Sensor heat compensated temperature [degrees Celsius] Temperature measured by BME680 which is compensated for the influence of sensor (heater) in degree Celsius. The self heating introduced by the heater is depending on the sensor operation mode and the sensor supply voltage.
		 BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,    // Sensor heat compensated humidity [%] Relative measured by BME680 which is compensated for the influence of sensor (heater) in %. It converts the ::BSEC_INPUT_HUMIDITY from temperature ::BSEC_INPUT_TEMPERATURE to temperature ::BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE.
		 BSEC_OUTPUT_GAS_PERCENTAGE                       // Percentage of min and max filtered gas value [%]
	 };
 
	 /*** V A R I A B L E s  &  C O N S T A N T s ***/
	 /* Public: strings to reduce flash memory usage (used more than twice) */
	 static const char _enabled[];
	 static const char _hadtopic[];
 
	 /* Public: Settings Strings*/
	 static const char _nameI2CAdr[];
	 static const char _nameInterval[];
	 static const char _nameMaxAge[];
	 static const char _namePubAc[];
	 static const char _namePubSenState[];
	 static const char _namePubAfterCalib[];
	 static const char _namePublishChange[];
	 static const char _nameTempScale[];
	 static const char _nameTempOffset[];
	 static const char _nameHADisc[];
	 static const char _nameDelCalib[];
 
	 /* Public: Sensor names / Sensor short names */
	 static const char _nameTemp[];
	 static const char _nameHum[];
	 static const char _namePress[];
	 static const char _nameGasRes[];
	 static const char _nameAHum[];
	 static const char _nameDrewP[];
	 static const char _nameIaq[];
	 static const char _nameIaqAc[];
	 static const char _nameIaqVerb[];
	 static const char _nameStaticIaq[];
	 static const char _nameStaticIaqVerb[];
	 static const char _nameStaticIaqAc[];
	 static const char _nameCo2[];
	 static const char _nameCo2Ac[];
	 static const char _nameVoc[];
	 static const char _nameVocAc[];
	 static const char _nameComGasAc[];
	 static const char _nameGasPer[];
	 static const char _nameGasPerAc[];
	 static const char _namePauseOnActWL[];
 
	 static const char _nameStabStatus[];
	 static const char _nameRunInStatus[];
 
	 /* Public: Sensor Units */
	 static const char _unitTemp[];
	 static const char _unitHum[];
	 static const char _unitPress[];
	 static const char _unitGasres[];
	 static const char _unitAHum[];
	 static const char _unitDrewp[];
	 static const char _unitIaq[];
	 static const char _unitStaticIaq[];
	 static const char _unitCo2[];
	 static const char _unitVoc[];
	 static const char _unitGasPer[];
	 static const char _unitNone[];
 
	 static const char _unitCelsius[];
	 static const char _unitFahrenheit[];
 }; // UsermodBME68X class definition End
 
 /*** Setting C O N S T A N T S ***/
 /* Private: Settings Strings*/
 const char UsermodBME68X::_enabled[] 			PROGMEM = "Enabled";
 const char UsermodBME68X::_hadtopic[] 			PROGMEM = "homeassistant/sensor/";
 
 const char UsermodBME68X::_nameI2CAdr[] 		PROGMEM = "i2C Address";
 const char UsermodBME68X::_nameInterval[] 		PROGMEM = "Interval";
 const char UsermodBME68X::_nameMaxAge[] 		PROGMEM = "Max Age";
 const char UsermodBME68X::_namePublishChange[] 	PROGMEM = "Pub changes only";
 const char UsermodBME68X::_namePubAc[] 			PROGMEM = "Pub Accuracy";
 const char UsermodBME68X::_namePubSenState[] 	PROGMEM = "Pub Calib State";
 const char UsermodBME68X::_namePubAfterCalib[] 	PROGMEM = "Pub After Calib";
 const char UsermodBME68X::_nameTempScale[] 		PROGMEM = "Temp Scale";
 const char UsermodBME68X::_nameTempOffset[] 	PROGMEM = "Temp Offset";
 const char UsermodBME68X::_nameHADisc[] 		PROGMEM = "HA Discovery";
 const char UsermodBME68X::_nameDelCalib[] 		PROGMEM = "Del Calibration Hist";
 const char UsermodBME68X::_namePauseOnActWL[] 	PROGMEM = "Pause while WLED active";
 
 /* Private: Sensor names / Sensor short name */
 const char UsermodBME68X::_nameTemp[] 			PROGMEM = "Temperature";
 const char UsermodBME68X::_nameHum[] 			PROGMEM = "Humidity";
 const char UsermodBME68X::_namePress[] 			PROGMEM = "Pressure";
 const char UsermodBME68X::_nameGasRes[] 		PROGMEM = "Gas-Resistance";
 const char UsermodBME68X::_nameAHum[] 			PROGMEM = "Absolute-Humidity";
 const char UsermodBME68X::_nameDrewP[] 			PROGMEM = "Drew-Point";
 const char UsermodBME68X::_nameIaq[] 			PROGMEM = "IAQ";
 const char UsermodBME68X::_nameIaqVerb[] 		PROGMEM = "IAQ-Verbal";
 const char UsermodBME68X::_nameStaticIaq[] 		PROGMEM = "Static-IAQ";
 const char UsermodBME68X::_nameStaticIaqVerb[] 	PROGMEM = "Static-IAQ-Verbal";
 const char UsermodBME68X::_nameCo2[] 			PROGMEM = "CO2";
 const char UsermodBME68X::_nameVoc[] 			PROGMEM = "VOC";
 const char UsermodBME68X::_nameGasPer[] 		PROGMEM = "Gas-Percentage";
 const char UsermodBME68X::_nameIaqAc[] 			PROGMEM = "IAQ-Accuracy";
 const char UsermodBME68X::_nameStaticIaqAc[] 	PROGMEM = "Static-IAQ-Accuracy";
 const char UsermodBME68X::_nameCo2Ac[] 			PROGMEM = "CO2-Accuracy";
 const char UsermodBME68X::_nameVocAc[] 			PROGMEM = "VOC-Accuracy";
 const char UsermodBME68X::_nameGasPerAc[] 		PROGMEM = "Gas-Percentage-Accuracy";
 const char UsermodBME68X::_nameStabStatus[] 	PROGMEM = "Stab-Status";
 const char UsermodBME68X::_nameRunInStatus[] 	PROGMEM = "Run-In-Status";
 
 /* Private Units */
 const char UsermodBME68X::_unitTemp[] 			PROGMEM = " "; 				// NOTE - Is set with the selectable temperature unit
 const char UsermodBME68X::_unitHum[] 			PROGMEM = "%";
 const char UsermodBME68X::_unitPress[] 			PROGMEM = "hPa";
 const char UsermodBME68X::_unitGasres[] 		PROGMEM = "kΩ";
 const char UsermodBME68X::_unitAHum[] 			PROGMEM = "g/m³";
 const char UsermodBME68X::_unitDrewp[] 			PROGMEM = " "; 				// NOTE - Is set with the selectable temperature unit
 const char UsermodBME68X::_unitIaq[] 			PROGMEM = " ";   			// No unit
 const char UsermodBME68X::_unitStaticIaq[] 		PROGMEM = " ";				// No unit
 const char UsermodBME68X::_unitCo2[] 			PROGMEM = "ppm";
 const char UsermodBME68X::_unitVoc[] 			PROGMEM = "ppm"; 			
 const char UsermodBME68X::_unitGasPer[] 		PROGMEM = "%";
 const char UsermodBME68X::_unitNone[] 			PROGMEM = "";
 
 const char UsermodBME68X::_unitCelsius[] 		PROGMEM = "°C";				// Symbol for Celsius
 const char UsermodBME68X::_unitFahrenheit[] 	PROGMEM = "°F";				// Symbol for Fahrenheit
 
 /* Load Sensor Settings */
 const uint8_t UsermodBME68X::bsec_config_iaq[] = { 
	 #include "config/generic_33v_3s_28d/bsec_iaq.txt"		// Allow 28 days for calibration because the WLED module normally stays in the same place anyway
 }; 	
 
 
 /************************************************************************************************************/
 /********************************************* M A I N  C O D E *********************************************/
 /************************************************************************************************************/
 
 /**
  * @brief Called by WLED: Setup of the usermod
  */
 void UsermodBME68X::setup() {
	 DEBUG_PRINTLN(F(UMOD_DEBUG_NAME ESC_FGCOLOR_CYAN "Initialize" ESC_STYLE_RESET));
 
	 /* Check, if i2c is activated */
	 if (i2c_scl < 0 || i2c_sda < 0) {
		 settings.enabled = false;												// Disable usermod once i2c is not running
		 DEBUG_PRINTLN(F(UMOD_DEBUG_NAME "I2C is not activated. Please activate I2C first." GOGAB_FAIL));
		 return;
	 }
 
	 flags.InitSuccessful = true; 												// Will be set to false on need
 
	 /* Set data structure pointers */
	 ValuesPtr = &valuesA;   
	 PrevValuesPtr = &valuesB;
 
	 /* Init Library*/
	 iaqSensor.begin(settings.I2cadress, Wire); 									// BME68X_I2C_ADDR_LOW
	 stringbuff = "BSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
	 DEBUG_PRINT(F(UMOD_NAME));
	 DEBUG_PRINTLN(F(stringbuff.c_str()));
 
	 /* Init Sensor*/
	 iaqSensor.setConfig(bsec_config_iaq);
	 iaqSensor.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
	 iaqSensor.setTPH(BME68X_OS_2X, BME68X_OS_16X, BME68X_OS_1X); 				// Set the temperature, Pressure and Humidity over-sampling
	 iaqSensor.setTemperatureOffset(settings.tempOffset);         				// set the temperature offset in degree Celsius
	 loadState();                                                 				// Load the old calibration data
	 checkIaqSensorStatus();                                      				// Check the sensor status
	 // HomeAssistantDiscovery();
	 DEBUG_PRINTLN(F(INFO_COLUMN GOGAB_DONE));
 }
 
 /**
  * @brief Called by WLED: Main loop called by WLED
  *
  */
 void UsermodBME68X::loop() {
	 if (!settings.enabled || strip.isUpdating() || !flags.InitSuccessful) return;						// Leave if not enabled or string is updating or init failed
 
	 if (settings.pauseOnActiveWled && strip.getBrightness()) return;									// Workarround Known Issue: handing led update - Leave once pause on activ wled is active and wled is active
 
	 timer.actual = millis(); 																			// Timer to fetch new temperature, humidity and pressure data at intervals
 
	 if (timer.actual - timer.lastRun >= settings.Interval * 1000) {
		 timer.lastRun = timer.actual;
 
		 /* Get the sonsor measurments and publish them */
		 if (iaqSensor.run()) {				// iaqSensor.run()
			 getValues();	// Get the new values
 
			 if (ValuesPtr->temperature   != PrevValuesPtr->temperature   || !settings.PublischChange) {				// NOTE - negative dig means inactive
				 MQTT_publish(_nameTemp,   ValuesPtr->temperature, 	  settings.decimals.temperature);					
			 }
			 if (ValuesPtr->humidity      != PrevValuesPtr->humidity      || !settings.PublischChange) {
				 MQTT_publish(_nameHum,    ValuesPtr->humidity, 		  settings.decimals.humidity);
			 }
			 if (ValuesPtr->pressure      != PrevValuesPtr->pressure      || !settings.PublischChange) {
				 MQTT_publish(_namePress,  ValuesPtr->pressure, 		  settings.decimals.humidity);
			 }
			 if (ValuesPtr->gasResistance != PrevValuesPtr->gasResistance || !settings.PublischChange) {
				  MQTT_publish(_nameGasRes, ValuesPtr->gasResistance,   settings.decimals.gasResistance);
			 }
			 if (ValuesPtr->absHumidity 	 != PrevValuesPtr->absHumidity || !settings.PublischChange) {
				  MQTT_publish(_nameAHum,   PrevValuesPtr->absHumidity, settings.decimals.absHumidity);
			 }
			 if (ValuesPtr->drewPoint 	 !=  PrevValuesPtr->drewPoint || !settings.PublischChange) {
				 MQTT_publish(_nameDrewP,  PrevValuesPtr->drewPoint,   settings.decimals.drewPoint);
			 }
			 if (ValuesPtr->iaq != PrevValuesPtr->iaq || !settings.PublischChange) {
				 MQTT_publish(_nameIaq, ValuesPtr->iaq, settings.decimals.iaq);
				 if (settings.pubAcc) MQTT_publish(_nameIaqAc, 		ValuesPtr->iaqAccuracy, 			0);
				 if (settings.decimals.iaq>-1) {
					 if (settings.PublishIAQVerbal) {
						 if 		(ValuesPtr->iaq <= 50) 	cvalues.iaqVerbal = F("Excellent");
						 else if (ValuesPtr->iaq <= 100) cvalues.iaqVerbal = F("Good");
						 else if (ValuesPtr->iaq <= 150) cvalues.iaqVerbal = F("Lightly polluted");
						 else if (ValuesPtr->iaq <= 200) cvalues.iaqVerbal = F("Moderately polluted");
						 else if (ValuesPtr->iaq <= 250) cvalues.iaqVerbal = F("Heavily polluted");
						 else if (ValuesPtr->iaq <= 350)	cvalues.iaqVerbal = F("Severely polluted");
						 else 							cvalues.iaqVerbal = F("Extremely polluted");
						 snprintf_P(charbuffer, 127, PSTR("%s/%s"), mqttDeviceTopic, _nameIaqVerb);
						 if (WLED_MQTT_CONNECTED) mqtt->publish(charbuffer, 0, false, cvalues.iaqVerbal.c_str());
					 }
				 }
			 }			
			 if (ValuesPtr->staticIaq != PrevValuesPtr->staticIaq || !settings.PublischChange) {
				 MQTT_publish(_nameStaticIaq, ValuesPtr->staticIaq, settings.decimals.staticIaq);
				 if (settings.pubAcc) MQTT_publish(_nameStaticIaqAc, 	ValuesPtr->staticIaqAccuracy, 	0);			
				 if (settings.decimals.staticIaq>-1) {
					 if (settings.PublishIAQVerbal) {
						 if 		(ValuesPtr->staticIaq <= 50)  cvalues.staticIaqVerbal = F("Excellent");
						 else if (ValuesPtr->staticIaq <= 100) cvalues.staticIaqVerbal = F("Good");
						 else if (ValuesPtr->staticIaq <= 150) cvalues.staticIaqVerbal = F("Lightly polluted");
						 else if (ValuesPtr->staticIaq <= 200) cvalues.staticIaqVerbal = F("Moderately polluted");
						 else if (ValuesPtr->staticIaq <= 250) cvalues.staticIaqVerbal = F("Heavily polluted");
						 else if (ValuesPtr->staticIaq <= 350) cvalues.staticIaqVerbal = F("Severely polluted");
						 else 								  cvalues.staticIaqVerbal = F("Extremely polluted");
						 snprintf_P(charbuffer, 127, PSTR("%s/%s"), mqttDeviceTopic, _nameStaticIaqVerb);
						 if (WLED_MQTT_CONNECTED) mqtt->publish(charbuffer, 0, false, cvalues.staticIaqVerbal.c_str());
					 }
				 }			
			 }
			 if (ValuesPtr->co2 != PrevValuesPtr->co2 || !settings.PublischChange) {
				 MQTT_publish(_nameCo2, 			ValuesPtr->co2, 				settings.decimals.co2);
				 if (settings.pubAcc) MQTT_publish(_nameCo2Ac, 		ValuesPtr->co2Accuracy, 			0);
			 }
			 if (ValuesPtr->Voc != PrevValuesPtr->Voc || !settings.PublischChange) {
				 MQTT_publish(_nameVoc, 			ValuesPtr->Voc, 				settings.decimals.Voc);
				 if (settings.pubAcc) MQTT_publish(_nameVocAc, 		ValuesPtr->VocAccuracy, 			0);
			 }
			 if (ValuesPtr->gasPerc != PrevValuesPtr->gasPerc || !settings.PublischChange) {
				 MQTT_publish(_nameGasPer, 		ValuesPtr->gasPerc, 			settings.decimals.gasPerc);
				 if (settings.pubAcc) MQTT_publish(_nameGasPerAc, 	ValuesPtr->gasPercAccuracy, 		0);
			 }
 
			 /**** Publish Sensor State Entrys *****/
			 if ((ValuesPtr->stabStatus != PrevValuesPtr->stabStatus || !settings.PublischChange) && settings.publishSensorState) 			MQTT_publish(_nameStabStatus, 	ValuesPtr->stabStatus, 0);
			 if ((ValuesPtr->runInStatus != PrevValuesPtr->runInStatus || !settings.PublischChange) && settings.publishSensorState) 			MQTT_publish(_nameRunInStatus, 	ValuesPtr->runInStatus, 0);
 
			 /* Check accuracies - if accurasy level 3 is reached -> save calibration data */
			 if ((ValuesPtr->iaqAccuracy != PrevValuesPtr->iaqAccuracy) 				&& ValuesPtr->iaqAccuracy == 3) 		flags.SaveState = true; 						// Save after calibration / recalibration
			 if ((ValuesPtr->staticIaqAccuracy != PrevValuesPtr->staticIaqAccuracy) 	&& ValuesPtr->staticIaqAccuracy == 3) 	flags.SaveState = true;
			 if ((ValuesPtr->co2Accuracy != PrevValuesPtr->co2Accuracy) 				&& ValuesPtr->co2Accuracy == 3) 		flags.SaveState = true;
			 if ((ValuesPtr->VocAccuracy != PrevValuesPtr->VocAccuracy) 				&& ValuesPtr->VocAccuracy == 3) 		flags.SaveState = true;
			 if ((ValuesPtr->gasPercAccuracy != PrevValuesPtr->gasPercAccuracy) 		&& ValuesPtr->gasPercAccuracy == 3) 	flags.SaveState = true;
 
			 if (flags.SaveState) saveState(); 													// Save if the save state flag is set
		 }
	 }
 }
 
 /**
  * @brief Retrieves the sensor data and truncates it to the requested decimal places
  * 
  */
 void UsermodBME68X::getValues() {
	 /* Swap the point to the data structures */
	 swap = PrevValuesPtr;
	 PrevValuesPtr = ValuesPtr;
	 ValuesPtr = swap;
 
	 /* Float Values */
	 ValuesPtr->temperature = 	roundf(iaqSensor.temperature * 			powf(10, settings.decimals.temperature)) /	powf(10, settings.decimals.temperature);	
	 ValuesPtr->humidity = 		roundf(iaqSensor.humidity * 			powf(10, settings.decimals.humidity)) / 	powf(10, settings.decimals.humidity);
	 ValuesPtr->pressure = 		roundf(iaqSensor.pressure * 			powf(10, settings.decimals.pressure)) / 	powf(10, settings.decimals.pressure)		/100;      	// Pa 2 hPa
	 ValuesPtr->gasResistance = 	roundf(iaqSensor.gasResistance * 		powf(10, settings.decimals.gasResistance)) /powf(10, settings.decimals.gasResistance)	/1000;		// Ohm 2 KOhm
	 ValuesPtr->iaq = 			roundf(iaqSensor.iaq * 					powf(10, settings.decimals.iaq)) / 			powf(10, settings.decimals.iaq);   			
	 ValuesPtr->staticIaq = 		roundf(iaqSensor.staticIaq * 			powf(10, settings.decimals.staticIaq)) / 	powf(10, settings.decimals.staticIaq);					
	 ValuesPtr->co2 = 			roundf(iaqSensor.co2Equivalent * 		powf(10, settings.decimals.co2)) / 			powf(10, settings.decimals.co2);
	 ValuesPtr->Voc = 			roundf(iaqSensor.breathVocEquivalent * 	powf(10, settings.decimals.Voc)) / 			powf(10, settings.decimals.Voc);
	 ValuesPtr->gasPerc = 		roundf(iaqSensor.gasPercentage * 		powf(10, settings.decimals.gasPerc)) /		powf(10, settings.decimals.gasPerc);
 
	 /* Calculate Absolute Humidity [g/m³] */
	 if (settings.decimals.absHumidity>-1) {
		 const float mw = 18.01534;                                                                                                                                                                   				// molar mass of water g/mol
		 const float r = 8.31447215;                                                                                                                                                                  				// Universal gas constant J/mol/K
		 ValuesPtr->absHumidity = (6.112 * powf(2.718281828, (17.67 * ValuesPtr->temperature) / (ValuesPtr->temperature + 243.5)) * ValuesPtr->humidity * mw) / ((273.15 + ValuesPtr->temperature) * r); 		// in ppm
	 }
	 /* Calculate Drew Point (C°) */
	 if (settings.decimals.drewPoint>-1) {
		 ValuesPtr->drewPoint = (243.5 * (log( ValuesPtr->humidity / 100) + ((17.67 *  ValuesPtr->temperature) / (243.5 + ValuesPtr->temperature))) / (17.67 - log(ValuesPtr->humidity / 100) - ((17.67 * ValuesPtr->temperature) / (243.5 + ValuesPtr->temperature))));
	 }
 
	 /* Convert to Fahrenheit when selected */
	 if (settings.tempScale) {												// settings.tempScale = 0 => Celsius, = 1 => Fahrenheit
		 ValuesPtr->temperature =  ValuesPtr->temperature * 1.8 + 32;		// Value stored in Fahrenheit
		 ValuesPtr->drewPoint = ValuesPtr->drewPoint * 1.8 + 32;	
	 } 
 
	 /* Integer Values */
	 ValuesPtr->iaqAccuracy = 		iaqSensor.iaqAccuracy; 	
	 ValuesPtr->staticIaqAccuracy = 	iaqSensor.staticIaqAccuracy;
	 ValuesPtr->co2Accuracy = 		iaqSensor.co2Accuracy;
	 ValuesPtr->VocAccuracy = 		iaqSensor.breathVocAccuracy;
	 ValuesPtr->gasPercAccuracy = 	iaqSensor.gasPercentageAccuracy;
	 ValuesPtr->stabStatus = 		iaqSensor.stabStatus;		
	 ValuesPtr->runInStatus = 		iaqSensor.runInStatus;
 }
 
 
 /**
  * @brief Sends the current sensor data via MQTT
  * @param topic Suptopic of the sensor as const char
  * @param value Current sensor value as float
  */
 void UsermodBME68X::MQTT_publish(const char* topic, const float& value, const int8_t& dig) {
	 if (dig<0) return;
	 if (WLED_MQTT_CONNECTED) {
		 snprintf_P(charbuffer, 127, PSTR("%s/%s"), mqttDeviceTopic, topic);
		 mqtt->publish(charbuffer, 0, false, String(value, dig).c_str());
	 }
 }
 
 /**
  * @brief Called by WLED: Initialize the MQTT parts when the connection to the MQTT server is established.
  * @param bool Session Present
  */
 void UsermodBME68X::onMqttConnect(bool sessionPresent) {
	 DEBUG_PRINTLN(UMOD_DEBUG_NAME "OnMQTTConnect event fired");
	 HomeAssistantDiscovery();
 
	 if (!flags.MqttInitialized) {
		 flags.MqttInitialized=true;
		 DEBUG_PRINTLN(UMOD_DEBUG_NAME "MQTT first connect");
	 }
 }
 
 
 /**
  * @brief MQTT initialization to generate the mqtt topic strings. This initialization also creates the HomeAssistat device configuration (HA Discovery), which home assinstant automatically evaluates to create a device.
  */
 void UsermodBME68X::HomeAssistantDiscovery() {
	 if (!settings.HomeAssistantDiscovery || !flags.InitSuccessful || !settings.enabled) return; 									// Leave once HomeAssistant Discovery is inactive
 
	 DEBUG_PRINTLN(UMOD_DEBUG_NAME ESC_FGCOLOR_CYAN "Creating HomeAssistant Discovery Mqtt-Entrys" ESC_STYLE_RESET);
 
	 /* Sensor Values */
	 MQTT_PublishHASensor(_nameTemp,  		"TEMPERATURE", 				tempScale.c_str(), 	settings.decimals.temperature		); 		// Temperature
	 MQTT_PublishHASensor(_namePress, 		"ATMOSPHERIC_PRESSURE", 	_unitPress, 		settings.decimals.pressure			); 		// Pressure
	 MQTT_PublishHASensor(_nameHum, 			"HUMIDITY", 				_unitHum, 			settings.decimals.humidity			); 		// Humidity
	 MQTT_PublishHASensor(_nameGasRes,		"GAS", 						_unitGasres, 		settings.decimals.gasResistance		); 		// There is no device class for resistance in HA yet: https://developers.home-assistant.io/docs/core/entity/sensor/
	 MQTT_PublishHASensor(_nameAHum,			"HUMIDITY", 				_unitAHum, 			settings.decimals.absHumidity		); 		// Absolute Humidity
	 MQTT_PublishHASensor(_nameDrewP,		"TEMPERATURE", 				tempScale.c_str(), 	settings.decimals.drewPoint			); 		// Drew Point
	 MQTT_PublishHASensor(_nameIaq,			"AQI", 						_unitIaq, 			settings.decimals.iaq				); 		// IAQ
	 MQTT_PublishHASensor(_nameIaqVerb,		"", 						_unitNone,			settings.PublishIAQVerbal, 			2); 	// IAQ Verbal / Set Option 2 (text sensor)
	 MQTT_PublishHASensor(_nameStaticIaq,	"AQI",						_unitNone, 			settings.decimals.staticIaq			); 		// Static IAQ
	 MQTT_PublishHASensor(_nameStaticIaqVerb, "", 						_unitNone, 			settings.PublishStaticIAQVerbal, 	2); 	// IAQ Verbal / Set Option 2 (text sensor
	 MQTT_PublishHASensor(_nameCo2,			"CO2", 						_unitCo2, 			settings.decimals.co2				); 		// CO2
	 MQTT_PublishHASensor(_nameVoc,			"VOLATILE_ORGANIC_COMPOUNDS", _unitVoc, 		settings.decimals.Voc				); 		// VOC
	 MQTT_PublishHASensor(_nameGasPer,		"AQI", 						_unitGasPer, 		settings.decimals.gasPerc			); 		// Gas %
 
	 /* Accuracys - switched off once publishAccuracy=0 or the main value is switched of by digs set to a negative number */
	 MQTT_PublishHASensor(_nameIaqAc,		"AQI", 						_unitNone, 			settings.pubAcc - 1 + settings.decimals.iaq * settings.pubAcc, 			1); 	// Option 1: Diagnostics Sektion
	 MQTT_PublishHASensor(_nameStaticIaqAc,	"", 						_unitNone, 			settings.pubAcc - 1 + settings.decimals.staticIaq * settings.pubAcc, 	1);
	 MQTT_PublishHASensor(_nameCo2Ac,		"", 						_unitNone, 			settings.pubAcc - 1 + settings.decimals.co2 * settings.pubAcc, 			1);
	 MQTT_PublishHASensor(_nameVocAc,		"", 						_unitNone, 			settings.pubAcc - 1 + settings.decimals.Voc * settings.pubAcc, 			1);
	 MQTT_PublishHASensor(_nameGasPerAc,		"", 						_unitNone, 			settings.pubAcc - 1 + settings.decimals.gasPerc * settings.pubAcc, 		1);
	 
	 MQTT_PublishHASensor(_nameStabStatus,	"", 						_unitNone, 			settings.publishSensorState - 1, 1);
	 MQTT_PublishHASensor(_nameRunInStatus,	"", 						_unitNone, 			settings.publishSensorState - 1, 1);
 
	 DEBUG_PRINTLN(UMOD_DEBUG_NAME GOGAB_DONE);
 }
 
 /**
  * @brief These MQTT entries are responsible for the Home Assistant Discovery of the sensors. HA is shown here where to look for the sensor data. This entry therefore only needs to be sent once.
  *        Important note: In order to find everything that is sent from this device to Home Assistant via MQTT under the same device name, the "device/identifiers" entry must be the same.
  *        I use the MQTT device name here. If other user mods also use the HA Discovery, it is recommended to set the identifier the same. Otherwise you would have several devices,
  *        even though it is one device. I therefore only use the MQTT client name set in WLED here.
  * @param name Name of the sensor
  * @param topic Topic of the live sensor data
  * @param unitOfMeasurement Unit of the measurment
  * @param digs Number of decimal places
  * @param option Set to true if the sensor is part of diagnostics (dafault 0)
  */
 void UsermodBME68X::MQTT_PublishHASensor(const String& name, const String& deviceClass, const String& unitOfMeasurement, const int8_t& digs, const uint8_t& option) {
	 DEBUG_PRINT(UMOD_DEBUG_NAME "\t" + name);
	 
	 snprintf_P(charbuffer, 127, PSTR("%s/%s"), mqttDeviceTopic, name.c_str());				// Current values will be posted here
	 String basetopic = String(_hadtopic) + mqttClientID + F("/") + name + F("/config");   	// This is the place where Home Assinstant Discovery will check for new devices
 
	 if (digs < 0) { // if digs are set to -1 -> entry deactivated
		 /* Delete MQTT Entry */
		 if (WLED_MQTT_CONNECTED) {
			 mqtt->publish(basetopic.c_str(), 0, true, "");							// Send emty entry to delete
			 DEBUG_PRINTLN(INFO_COLUMN "deleted");
		 }
	 } else {
		 /* Create all the necessary HAD MQTT entrys - see: https://www.home-assistant.io/integrations/sensor.mqtt/#configuration-variables */
		 DynamicJsonDocument jdoc(700); // json document
																					 // See: https://www.home-assistant.io/integrations/mqtt/
		 JsonObject avail = jdoc.createNestedObject(F("avty"));						// 'avty': 'availability'
		 avail[F("topic")] = mqttDeviceTopic + String("/status"); // An MQTT topic subscribed to receive availability (online/offline) updates.
		 avail[F("payload_available")] = "online";
		 avail[F("payload_not_available")] = "offline";
		 JsonObject device = jdoc.createNestedObject(F("device")); // Information about the device this sensor is a part of to tie it into the device registry. Only works when unique_id is set. At least one of identifiers or connections must be present to identify the device.
		 device[F("name")] = serverDescription;
		 device[F("identifiers")] = String(mqttClientID);
		 device[F("manufacturer")] = F("WLED");
		 device[F("model")] = UMOD_DEVICE;
		 device[F("sw_version")] = versionString;
		 device[F("hw_version")] = F(HARDWARE_VERSION);
 
		 if (deviceClass != "") jdoc[F("device_class")] = deviceClass; 						// The type/class of the sensor to set the icon in the frontend. The device_class can be null
		 if (option == 1) jdoc[F("entity_category")] = "diagnostic"; 						// Option 1: The category of the entity | When set, the entity category must be diagnostic for sensors.
		 if (option == 2) jdoc[F("mode")] = "text";                             				// Option 2: Set text mode |
		 jdoc[F("expire_after")] = 1800;           											// If set, it defines the number of seconds after the sensor’s state expires, if it’s not updated. After expiry, the sensor’s state becomes unavailable. Default the sensors state never expires.
		 jdoc[F("name")] = name; 															// The name of the MQTT sensor. Without server/module/device name. The device name will be added by HomeAssinstant anyhow
		 if (unitOfMeasurement != "") jdoc[F("state_class")] = "measurement";        		// NOTE: This entry is missing in some other usermods. But it is very important. Because only with this entry, you can use statistics (such as statistical graphs).
		 jdoc[F("state_topic")] = charbuffer;                      							// The MQTT topic subscribed to receive sensor values. If device_class, state_class, unit_of_measurement or suggested_display_precision is set, and a numeric value is expected, an empty value '' will be ignored and will not update the state, a 'null' value will set the sensor to an unknown state. The device_class can be null.
		 jdoc[F("unique_id")] = String(mqttClientID) + "-" + name; 							// An ID that uniquely identifies this sensor. If two sensors have the same unique ID, Home Assistant will raise an exception.
		 if (unitOfMeasurement != "") jdoc[F("unit_of_measurement")] = unitOfMeasurement; 	// Defines the units of measurement of the sensor, if any. The unit_of_measurement can be null.
 
		 DEBUG_PRINTF(" (%d bytes)", jdoc.memoryUsage());
 
		 stringbuff = "";                                                            		// clear string buffer
		 serializeJson(jdoc, stringbuff); 													// JSON to String
 
		 if (WLED_MQTT_CONNECTED) {                                         					// Check if MQTT Connected, otherwise it will crash the 8266
			 mqtt->publish(basetopic.c_str(), 0, true, stringbuff.c_str()); 					// Publish the HA discovery sensor entry
			 DEBUG_PRINTLN(INFO_COLUMN "published");
		 }
	 }
 }
 
 /**
  * @brief Called by WLED: Publish Sensor Information to Info Page
  * @param JsonObject Pointer
  */
 void UsermodBME68X::addToJsonInfo(JsonObject& root) {
	 //DEBUG_PRINTLN(F(UMOD_DEBUG_NAME "Add to info event"));
	 JsonObject user = root[F("u")];
 
	 if (user.isNull())
		 user = root.createNestedObject(F("u"));
 
	 if (!flags.InitSuccessful) {
		 // Init was not seccessful - let the user know
		 JsonArray temperature_json = user.createNestedArray(F("BME68X Sensor"));
		 temperature_json.add(F("not found"));
		 JsonArray humidity_json = user.createNestedArray(F("BMW68x Reason"));
		 humidity_json.add(InfoPageStatusLine);
	 }
	 else if (!settings.enabled) {
		 JsonArray temperature_json = user.createNestedArray(F("BME68X Sensor"));
		 temperature_json.add(F("disabled"));
	 }
	 else {
		 InfoHelper(user, _nameTemp, 		ValuesPtr->temperature, 		settings.decimals.temperature, 		tempScale.c_str());
		 InfoHelper(user, _nameHum, 			ValuesPtr->humidity, 			settings.decimals.humidity, 		_unitHum);
		 InfoHelper(user, _namePress, 		ValuesPtr->pressure, 			settings.decimals.pressure, 		_unitPress);
		 InfoHelper(user, _nameGasRes,		ValuesPtr->gasResistance, 		settings.decimals.gasResistance,	_unitGasres);
		 InfoHelper(user, _nameAHum, 		ValuesPtr->absHumidity, 		settings.decimals.absHumidity, 		_unitAHum);
		 InfoHelper(user, _nameDrewP, 		ValuesPtr->drewPoint, 			settings.decimals.drewPoint, 		tempScale.c_str());
		 InfoHelper(user, _nameIaq, 			ValuesPtr->iaq, 				settings.decimals.iaq, 				_unitIaq);
		 InfoHelper(user, _nameIaqVerb, 		cvalues.iaqVerbal, 				settings.PublishIAQVerbal);
		 InfoHelper(user, _nameStaticIaq,	ValuesPtr->staticIaq, 			settings.decimals.staticIaq, 		_unitStaticIaq);
		 InfoHelper(user, _nameStaticIaqVerb,cvalues.staticIaqVerbal, 		settings.PublishStaticIAQVerbal);
		 InfoHelper(user, _nameCo2, 			ValuesPtr->co2, 				settings.decimals.co2, 				_unitCo2);
		 InfoHelper(user, _nameVoc, 			ValuesPtr->Voc, 				settings.decimals.Voc, 				_unitVoc);
		 InfoHelper(user, _nameGasPer, 		ValuesPtr->gasPerc, 			settings.decimals.gasPerc, 			_unitGasPer);
 
		 if (settings.pubAcc) {
			 if (settings.decimals.iaq >= 0) 		InfoHelper(user, _nameIaqAc, 		ValuesPtr->iaqAccuracy, 		0, " ");
			 if (settings.decimals.staticIaq >= 0) 	InfoHelper(user, _nameStaticIaqAc, 	ValuesPtr->staticIaqAccuracy, 	0, " ");
			 if (settings.decimals.co2 >= 0)			InfoHelper(user, _nameCo2Ac, 		ValuesPtr->co2Accuracy, 		0, " ");
			 if (settings.decimals.Voc >= 0)			InfoHelper(user, _nameVocAc, 		ValuesPtr->VocAccuracy, 		0, " ");
			 if (settings.decimals.gasPerc >= 0)		InfoHelper(user, _nameGasPerAc, 	ValuesPtr->gasPercAccuracy, 	0, " ");
		 }
 
		 if (settings.publishSensorState) {
			 InfoHelper(user, _nameStabStatus, 	ValuesPtr->stabStatus, 	0, " ");
			 InfoHelper(user, _nameRunInStatus, 	ValuesPtr->runInStatus, 0, " ");
		 }
	 }
 }
 
 /**
  * @brief Info Page helper function
  * @param root JSON object
  * @param name Name of the sensor as char
  * @param sensorvalue Value of the sensor as float
  * @param decimals Decimal places of the value
  * @param unit Unit of the sensor
  */
 void UsermodBME68X::InfoHelper(JsonObject& root, const char* name, const float& sensorvalue, const int8_t& decimals, const char* unit) {
	 if (decimals > -1) {
		 JsonArray sub_json = root.createNestedArray(name);
		 sub_json.add(roundf(sensorvalue * powf(10, decimals)) / powf(10, decimals));
		 sub_json.add(unit);
	 }
 }
 
 /**
  * @brief Info Page helper function (overload)
  * @param root JSON object
  * @param name Name of the sensor
  * @param sensorvalue Value of the sensor as string
  * @param status Status of the value (active/inactive)
  */
 void UsermodBME68X::InfoHelper(JsonObject& root, const char* name, const String& sensorvalue, const bool& status) {
	 if (status) {
		 JsonArray sub_json = root.createNestedArray(name);
		 sub_json.add(sensorvalue);
	 }
 }
 
 /**
  * @brief Called by WLED: Adds the usermodul neends on the config page for user modules
  * @param JsonObject Pointer
  *
  * @see Usermod::addToConfig()
  * @see UsermodManager::addToConfig()
  */
 void UsermodBME68X::addToConfig(JsonObject& root) {
	 DEBUG_PRINT(F(UMOD_DEBUG_NAME "Creating configuration pages content: "));
 
	 JsonObject top = root.createNestedObject(FPSTR(UMOD_NAME));
	 /* general settings */
	 top[FPSTR(_enabled)] = 						settings.enabled;
	 top[FPSTR(_nameI2CAdr)] = 					settings.I2cadress;
	 top[FPSTR(_nameInterval)] = 				settings.Interval;
	 top[FPSTR(_namePublishChange)] =		 	settings.PublischChange;
	 top[FPSTR(_namePubAc)] = 					settings.pubAcc;
	 top[FPSTR(_namePubSenState)] = 				settings.publishSensorState;
	 top[FPSTR(_nameTempScale)] = 				settings.tempScale;
	 top[FPSTR(_nameTempOffset)] = 				settings.tempOffset;
	 top[FPSTR(_nameHADisc)] = 					settings.HomeAssistantDiscovery;
	 top[FPSTR(_namePauseOnActWL)] = 			settings.pauseOnActiveWled;
	 top[FPSTR(_nameDelCalib)] = 				flags.DeleteCaibration;
 
	 /* Digs */
	 JsonObject sensors_json = top.createNestedObject("Sensors");
	 sensors_json[FPSTR(_nameTemp)] = 			settings.decimals.temperature;
	 sensors_json[FPSTR(_nameHum)] = 			settings.decimals.humidity;
	 sensors_json[FPSTR(_namePress)] = 			settings.decimals.pressure;
	 sensors_json[FPSTR(_nameGasRes)] = 			settings.decimals.gasResistance;
	 sensors_json[FPSTR(_nameAHum)] = 			settings.decimals.absHumidity;
	 sensors_json[FPSTR(_nameDrewP)] = 			settings.decimals.drewPoint;
	 sensors_json[FPSTR(_nameIaq)] = 			settings.decimals.iaq;
	 sensors_json[FPSTR(_nameIaqVerb)] = 		settings.PublishIAQVerbal;
	 sensors_json[FPSTR(_nameStaticIaq)] = 		settings.decimals.staticIaq;
	 sensors_json[FPSTR(_nameStaticIaqVerb)] = 	settings.PublishStaticIAQVerbal;
	 sensors_json[FPSTR(_nameCo2)] = 			settings.decimals.co2;
	 sensors_json[FPSTR(_nameVoc)] = 			settings.decimals.Voc;
	 sensors_json[FPSTR(_nameGasPer)] =			settings.decimals.gasPerc;
 
	 DEBUG_PRINTLN(F(GOGAB_OK));
 }
 
 /**
  * @brief Called by WLED: Add dropdown and additional infos / structure
  * @see Usermod::appendConfigData()
  * @see UsermodManager::appendConfigData()
  */
 void UsermodBME68X::appendConfigData() {
	 // snprintf_P(charbuffer, 127, PSTR("addInfo('%s:%s',1,'read interval [seconds]');"), UMOD_NAME, _nameInterval); oappend(charbuffer);
	 // snprintf_P(charbuffer, 127, PSTR("addInfo('%s:%s',1,'only if value changes');"), UMOD_NAME, _namePublishChange); oappend(charbuffer);
	 // snprintf_P(charbuffer, 127, PSTR("addInfo('%s:%s',1,'maximum age of a message in seconds');"), UMOD_NAME, _nameMaxAge); oappend(charbuffer);
	 // snprintf_P(charbuffer, 127, PSTR("addInfo('%s:%s',1,'Gas related values are only published after the gas sensor has been calibrated');"), UMOD_NAME, _namePubAfterCalib); oappend(charbuffer);
	 // snprintf_P(charbuffer, 127, PSTR("addInfo('%s:%s',1,'*) Set to minus to deactivate (all sensors)');"), UMOD_NAME, _nameTemp); oappend(charbuffer);
 
	 /* Dropdown for Celsius/Fahrenheit*/
	 oappend(F("dd=addDropdown('"));
	 oappend(UMOD_NAME);
	 oappend(F("','"));
	 oappend(_nameTempScale);
	 oappend(F("');"));
	 oappend(F("addOption(dd,'Celsius',0);"));
	 oappend(F("addOption(dd,'Fahrenheit',1);"));
 
	 /* i²C Address*/
	 oappend(F("dd=addDropdown('"));
	 oappend(UMOD_NAME);
	 oappend(F("','"));
	 oappend(_nameI2CAdr);
	 oappend(F("');"));
	 oappend(F("addOption(dd,'0x76',0x76);"));
	 oappend(F("addOption(dd,'0x77',0x77);"));
 }
 
 /**
  * @brief Called by WLED: Read Usermod Config Settings default settings values could be set here (or below using the 3-argument getJsonValue()) 
  * 		  instead of in the class definition or constructor setting them inside readFromConfig() is slightly more robust, handling the rare but 
  * 		  plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)
  *        This is called whenever WLED boots and loads cfg.json, or when the UM config
  *        page is saved. Will properly re-instantiate the SHT class upon type change and
  *        publish HA discovery after enabling.
  * 		  NOTE: Here are the default settings of the user module 
  * @param JsonObject Pointer
  * @return bool
  * @see Usermod::readFromConfig()
  * @see UsermodManager::readFromConfig()
  */
 bool UsermodBME68X::readFromConfig(JsonObject& root) {
	 DEBUG_PRINT(F(UMOD_DEBUG_NAME "Reading configuration: "));
 
	 JsonObject top = root[FPSTR(UMOD_NAME)];
	 bool configComplete = !top.isNull();
 
	 /* general settings */ 																							/* DEFAULTS */
	 configComplete &= getJsonValue(top[FPSTR(_enabled)], 						settings.enabled, 							1		);		// Usermod enabled per default
	 configComplete &= getJsonValue(top[FPSTR(_nameI2CAdr)], 					settings.I2cadress, 						0x77	);		// Defalut IC2 adress set to 0x77 (some modules are set to 0x76)
	 configComplete &= getJsonValue(top[FPSTR(_nameInterval)], 					settings.Interval, 							1		);		// Executed every second
	 configComplete &= getJsonValue(top[FPSTR(_namePublishChange)], 				settings.PublischChange, 					false	);		// Publish changed values only
	 configComplete &= getJsonValue(top[FPSTR(_nameTempScale)], 					settings.tempScale, 						0		);		// Temp sale set to Celsius (1=Fahrenheit)
	 configComplete &= getJsonValue(top[FPSTR(_nameTempOffset)], 				settings.tempOffset, 						0		);		// Temp offset is set to 0 (Celsius)
	 configComplete &= getJsonValue(top[FPSTR(_namePubSenState)], 				settings.publishSensorState, 				1		);		// Publish the sensor states
	 configComplete &= getJsonValue(top[FPSTR(_namePubAc)], 						settings.pubAcc, 							1		);		// Publish accuracy values 
	 configComplete &= getJsonValue(top[FPSTR(_nameHADisc)], 					settings.HomeAssistantDiscovery, 			true	);		// Activate HomeAssistant Discovery (this Module will be shown as MQTT device in HA)
	 configComplete &= getJsonValue(top[FPSTR(_namePauseOnActWL)],				settings.pauseOnActiveWled,					false	);		// Pause on active WLED not activated per default
	 configComplete &= getJsonValue(top[FPSTR(_nameDelCalib)], 					flags.DeleteCaibration, 					false	);		// IF checked the calibration file will be delete when the save button is pressed
 
	 /* Decimal places */																							/* no of digs / -1 means deactivated */
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameTemp)], 			settings.decimals.temperature, 				1		);		// One decimal places
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameHum)], 			settings.decimals.humidity, 				1		);
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_namePress)], 			settings.decimals.pressure, 				0		);		// Zero decimal places
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameGasRes)], 			settings.decimals.gasResistance,			-1		);		// deavtivated
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameDrewP)], 			settings.decimals.drewPoint, 				1		);
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameAHum)], 			settings.decimals.absHumidity, 				1		);
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameIaq)], 			settings.decimals.iaq, 						0		);		// Index for Air Quality Number is active
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameIaqVerb)], 		settings.PublishIAQVerbal, 					-1		); 		// deactivated - Index for Air Quality (IAQ) verbal classification
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameStaticIaq)], 		settings.decimals.staticIaq, 				0		);		// activated - Static IAQ is better than IAQ for devices that are not moved
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameStaticIaqVerb)], 	settings.PublishStaticIAQVerbal,			0		);		// activated
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameCo2)], 			settings.decimals.co2, 						0		);
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameVoc)], 			settings.decimals.Voc, 						0		);
	 configComplete &= getJsonValue(top["Sensors"][FPSTR(_nameGasPer)], 			settings.decimals.gasPerc, 					0		);
 
	 DEBUG_PRINTLN(F(GOGAB_OK));	
 
	 /* Set the selected temperature unit */
	 if (settings.tempScale) {
		 tempScale = F(_unitFahrenheit);
	 }
	 else {
		 tempScale = F(_unitCelsius);
	 }
 
	 if (flags.DeleteCaibration) {
		 DEBUG_PRINT(F(UMOD_DEBUG_NAME "Deleting Calibration File"));
		 flags.DeleteCaibration = false;
		 if (WLED_FS.remove(CALIB_FILE_NAME)) {
			 DEBUG_PRINTLN(F(GOGAB_OK));
		 }
		 else {
			 DEBUG_PRINTLN(F(GOGAB_FAIL));
		 }
	 }
 
	 if (settings.Interval < 1) settings.Interval = 1;                           // Correct interval on need (A number less than 1 is not permitted)
	 iaqSensor.setTemperatureOffset(settings.tempOffset); 						// Set Temp Offset
 
	 return configComplete;
 }
 
 /**
  * @brief Called by WLED: Retunrs the user modul id number
  * 
  * @return uint16_t User module number
  */
 uint16_t UsermodBME68X::getId() {
	 return USERMOD_ID_BME68X;
 }
 
 
 /**
  * @brief Returns the current temperature in the scale which is choosen in settings
  * @return Temperature value (°C or °F as choosen in settings)
 */
 inline float UsermodBME68X::getTemperature() {
	 return ValuesPtr->temperature;
 }
 
 /**
  * @brief Returns the current humidity
  * @return Humididty value (%)
 */
 inline float UsermodBME68X::getHumidity() {
   return ValuesPtr->humidity;
 }
 
 /**
  * @brief Returns the current pressure
  * @return Pressure value (hPa)
 */
 inline float UsermodBME68X::getPressure() {
   return ValuesPtr->pressure;
 }
 
 /**
  * @brief Returns the current gas resistance
  * @return Gas resistance value (kΩ)
 */
 inline float UsermodBME68X::getGasResistance() {
   return ValuesPtr->gasResistance;
 }
 
 /**
  * @brief Returns the current absolute humidity
  * @return Absolute humidity value (g/m³)
 */
 inline float UsermodBME68X::getAbsoluteHumidity() {
   return ValuesPtr->absHumidity;
 }
 
 /**
  * @brief Returns the current dew point
  * @return Dew point (°C or °F as choosen in settings)
 */
 inline float UsermodBME68X::getDewPoint() {
   return ValuesPtr->drewPoint;
 }
 
 /**
  * @brief Returns the current iaq (Indoor Air Quallity)
  * @return Iaq value (0-500)
 */
 inline float UsermodBME68X::getIaq() {
   return ValuesPtr->iaq;
 }
 
 /**
  * @brief Returns the current static iaq (Indoor Air Quallity) (NOTE: Static iaq is the better choice than iaq for fixed devices such as the wled module)
  * @return Static iaq value (float)
 */
 inline float UsermodBME68X::getStaticIaq() {
   return ValuesPtr->staticIaq;
 }
 
 /**
  * @brief Returns the current co2
  * @return Co2 value (ppm)
 */
 inline float UsermodBME68X::getCo2() {
   return ValuesPtr->co2;
 }
 
 /**
  * @brief Returns the current voc (Breath VOC concentration estimate [ppm])
  * @return Voc value (ppm)
 */
 inline float UsermodBME68X::getVoc() {
   return ValuesPtr->Voc;
 }
 
 /**
  * @brief Returns the current gas percentage
  * @return Gas percentage value (%)
 */
 inline float UsermodBME68X::getGasPerc() {
   return ValuesPtr->gasPerc;
 }
 
 /**
  * @brief Returns the current iaq accuracy (0 = not calibrated, 2 = being calibrated, 3 = calibrated)
  * @return Iaq accuracy value (0-3)
 */
 inline uint8_t UsermodBME68X::getIaqAccuracy() {
   return ValuesPtr->iaqAccuracy ;
 }
 
 /**
  * @brief Returns the current static iaq accuracy accuracy (0 = not calibrated, 2 = being calibrated, 3 = calibrated)
  * @return Static iaq accuracy value (0-3)
 */
 inline uint8_t UsermodBME68X::getStaticIaqAccuracy() {
   return ValuesPtr->staticIaqAccuracy;
 }
 
 /**
  * @brief Returns the current co2 accuracy (0 = not calibrated, 2 = being calibrated, 3 = calibrated)
  * @return Co2 accuracy  value (0-3)
 */
 inline uint8_t UsermodBME68X::getCo2Accuracy() {
   return ValuesPtr->co2Accuracy;
 }
 
 /**
  * @brief Returns the current voc accuracy (0 = not calibrated, 2 = being calibrated, 3 = calibrated)
  * @return Voc accuracy  value (0-3)
 */
 inline uint8_t UsermodBME68X::getVocAccuracy() {
   return ValuesPtr->VocAccuracy;
 } 
 
 /**
  * @brief Returns the current gas percentage accuracy (0 = not calibrated, 2 = being calibrated, 3 = calibrated)
  * @return Gas percentage accuracy value (0-3)
 */
 inline uint8_t UsermodBME68X::getGasPercAccuracy() {
   return ValuesPtr->gasPercAccuracy;
 }
 
 /**
  * @brief Returns the current stab status.
  * 		  Indicates when the sensor is ready after after switch-on
  * @return stab status value (0 = switched on / 1 = stabilized)
 */
 inline bool  UsermodBME68X::getStabStatus() {
   return ValuesPtr->stabStatus;
 }
 
 /**
  * @brief Returns the current run in status. 
  * 		  Indicates if the sensor is undergoing initial stabilization during its first use after production
  * @return Tun status accuracy value (0 = switched on first time / 1 = stabilized)
 */
 inline bool UsermodBME68X::getRunInStatus() {
   return ValuesPtr->runInStatus;
 }
 
 
 /**
  * @brief Checks whether the library and the sensor are running.
  */
 void UsermodBME68X::checkIaqSensorStatus() {
 
	 if (iaqSensor.bsecStatus != BSEC_OK) {
		 InfoPageStatusLine = "BSEC Library ";
		 DEBUG_PRINT(UMOD_DEBUG_NAME + InfoPageStatusLine);
		 flags.InitSuccessful = false;
		 if (iaqSensor.bsecStatus < BSEC_OK) {
			 InfoPageStatusLine += " Error Code : " + String(iaqSensor.bsecStatus);
			 DEBUG_PRINTLN(GOGAB_FAIL);
		 }
		 else {
			 InfoPageStatusLine += " Warning Code : " + String(iaqSensor.bsecStatus);
			 DEBUG_PRINTLN(GOGAB_WARN);
		 }
	 }
	 else {
		 InfoPageStatusLine = "Sensor BME68X ";
		 DEBUG_PRINT(UMOD_DEBUG_NAME + InfoPageStatusLine);
 
		 if (iaqSensor.bme68xStatus != BME68X_OK) {
			 flags.InitSuccessful = false;
			 if (iaqSensor.bme68xStatus < BME68X_OK) {
				 InfoPageStatusLine += "error code: " + String(iaqSensor.bme68xStatus);
				 DEBUG_PRINTLN(GOGAB_FAIL);
			 }
			 else {
				 InfoPageStatusLine += "warning code: " + String(iaqSensor.bme68xStatus);
				 DEBUG_PRINTLN(GOGAB_WARN);
			 }
		 }
		 else {
			 InfoPageStatusLine += F("OK");
			 DEBUG_PRINTLN(GOGAB_OK);
		 }
	 }
 }
 
 /**
  * @brief Loads the calibration data from the file system of the device
  */
 void UsermodBME68X::loadState() {
	 if (WLED_FS.exists(CALIB_FILE_NAME)) {
		 DEBUG_PRINT(F(UMOD_DEBUG_NAME "Read the calibration file: "));
		 File file = WLED_FS.open(CALIB_FILE_NAME, FILE_READ);
		 if (!file) {
			 DEBUG_PRINTLN(GOGAB_FAIL);
		 }
		 else {
			 file.read(bsecState, BSEC_MAX_STATE_BLOB_SIZE);
			 file.close();
			 DEBUG_PRINTLN(GOGAB_OK);
			 iaqSensor.setState(bsecState);
		 }
	 }
	 else {
		 DEBUG_PRINTLN(F(UMOD_DEBUG_NAME "Calibration file not found."));
	 }
 }
 
 /**
  * @brief Saves the calibration data from the file system of the device
  */
 void UsermodBME68X::saveState() {
	 DEBUG_PRINT(F(UMOD_DEBUG_NAME "Write the calibration file  "));
	 File file = WLED_FS.open(CALIB_FILE_NAME, FILE_WRITE);
	 if (!file) {
		 DEBUG_PRINTLN(GOGAB_FAIL);
	 }
	 else {
		 iaqSensor.getState(bsecState);
		 file.write(bsecState, BSEC_MAX_STATE_BLOB_SIZE);
		 file.close();
		 stateUpdateCounter++;
		 DEBUG_PRINTF("(saved %d times)" GOGAB_OK "\n", stateUpdateCounter);
		 flags.SaveState = false; // Clear save state flag
 
		 char contbuffer[30];
 
		 /* Timestamp */
		 time_t curr_time;
		 tm* curr_tm;
		 time(&curr_time);
		 curr_tm = localtime(&curr_time);
 
		 snprintf_P(charbuffer, 127, PSTR("%s/%s"), mqttDeviceTopic, UMOD_NAME "/Calib Last Run");
		 strftime(contbuffer, 30, "%d %B %Y - %T", curr_tm);
		 if (WLED_MQTT_CONNECTED) mqtt->publish(charbuffer, 0, false, contbuffer);
 
		 snprintf(contbuffer, 30, "%d", stateUpdateCounter);
		 snprintf_P(charbuffer, 127, PSTR("%s/%s"), mqttDeviceTopic, UMOD_NAME "/Calib Count");
		 if (WLED_MQTT_CONNECTED) mqtt->publish(charbuffer, 0, false, contbuffer);
	 }
 }
 
 
 static UsermodBME68X bme68x_v2;
 REGISTER_USERMOD(bme68x_v2);