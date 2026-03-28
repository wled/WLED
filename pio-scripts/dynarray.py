# Add a section to the linker script to store our dynamic arrays
# This is implemented as a pio post-script to ensure that we can
# place our linker script at the correct point in the command arguments.
Import("env")
from pathlib import Path

platform = env.get("PIOPLATFORM")
script_file = Path(f"tools/dynarray_{platform}.ld")
if script_file.is_file():
    linker_script = f"-T{script_file}"
    if platform == "espressif32":
        # For ESP32, the script must be added at the right point in the list
        linkflags = env.get("LINKFLAGS", [])
        idx = linkflags.index("memory.ld")    
        linkflags.insert(idx+1, linker_script)    
        env.Replace(LINKFLAGS=linkflags)
    else:
        # For other platforms, put it in last
        env.Append(LINKFLAGS=[linker_script])
