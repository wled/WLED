# Controlling Wiz lights

This usermod allows the control of [WiZ](https://www.wizconnected.com/en/consumer/) lights that are in the same network as the WLED controller.

The mod takes the colors from the first few pixels and sends them to the lights.

## Configuration

- Interval (ms)
    - How frequently to update the WiZ lights, in milliseconds.
    - Setting too low may causse ESP to become unresponsive.
- Send Delay (ms)
    - An optional millisecond delay after updating each WiZ light. 
    - Can help smooth out effects when using a larger number of WiZ lights
- Use Enhanced White
    - Enables using the WiZ lights onboard white LEDs instead of sending maximum RGB values.
    - Tunable with warm and cool LEDs as supported by WiZ bulbs
    - Note: Only sent when max RGB value is set, need to have automatic brightness limiter disabled
    - ToDo: Have better logic for white value mixing to better take advantage of the lights capabilities
- Always Force Update
    - Can be enabled to always send update message to light, even when color matches what was previously sent.
- Force update every x minutes
    - Configuration option to allow adjusting the default force update timeout of 5 minutes.
    - Setting to 0 has the same impact as enabling Always Force Update
    - 
Then enter the IPs for the lights to be controlled, in order. There is currently a limit of 15 devices that can be controled, but that number
can be easily changed by updating _MAX_WIZ_LIGHTS_.




## Related project

If you use these lights and python, make sure to check out the [pywizlight](https://github.com/sbidy/pywizlight) project. I learned how to
format the messages to control the lights from that project.
