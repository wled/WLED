Import('env');

env.Append(CPPDEFINES=[("USERMOD_BLE")
                     , ("CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED ")
                     , ("CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED")
                     , ("CONFIG_BT_ENABLED                      ", "y")
                     , ("CONFIG_BTDM_CTRL_MODE_BLE_ONLY         ", "y")
                     , ("CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY      ", "n")
                     , ("CONFIG_BTDM_CTRL_MODE_BTDM             ", "n")
                     , ("CONFIG_BT_BLUEDROID_ENABLED            ", "n")
                     , ("CONFIG_BT_NIMBLE_ENABLED               ", "y")]);
