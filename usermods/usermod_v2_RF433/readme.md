# RF433 remote usermod

Usermod for controlling WLED using a generic 433 / 315MHz remote and simple 3-pin receiver
See <https://github.com/sui77/rc-switch/> for compatibility details

## Build

- Create a `platformio_override.ini` file at the root of the wled source directory if not already present
- Copy the `433MHz RF remote example for esp32dev` section from `platformio_override.sample.ini` into it
- Duplicate/adjust for other boards

## Usage

- Connect receiver to a free pin
- Set pin in Config->Usermods
- Info pane will show the last received button code
- Upload the remote433.json sample file in this folder to the ESP with the file editor at [http://\[wled-ip\]/edit](http://ip/edit)
- Edit as necessary, the key is the button number retrieved from the info pane, and the "cmd" can be either an [HTTP API](https://kno.wled.ge/interfaces/http-api/) or a [JSON API](https://kno.wled.ge/interfaces/json-api/) command. 