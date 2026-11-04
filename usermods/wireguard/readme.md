# WireGuard VPN

This usermod will connect your WLED instance to a remote WireGuard subnet.

Configuration is performed via the Usermod menu. There are no parameters to set in code!

## Installation 

Copy the `platformio_override.ini` file to the root project directory, review the build options, and select the `WLED_ESP32-WireGuard` environment.

## Important

The wireguard usermod is currently not compiling with WLED v17, due to a library incompatibility - see https://github.com/wled/WLED/issues/5937

## Author

Aiden Vigue [vigue.me](https://vigue.me)
[@acvigue](https://github.com/acvigue)
aiden@vigue.me



