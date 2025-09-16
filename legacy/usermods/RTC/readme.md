# DS1307/DS3231 Real time clock

Gets the time from I2C RTC module on boot. This allows clock operation if WiFi is not available.
The stored time is updated each time NTP is synced. 

## Installation 

Add the build flag `-D USERMOD_RTC` to your platformio environment.
