# Controlling Wiz lights

Enabless controlling [WiZ](https://www.wizconnected.com/en/consumer/) lights that are part of the same network as the WLED controller.

The mod takes the colors from the first few pixels and sends them to the lights.

## Configuration

- Interval (ms)
    - How frequently to update the WiZ lights, in milliseconds.
    - Setting it too low may causse the ESP to become unresponsive.
- Send Delay (ms)
    - An optional millisecond delay after updating each WiZ light. 
    - Can help smooth out effects when using a large number of WiZ lights
- Use Enhanced White
    - Uses the WiZ lights onboard white LEDs instead of sending maximum RGB values.
    - Tunable with warm and cool LEDs as supported by WiZ bulbs
    - Note: Only sent when max RGB value is set, the automatic brightness limiter must be disabled
    - ToDo: Have better logic for white value mixing to take advantage of the light's capabilities
- Always Force Update
    - Can be enabled to always send update message to light even if the new value matches the old value.
- Force update every x minutes
    - adjusts the default force update timeout of 5 minutes.
    - Setting to 0 is the same as enabling Always Force Update
    - 
Next, enter the IP addresses for the lights to be controlled, in order. The limit is 15 devices, but that number
can be easily changed by updating _MAX_WIZ_LIGHTS_.




## Related project

If you use these lights and python, make sure to check out the [pywizlight](https://github.com/sbidy/pywizlight) project. You can learn how to
format the messages to control the lights from that project.
