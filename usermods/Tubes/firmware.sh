#!/bin/bash

# Updates new boards (which start as broadcasting on WLED-AP) to custom firmware
# Will update as many boards as are plugged in, one at a time.

WLEDPATH=../../build_output/firmware
ESPPATH=~/.platformio/packages/framework-arduinoespressif32/tools

update_firmware() {
  echo "Updating firmware via OTA"
  curl -s -F "update=@../../build_output/firmware/esp32_quinled_uno.bin" $1/update >/dev/null
  echo "Updated; wait..."
  sleep 5
}

update_config() {
  curl -s http://$1/upload -F "data=@cfg-tmp.json;filename=/cfg.json" >/dev/null
  curl -s http://$1/reset >/dev/null
}

connect() {
  if ! networksetup -getairportnetwork en0 | grep $1
  then
    echo "Connecting to $1"
    networksetup -setairportnetwork en0 $1 "wled1234"
    echo "Connected; wait..."
    sleep 5
  fi
}

start() {
  connect "WLED-AP"

  ping -c 1 -t 2 $1 >/dev/null
  PINGRESULT=$?
  if [ $PINGRESULT -eq 0 ]; then
    update_firmware $1
  else
    echo Missing $1
  fi
  return 1
}

# TODO: extract the config-creation from here
update_one() {
  ping -c 1 -t 2 $1 >/dev/null
  PINGRESULT=$?
  if [ $PINGRESULT -eq 0 ]; then
    echo Updating $1
    curl -s http://$1/presets.json -o presets-tmp.json >/dev/null
    curl -s http://$1/cfg.json -o cfg-tmp.json >/dev/null

    update_firmware $1
    curl -s -F "data=@cfg-tmp.json;filename=/cfg.json" http://$1/upload >/dev/null
    curl -s -F "data=@presets-tmp.json;filename=/presets.json" http://$1/upload >/dev/null
    curl -s http://$1/reset >/dev/null
    rm presets-tmp.json
    rm cfg-tmp.json
    return 0
  else
    echo Missing $1
  fi
  return 1
}

while :
do 
  start 4.3.2.1
done  
