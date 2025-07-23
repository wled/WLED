# Select target board
# Ref: https://github.com/Bodmer/TFT_eSPI/wiki/Installing-on-PlatformIO#4-configure-library
# These flags are passed to this library and the TFT_eSPI library only

Import("env")

def add_display_setup(tgt_env):
    tgt_env.Append(CPPDEFINES=[("USER_SETUP_LOADED", "1")])
    tgt_env.Append(CCFLAGS="-include $PROJECT_LIBDEPS_DIR/$PIOENV/TFT_eSPI/User_Setups/Setup25_TTGO_T_Display.h")

add_display_setup(env)
for lb in env.GetLibBuilders():
    if lb.name == "TFT_eSPI":
        add_display_setup(lb.env)
        break

# Add pin definitions to whole project environment if they haven't otherwise been specified
global_env = DefaultEnvironment()
global_defines = global_env.get("CPPDEFINES", [])
if "BTNPIN" not in [v[0] for v in global_defines if isinstance(v, tuple)]:
    global_env.Append(CPPDEFINES=[("BTNPIN", "35")])
