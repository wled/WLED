# Usermods API v2 example usermod

In this usermod file you can find the documentation on how to take advantage of the new version 2 usermods!

## Installation

Add the build flag `-D usermod_v2_social_counter` to your platformio environment.

```ini
[platformio]
default_envs = esp32dev_social_counter

[env:esp32dev_social_counter]
extends = env:esp32dev
custom_usermods = usermod_v2_social_counter
```
