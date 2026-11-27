# Tetris AI effect usermod

Adds a self-playing Tetris game as an effect ('Tetris AI') for 2D matrices.

Development of this usermod now happens in a [separate repository](https://github.com/muebau/wled-usermod-tetrisai). Please refer to that repository and the official documentation on [community usermods](https://kno.wled.ge/advanced/community-usermods/) for additional information.

This usermod can be enabled by adding the following `platformio_override.ini` fragment. Additional instructions and options are available in the usermod repository [documentation](https://github.com/muebau/wled-usermod-tetrisai).

```ini
[env:esp32dev_tetrisai]
extends = env:esp32dev
custom_usermods =
  ${env:esp32dev.custom_usermods}
  wled-usermod-tetrisai = https://github.com/muebau/wled-usermod-tetrisai.git
```

The usermod settings are stored under the new name `TetrisAI` (previously `TetrisAI_v2`).
