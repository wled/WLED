# RelayBlinds usermod

This simple usermod toggles two relay pins momentarily (default for 500ms) when `userVar0` is set.  
This can be used to e.g. "push" the buttons of a window blinds motor controller.

v1 usermod. Please replace usermod.cpp in the `wled00` directory with the one in this file.
You may upload `index.htm` to `[WLED-IP]/edit` to replace the default lighting UI with a simple Up/Down button one.  
Also, a simple `presets.json` file is available, this makes the relay actions controllable via two presets to facilitate control e.g. via the default UI or Alexa.