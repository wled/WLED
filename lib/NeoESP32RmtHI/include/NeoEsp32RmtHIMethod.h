/*-------------------------------------------------------------------------
NeoPixel driver for ESP32 RMTs using High-priority Interrupt

(NB. This cannot be mixed with the non-HI driver.)

Written by Will M. Miles.

I invest time and resources providing this open source code,
please support me by donating (see https://github.com/Makuna/NeoPixelBus)

-------------------------------------------------------------------------
This file is part of the Makuna/NeoPixelBus library.

NeoPixelBus is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

NeoPixelBus is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with NeoPixel.  If not, see
<http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------*/

#pragma once

#if defined(ARDUINO_ARCH_ESP32)

// Use the NeoEspRmtSpeed types from the driver-based implementation
#include <NeoPixelBus.h>


namespace NeoEsp32RmtHiMethodDriver {
    // Install the driver for a specific channel, specifying timing properties
    esp_err_t Install(rmt_channel_t channel, uint32_t rmtBit0, uint32_t rmtBit1, uint32_t resetDuration);

    // Remove the driver on a specific channel
    esp_err_t Uninstall(rmt_channel_t channel);

    // Write a buffer of data to a specific channel.
    // Buffer reference is held until write completes.
    esp_err_t Write(rmt_channel_t channel, const uint8_t *src, size_t src_size);

    // Wait until transaction is complete.
    esp_err_t WaitForTxDone(rmt_channel_t channel, TickType_t wait_time);
};

template<typename T_SPEED, typename T_CHANNEL> class NeoEsp32RmtHIMethodBase
{
public:
    typedef NeoNoSettings SettingsObject;

    NeoEsp32RmtHIMethodBase(uint8_t pin, uint16_t pixelCount, size_t elementSize, size_t settingsSize)  :
        _sizeData(pixelCount * elementSize + settingsSize),
        _pin(pin)
    {
        construct();
    }

    NeoEsp32RmtHIMethodBase(uint8_t pin, uint16_t pixelCount, size_t elementSize, size_t settingsSize, NeoBusChannel channel) :
        _sizeData(pixelCount* elementSize + settingsSize),
        _pin(pin),
        _channel(channel)
    {
        construct();
    }

    ~NeoEsp32RmtHIMethodBase()
    {
        // wait until the last send finishes before destructing everything
        // arbitrary time out of 10 seconds
        ESP_ERROR_CHECK_WITHOUT_ABORT(NeoEsp32RmtHiMethodDriver::WaitForTxDone(_channel.RmtChannelNumber, 10000 / portTICK_PERIOD_MS));

        ESP_ERROR_CHECK(NeoEsp32RmtHiMethodDriver::Uninstall(_channel.RmtChannelNumber));

        gpio_matrix_out(_pin, SIG_GPIO_OUT_IDX, false, false);
        pinMode(_pin, INPUT);

        free(_dataEditing);
        free(_dataSending);
    }

    bool IsReadyToUpdate() const
    {
        return (ESP_OK == ESP_ERROR_CHECK_WITHOUT_ABORT_SILENT_TIMEOUT(NeoEsp32RmtHiMethodDriver::WaitForTxDone(_channel.RmtChannelNumber, 0)));
    }

    void Initialize()
    {
        rmt_config_t config = {};

        config.rmt_mode = RMT_MODE_TX;
        config.channel = _channel.RmtChannelNumber;
        config.gpio_num = static_cast<gpio_num_t>(_pin);
        config.mem_block_num = 1;
        config.tx_config.loop_en = false;

        config.tx_config.idle_output_en = true;
        config.tx_config.idle_level = T_SPEED::IdleLevel;

        config.tx_config.carrier_en = false;
        config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;

        config.clk_div = T_SPEED::RmtClockDivider;

        ESP_ERROR_CHECK(rmt_config(&config));   // Uses ESP library
        ESP_ERROR_CHECK(NeoEsp32RmtHiMethodDriver::Install(_channel.RmtChannelNumber, T_SPEED::RmtBit0, T_SPEED::RmtBit1, T_SPEED::RmtDurationReset));
    }

    void Update(bool maintainBufferConsistency)
    {
        // wait for not actively sending data
        // this will time out at 10 seconds, an arbitrarily long period of time
        // and do nothing if this happens
        if (ESP_OK == ESP_ERROR_CHECK_WITHOUT_ABORT(NeoEsp32RmtHiMethodDriver::WaitForTxDone(_channel.RmtChannelNumber, 10000 / portTICK_PERIOD_MS)))
        {
            // now start the RMT transmit with the editing buffer before we swap
            ESP_ERROR_CHECK_WITHOUT_ABORT(NeoEsp32RmtHiMethodDriver::Write(_channel.RmtChannelNumber, _dataEditing, _sizeData));

            if (maintainBufferConsistency)
            {
                // copy editing to sending,
                // this maintains the contract that "colors present before will
                // be the same after", otherwise GetPixelColor will be inconsistent
                memcpy(_dataSending, _dataEditing, _sizeData);
            }

            // swap so the user can modify without affecting the async operation
            std::swap(_dataSending, _dataEditing);
        }
    }

    bool AlwaysUpdate()
    {
        // this method requires update to be called only if changes to buffer
        return false;
    }

    bool SwapBuffers()
    {
        std::swap(_dataSending, _dataEditing);
        return true;
    }

    uint8_t* getData() const
    {
        return _dataEditing;
    };

    size_t getDataSize() const
    {
        return _sizeData;
    }

    void applySettings([[maybe_unused]] const SettingsObject& settings)
    {
    }

private:
    const size_t  _sizeData;      // Size of '_data*' buffers
    const uint8_t _pin;            // output pin number
    const T_CHANNEL _channel; // holds instance for multi channel support

    // Holds data stream which include LED color values and other settings as needed
    uint8_t*  _dataEditing;   // exposed for get and set
    uint8_t*  _dataSending;   // used for async send using RMT


    void construct()
    {
        _dataEditing = static_cast<uint8_t*>(malloc(_sizeData));
        // data cleared later in Begin()

        _dataSending = static_cast<uint8_t*>(malloc(_sizeData));
        // no need to initialize it, it gets overwritten on every send
    }
};

// normal
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannelN> NeoEsp32RmtHINSk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannelN> NeoEsp32RmtHINTm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannelN> NeoEsp32RmtHINTm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannelN> NeoEsp32RmtHINTm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannelN> NeoEsp32RmtHINApa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannelN> NeoEsp32RmtHINTx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannelN> NeoEsp32RmtHINGs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannelN> NeoEsp32RmtHIN800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannelN> NeoEsp32RmtHIN400KbpsMethod;
typedef NeoEsp32RmtHINWs2805Method NeoEsp32RmtHINWs2814Method;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel0> NeoEsp32RmtHI0800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel0> NeoEsp32RmtHI0400KbpsMethod;
typedef NeoEsp32RmtHI0Ws2805Method NeoEsp32RmtHI0Ws2814Method;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel1> NeoEsp32RmtHI1800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel1> NeoEsp32RmtHI1400KbpsMethod;
typedef NeoEsp32RmtHI1Ws2805Method NeoEsp32RmtHI1Ws2814Method;

#if !defined(CONFIG_IDF_TARGET_ESP32C3)

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel2>  NeoEsp32RmtHI2Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel2> NeoEsp32RmtHI2800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel2> NeoEsp32RmtHI2400KbpsMethod;
typedef NeoEsp32RmtHI2Ws2805Method NeoEsp32RmtHI2Ws2814Method;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel3>  NeoEsp32RmtHI3Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel3> NeoEsp32RmtHI3800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel3> NeoEsp32RmtHI3400KbpsMethod;
typedef NeoEsp32RmtHI3Ws2805Method NeoEsp32RmtHI3Ws2814Method;

#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel4>  NeoEsp32RmtHI4Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel4> NeoEsp32RmtHI4800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel4> NeoEsp32RmtHI4400KbpsMethod;
typedef NeoEsp32RmtHI4Ws2805Method NeoEsp32RmtHI4Ws2814Method;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel5>  NeoEsp32RmtHI5Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel5> NeoEsp32RmtHI5800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel5> NeoEsp32RmtHI5400KbpsMethod;
typedef NeoEsp32RmtHI5Ws2805Method NeoEsp32RmtHI5Ws2814Method;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel6>  NeoEsp32RmtHI6Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel6> NeoEsp32RmtHI6800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel6> NeoEsp32RmtHI6400KbpsMethod;
typedef NeoEsp32RmtHI6Ws2805Method NeoEsp32RmtHI6Ws2814Method;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2811Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2812xMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2816Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedWs2805, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2805Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel7>  NeoEsp32RmtHI7Sk6812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1814, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tm1814Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1829, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tm1829Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1914, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tm1914Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Apa106Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTx1812, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tx1812Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedGs1903, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Gs1903Method;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel7> NeoEsp32RmtHI7800KbpsMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel7> NeoEsp32RmtHI7400KbpsMethod;
typedef NeoEsp32RmtHI7Ws2805Method NeoEsp32RmtHI7Ws2814Method;

#endif // !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#endif // !defined(CONFIG_IDF_TARGET_ESP32C3)

// inverted
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannelN> NeoEsp32RmtHINWs2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannelN> NeoEsp32RmtHINSk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannelN> NeoEsp32RmtHINTm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannelN> NeoEsp32RmtHINTm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannelN> NeoEsp32RmtHINTm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannelN> NeoEsp32RmtHINApa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannelN> NeoEsp32RmtHINTx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannelN> NeoEsp32RmtHINGs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannelN> NeoEsp32RmtHIN800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannelN> NeoEsp32RmtHIN400KbpsInvertedMethod;
typedef NeoEsp32RmtHINWs2805InvertedMethod NeoEsp32RmtHINWs2814InvertedMethod;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel0> NeoEsp32RmtHI0Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel0> NeoEsp32RmtHI0800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel0> NeoEsp32RmtHI0400KbpsInvertedMethod;
typedef NeoEsp32RmtHI0Ws2805InvertedMethod NeoEsp32RmtHI0Ws2814InvertedMethod;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel1> NeoEsp32RmtHI1Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel1> NeoEsp32RmtHI1800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel1> NeoEsp32RmtHI1400KbpsInvertedMethod;
typedef NeoEsp32RmtHI1Ws2805InvertedMethod NeoEsp32RmtHI1Ws2814InvertedMethod;

#if !defined(CONFIG_IDF_TARGET_ESP32C3)

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel2>  NeoEsp32RmtHI2Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel2> NeoEsp32RmtHI2Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel2> NeoEsp32RmtHI2800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel2> NeoEsp32RmtHI2400KbpsInvertedMethod;
typedef NeoEsp32RmtHI2Ws2805InvertedMethod NeoEsp32RmtHI2Ws2814InvertedMethod;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel3>  NeoEsp32RmtHI3Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel3> NeoEsp32RmtHI3Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel3> NeoEsp32RmtHI3800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel3> NeoEsp32RmtHI3400KbpsInvertedMethod;
typedef NeoEsp32RmtHI3Ws2805InvertedMethod NeoEsp32RmtHI3Ws2814InvertedMethod;

#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel4>  NeoEsp32RmtHI4Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel4> NeoEsp32RmtHI4Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel4> NeoEsp32RmtHI4800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel4> NeoEsp32RmtHI4400KbpsInvertedMethod;
typedef NeoEsp32RmtHI4Ws2805InvertedMethod NeoEsp32RmtHI4Ws2814InvertedMethod;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel5>  NeoEsp32RmtHI5Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel5> NeoEsp32RmtHI5Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel5> NeoEsp32RmtHI5800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel5> NeoEsp32RmtHI5400KbpsInvertedMethod;
typedef NeoEsp32RmtHI5Ws2805InvertedMethod NeoEsp32RmtHI5Ws2814InvertedMethod;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel6>  NeoEsp32RmtHI6Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel6> NeoEsp32RmtHI6Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel6> NeoEsp32RmtHI6800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel6> NeoEsp32RmtHI6400KbpsInvertedMethod;
typedef NeoEsp32RmtHI6Ws2805InvertedMethod NeoEsp32RmtHI6Ws2814InvertedMethod;

typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2811InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2812xInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2816InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedWs2805, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Ws2805InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel7>  NeoEsp32RmtHI7Sk6812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1814, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tm1814InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1829, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tm1829InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTm1914, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tm1914InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Apa106InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedTx1812, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Tx1812InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeedGs1903, NeoEsp32RmtChannel7> NeoEsp32RmtHI7Gs1903InvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel7> NeoEsp32RmtHI7800KbpsInvertedMethod;
typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel7> NeoEsp32RmtHI7400KbpsInvertedMethod;
typedef NeoEsp32RmtHI7Ws2805InvertedMethod NeoEsp32RmtHI7Ws2814InvertedMethod;

#endif // !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#endif // !defined(CONFIG_IDF_TARGET_ESP32C3)

#endif
