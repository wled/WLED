# JSON IR remote

## Purpose 

The JSON IR remote enables users to customize IR remote behavior without writing custom code and compiling. 
It also allows using any remote compatible with your IR receiver. Using the JSON IR remote, you can
map buttons from any remote to any HTTP request API or JSON API command.  

## Usage

* Upload the IR config file, named _ir.json_ to your board using the [ip address]/edit url. Pick from one of the included files or create your own.
* On the config > LED settings page, set the correct IR pin.
* On the config > Sync Interfaces page, select "JSON Remote" as the Infrared remote.

## Modification

* See if there is a json file with the same number of buttons as your remote. Many remotes will have the same internals and emit the same codes but have different labels.
* In the ir.json file, each key will be the hex encoded IR code.
* The "cmd" property will be the HTTP Request API or JSON API to execute when that button is pressed.
* A limited number of c functions are supported (!incBrightness, !decBrightness, !presetFallback)
* When using !presetFallback, include properties PL (preset to load), FX (effect to fall back to) and FP (palette to fall back to)
* If the command is _repeatable_ and does not contain the "~" character, add a "rpt": true property.
* Other properties are ignored, but having a label property may help when editing.


Sample:
{
  "0xFF629D": {"cmd": "T=2", "rpt": true, "label": "Toggle on/off"},  // HTTP command
  "0xFF9867": {"cmd": "A=~16", "label": "Inc brightness"},            // HTTP command with incrementing          
  "0xFF38C7": {"cmd": {"bri": 10}, "label": "Dim to 10"},             // JSON command 
  "0xFF22DD": {"cmd": "!presetFallback", "PL": 1, "FX": 16, "FP": 6,  
               "label": "Preset 1 or fallback to Saw - Party"},       // c function
}
