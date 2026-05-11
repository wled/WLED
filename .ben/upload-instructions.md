# Get into the project folder first:

cd "WLED-Main-2026-04-29"



# Only when you edit files inside wled00/data/ (the web UI source)
npm run build

# Whenever any C++, .h, or .ini file changes
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target upload

# Fresh device only — it wipes all saved presets, config, and settings on the device
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target uploadfs



### Flashing new device ###

& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target upload