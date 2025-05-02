#ifndef USERMOD_BLE
    #define USERMOD_BLE
#endif
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

/** Time in milliseconds to advertise */
static uint32_t advTime = 5000;

/** Time to sleep between advertisements */
static uint32_t sleepSeconds = 20;

/** Primary PHY used for advertising, can be one of BLE_HCI_LE_PHY_1M or BLE_HCI_LE_PHY_CODED */
static uint8_t primaryPhy = BLE_HCI_LE_PHY_1M;

/**
 *  Secondary PHY used for advertising and connecting,
 *  can be one of BLE_HCI_LE_PHY_1M, BLE_HCI_LE_PHY_2M or BLE_HCI_LE_PHY_CODED
 */
static uint8_t secondaryPhy = BLE_HCI_LE_PHY_2M;


