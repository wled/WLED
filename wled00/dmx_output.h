/**
 * Unified DMX Output class for all platforms.
 *
 * ESP8266 uses the hw FIFO for sending frames. This is apparently 128 Bytes in size -> the 513 Bytes DMX frame will
 * still take a big chunk of processor time.
 * For all other architectures, the HAL ring buffer is activated, so that update() calls are almost non-blocking, but
 * the first BREAK and MAB pulses of approx 150us in case of this implementation.
 *
 * ESP8266 uses some timeout in the busy() check that is needed according to the Arduino library code. This means
 * calling busy() blocks approx 50us if the TX FIFO is empty to make sure really no transfer is ongoing anymore.
 * busy() checks are implemented in the update() and handleDMXOutput() functions.
 *
 * Reducing the number of DMX channels makes no sense for ESP32, but to save a few Bytes of RAM.
 * It would make sense for ESP8266, but is not implemented, yet. One would have to dynamically allocate the _dmxData
 * array
 */

#ifndef DMX_OUTPUT_H
#define DMX_OUTPUT_H

#define DMX_CHANNEL_TOP 512
#define DMX_CHANNELS DMX_CHANNEL_TOP + 1

#define DMXSPEED       250000
#define DMXFORMAT      SERIAL_8N2
#define BREAKSPEED     83000
#define BREAKFORMAT    SERIAL_8N1   // unused, instead, DMXFORMAT is used


class DMXOutput {
  public:
    ~DMXOutput();
    /**
    * Initialize DMXOutput.
    * Use _outputPin_ for TX.
    * _updateRate_ specifies update rate in Hz. Use 0 for max.
    * Use Serial _uartNo_. Specify -1 for default, which is the highest one available.
    */
    bool init(uint8_t outputPin, uint8_t updateRate = 44, int8_t uartNo = -1);
    void end();
    /**
     * Write one DMX _channel_ to _value_
     */
    void write(uint16_t channel, uint8_t value);
    /**
     * Write _len_ Bytes to DMX channels starting at _channelStart_.
     * channelStart must be 1 or more. Use write() if you need to access channel 0.
     */
    void writeBytes(uint16_t channelStart, uint8_t values[], uint16_t len);
    /**
     * Read one DMX _channel_ from output buffer
     */
    uint8_t read(uint16_t channel);
    /**
     * Read _len_ Bytes from DMX channels output buffer data starting at _channelStart_.
     * Returns an array[len].
     */
    bool readBytes(uint16_t channelStart, uint8_t values[], uint16_t len);
    /**
     * Send a new DMX frame of _dmxData out.
     */
    bool update();
    /**
     * Write LED data to DMX output buffer and send out.
     */
    bool handleDMXOutput();
    /**
     * Get last time DMX Output was started in ms.
     */
    unsigned long getLastDmxOut();
    /**
     * Whether the DMX is busy sending a frame.
     */
    bool busy();
    /**
     * Change update rate to _updateRate_.
     */
    void setUpdateRate(uint8_t updateRate);
    /**
     * Get current _updateRate_.
     */
    uint8_t getUpdateRate();
    /**
     * Returns time in ms to next update to reach a maximum of _updateRate.
     * Pay attention that when the last update was at let's say 100.9ms, at 122.0ms this reports as if 22ms had passed.
     * To get a definitive max refresh rate, trigger update only on a result of -1.
     * Returns negative numbers if the time has passed already.
     */
    int timeToNextUpdate();

  private:
    HardwareSerial* _dmxSerial;
    uint8_t _outputPin;     // DMX TX pin
    uint8_t _dmxData[DMX_CHANNELS] = {0};
    int8_t _uartNo = -1;
    uint8_t _updateRate;
    unsigned long _lastDmxOutMillis = 0;
};

#endif // DMX_OUTPUT_H
