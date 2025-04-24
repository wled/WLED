
#define CONFIG_BT_ENABLED                   y
#define CONFIG_BTDM_CTRL_MODE_BLE_ONLY      y
#define CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY   n
#define CONFIG_BTDM_CTRL_MODE_BTDM          n
#define CONFIG_BT_BLUEDROID_ENABLED         n
#define CONFIG_BT_NIMBLE_ENABLED            y
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED 
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED

#include "wled.h"
#include <NimBLEDevice.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "9b01c315-9649-4360-9b3c-2599f6f28d65"
#define CHARACTERISTIC_UUID "220f62a4-b861-40d4-bffa-91e911ace3a4"


