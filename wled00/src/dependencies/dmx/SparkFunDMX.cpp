/******************************************************************************
SparkFunDMX.h
Arduino Library for the SparkFun ESP32 LED to DMX Shield
Andy England @ SparkFun Electronics
7/22/2019

Development environment specifics:
Arduino IDE 1.6.4

This code is released under the [MIT License](http://opensource.org/licenses/MIT).
Please review the LICENSE.md file included with this example. If you have any questions 
or concerns with licensing, please contact techsupport@sparkfun.com.
Distributed as-is; no warranty is given.
******************************************************************************/

/* ----- LIBRARIES ----- */
#ifdef ESP32

#include <Arduino.h>

#include "SparkFunDMX.h"
#include <HardwareSerial.h>

#define dmxMaxChannel  512
#define defaultMax 32

#define DMXSPEED       250000
#define DMXFORMAT      SERIAL_8N2
#define BREAKSPEED     83333
#define BREAKFORMAT    SERIAL_8N1

int enablePin = -1;		// disable the enable pin because it is not needed
int rxPin = -1;       // disable the receiving pin because it is not needed
int txPin = 2;        // transmit DMX data over this pin (default is pin 2)

//DMX value array and size. Entry 0 will hold startbyte
uint8_t dmxData[dmxMaxChannel] = {};
int chanSize;
int currentChannel = 0;

HardwareSerial DMXSerial(2);

/* Interrupt Timer for DMX Receive */
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile int _interruptCounter;
volatile bool _startCodeDetected = false;


/* Start Code is detected by 21 low interrupts */
void IRAM_ATTR onTimer() {
	if (digitalRead(rxPin) == 1)
	{
		_interruptCounter = 0; //If the RX Pin is high, we are not in an interrupt
	}
	else
	{
		_interruptCounter++;
	}
	if (_interruptCounter > 9)
	{	
		portENTER_CRITICAL_ISR(&timerMux);
		_startCodeDetected = true;
		DMXSerial.begin(DMXSPEED, DMXFORMAT, rxPin, txPin);
		portEXIT_CRITICAL_ISR(&timerMux);
		_interruptCounter = 0;
	}
}

void SparkFunDMX::initRead(int chanQuant) {
	
  timer = timerBegin(0, 1, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 320, true);
  timerAlarmEnable(timer);
  _READWRITE = _READ;
  if (chanQuant > dmxMaxChannel || chanQuant <= 0) 
  {
    chanQuant = defaultMax;
  }
  chanSize = chanQuant;
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW);
  pinMode(rxPin, INPUT);
}

// Set up the DMX-Protocol
void SparkFunDMX::initWrite (int chanQuant) {

  _READWRITE = _WRITE;
  if (chanQuant > dmxMaxChannel || chanQuant <= 0) {
    chanQuant = defaultMax;
  }

  chanSize = chanQuant + 1; //Add 1 for start code

  DMXSerial.begin(DMXSPEED, DMXFORMAT, rxPin, txPin);
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, HIGH);
}

// Function to read DMX data
uint8_t SparkFunDMX::read(int Channel) {
  if (Channel > chanSize) Channel = chanSize;
  return(dmxData[Channel - 1]); //subtract one to account for start byte
}

// Function to send DMX data
void SparkFunDMX::write(int Channel, uint8_t value) {
  if (Channel < 0) Channel = 0;
  if (Channel > chanSize) chanSize = Channel;
  dmxData[0] = 0;
  dmxData[Channel] = value; //add one to account for start byte
}



void SparkFunDMX::update() {
  if (_READWRITE == _WRITE)
  {
    //Send DMX break
    digitalWrite(txPin, HIGH);
    DMXSerial.begin(BREAKSPEED, BREAKFORMAT, rxPin, txPin);//Begin the Serial port
    DMXSerial.write(0);
    DMXSerial.flush();
    delay(1);
    DMXSerial.end();
    
    //Send DMX data
    DMXSerial.begin(DMXSPEED, DMXFORMAT, rxPin, txPin);//Begin the Serial port
    DMXSerial.write(dmxData, chanSize);
    DMXSerial.flush();
    DMXSerial.end();//clear our DMX array, end the Hardware Serial port
  }
  else if (_READWRITE == _READ)//In a perfect world, this function ends serial communication upon packet completion and attaches RX to a CHANGE interrupt so the start code can be read again
  { 
	if (_startCodeDetected == true)
	{
		while (DMXSerial.available())
		{
			dmxData[currentChannel++] = DMXSerial.read();
		}
	if (currentChannel > chanSize) //Set the channel counter back to 0 if we reach the known end size of our packet
	{
		
      portENTER_CRITICAL(&timerMux);
	  _startCodeDetected = false;
	  DMXSerial.flush();
	  DMXSerial.end();
      portEXIT_CRITICAL(&timerMux);
	  currentChannel = 0;
	}
	}
  }
}

// Function to update the DMX bus

#endif