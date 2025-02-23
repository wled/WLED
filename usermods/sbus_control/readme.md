This user mod can be used to control WLED via the sbus. 
Sbus is a protocol that RC receivers use to transmit control signals to other components. 
By connecting the ESP32 to the receiver, WLED can be used as LEDController in RC plains, cars or ships. 

You can change the brightness and between 9 presets. Setting up the Leds needs to be done via APP or WebUi.

To build this usermod the following lines needs to be added to your target in the platformio.ini file:

build_flags = DUSERMOD_SBUS_CONTROL
lib_deps = https://github.com/bolderflight/sbus.git

Hardware setup:

Connect ground of receiver and ESP32. 
Connect receiver sbus_out pin with ESP32 pin configured for sbus_in in config.