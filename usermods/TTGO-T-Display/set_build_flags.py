# Select target board
# Ref: https://github.com/Bodmer/TFT_eSPI/wiki/Installing-on-PlatformIO#4-configure-library
# These flags are passed to this library and the TFT_eSPI library only

Import("env")
import os


# Add pin definitions to whole project environment if they haven't otherwise been specified
# WM: This is a feature of the original usermod; I'm not sure if it's globally applicable..
global_env = DefaultEnvironment()
global_defines = global_env.get("CPPDEFINES", [])
if "BTNPIN" not in [v[0] for v in global_defines if isinstance(v, tuple)]:
    global_env.Append(CPPDEFINES=[("BTNPIN", "35")])

# Select display setup
display_setup = global_env.GetProjectOption("custom_display_setup","User_Setups/Setup25_TTGO_T_Display.h")

# Resolve this file's path
def find_in_paths(filename, paths):
    for path in paths:
        fullpath = os.path.join(str(path), filename)
        if os.path.isfile(fullpath):
            return fullpath
    return None

lib_builders = env.GetLibBuilders()
if os.path.isfile(display_setup):
    display_setup_path = display_setup
else:
    search_paths = [global_env["PROJECT_SRC_DIR"],
                    global_env["PROJECT_INCLUDE_DIR"],
                    *[incdir for lb in lib_builders for incdir in lb.get_include_dirs()]]

    display_setup_path = find_in_paths(display_setup, search_paths)
    
if display_setup_path is None:
    print(f"Unable to find {display_setup} in any include path - {[str(path) for path in search_paths]}!")
    raise RuntimeError("Missing display setup; use 'User_Setups/something.h' for setups out of TFT_eSPI, or put your setup in wled00")

def add_display_setup(tgt_env):
    tgt_env.Append(CPPDEFINES=[("USER_SETUP_LOADED", "1")])
    tgt_env.Append(CCFLAGS=f"-include {display_setup_path}")

# Add it for this library and the TFT_eSPI dependency
add_display_setup(env)
for lb in lib_builders:
    if lb.name == "TFT_eSPI":
        add_display_setup(lb.env)
        break
