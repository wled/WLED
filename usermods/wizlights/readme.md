# Controlling Wiz lights

Enables controlling [WiZ](https://www.wizconnected.com/en/consumer/) lights that are part of the same network as the WLED controller.

Development of this usermod now happens in a [separate repository](https://github.com/eibanez/wled-usermod-wizlights). Please refer to that repository and the official documentation on [community usermods](https://kno.wled.ge/advanced/community-usermods/) for additional information.

This usermod can be enabled by adding the following ```platformio_override.ini``` fragment. Additional instructions and options are available in the usermod repository [documentation](https://github.com/eibanez/wled-usermod-wizlights).

```ini
[env:esp32dev_wizlights]
extends = env:esp32dev
custom_usermods =
  ${env:esp32dev.custom_usermods}
  wled-usermod-wizlights = https://github.com/eibanez/wled-usermod-wizlights.git
```
