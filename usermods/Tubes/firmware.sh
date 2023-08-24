#!/bin/bash

# Updates new boards (which start as broadcasting on WLED-AP) to custom firmware
# Will update as many boards as are plugged in, one at a time.

WLEDPATH=../../build_output/firmware
ESPPATH=~/.platformio/packages/framework-arduinoespressif32/tools

upload_firmware() {
  echo "Uploading firmware"
  sftp control@brcac.com <<EOF
cd brcac.com
put ../../build_output/firmware/tubes.bin firmware.bin
quit
EOF
}

update_config() {
  # No longer update configs? comment this
  # return;

  echo "Updating configuration via OTA"
  curl -s http://$1/upload -F "data=@default_config.json;filename=/cfg.json" -H "Connection: close" --no-keepalive
  echo "Configured; wait..."
  curl -s http://$1/reset -H "Connection: close" >/dev/null
}

update_firmware() {
  echo "Updating firmware via OTA"
  curl -s -F "update=@../../build_output/firmware/tubes.bin" -H "Connection: close" --no-keepalive $1/update >/dev/null
  echo "Updated; wait..."
  sleep 5
  update_config $1
}

connect() {
  if ! networksetup -getairportnetwork en0 | grep "$1"
  then
    echo "Connecting to $1"
    networksetup -setairportnetwork en0 "$1" "$2"
    echo "Connected; wait..."
    sleep 5
  fi
}

update_one() {
  connect "$2" "$3"

  ping -c 1 -t 2 $1 >/dev/null
  PINGRESULT=$?
  if [ $PINGRESULT -eq 0 ]; then
    update_firmware $1
  else
    echo Missing $1
  fi
  return 1
}

update_batch() {
  airport -s | grep WLED | cut -c10-32 | while read line
  do
    if [ "$line" == "WLED-AP" ]; then
      update_one 4.3.2.1 "$line" "wled1234"    
    elif [ "$line" == "WLED-UPDATE" ]; then
      update_one 4.3.2.1 "$line" "update1234"    
    else
      update_one 4.3.2.1 "$line" "WledWled"
    fi
  done
}

process() {
  if [ "$1" == "upload" ]; then
    upload_firmware
  elif [ "$1" == "batch" ]; then
    update_batch
  else
    while :
    do 
      update_one 4.3.2.1 "WLED-AP" "wled1234"
    done
  fi
}

# WiFi is spotty. Let's do it a couple times :)
process "$@"
process "$@"
process "$@"
process "$@"
process "$@"
process "$@"
process "$@"