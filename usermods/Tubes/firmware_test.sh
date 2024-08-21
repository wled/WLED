#!/bin/bash

# Updates new boards (which start as broadcasting on WLED-AP) to custom firmware
# Will update as many boards as are plugged in, one at a time.


# Two ways to use it:
# 1) ./firmware.sh
#    This will update all boards with open APs named WLED-AP
# 2) ./firmware.sh batch
#   This will update all boards with open APs named WLED-AP or WLED-UPDATE
#   This is useful for updating a batch of boards at once
#   To put a board in this mode, send it a V## command with a version higher than the current one
# There's also:
# 3) ./firmware.sh upload
#    This will upload the firmware to the internet server, but not update any boards
#    Internet upload is not working 100% yet, so this is not recommended


WLEDPATH=../../build_output/firmware
ESPPATH=~/.platformio/packages/framework-arduinoespressif32/tools

update_config() {
  # No longer update configs? comment this
  # return;

  echo "Updating configuration via OTA"
#  curl -s http://$1/upload -F "data=@default_config.json;filename=/cfg.json" -H "Connection: close" --no-keepalive
  echo "Configured; wait..."
#  curl -s http://$1/reset -H "Connection: close" >/dev/null
}


update_firmware() {

  echo "Getting info"
  json=$( curl -s http://$1/json/si )

  arch=$(echo "$json" | jq -r '.["info"].arch')
  name=$(echo "$json" | jq -r '.["info"].name')
  echo "$arch $name"

  firmware=

  if [ ! -z "$name" ]; then
    if [ "dig2go" == "$name" ]; then
      firmware="esp32_quinled_dig2go_tubes.bin"
    fi
  fi

  if [ -z "$firmware" ] && [ ! -z "$arch" ]; then
    if [ "ESP32-C3" == "$arch" ]; then
      firmware="esp32-c3-athom_tubes.bin"
    elif [ "esp32" == "$arch" ]; then
      firmware="esp32_quinled_dig2go_tubes.bin"
    fi
  fi

  if [ -z "$firmware" ]; then
    echo "firmware not set - not updating OTA"
    curl -s http://$1/reset -H "Connection: close" >/dev/null
  else
    echo "Updating $firmware firmware via OTA"
    curl -s -F "update=@../../build_output/firmware/$firmware" -H "Connection: close" --no-keepalive $1/update >/dev/null
    curl -s http://$1/reset -H "Connection: close" >/dev/null
  fi
}

update_firmware $1
