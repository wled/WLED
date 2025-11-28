# Add a section to the linker script to store our dynamic arrays
# This is implemented as a pio post-script to ensure that our extra linker 
# script fragments are processed last, after the base platform scripts have 
# been loaded and all sections defined.
Import("env")

if env.get("PIOPLATFORM") == "espressif8266":
    # Use a shim on this platform so we can share the same output blocks
    # names as used by later platforms (ESP32)
    env.Append(LINKFLAGS=["-Ttools/esp8266_rodata.ld"])

env.Append(LINKFLAGS=["-Ttools/dynarray.ld"])
