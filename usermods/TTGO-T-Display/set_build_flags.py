Import("env")

global_env = DefaultEnvironment()

# Provide TTGO's preferred display setup as the default for TFT_eSPI_compat.
# Users can override this by setting custom_display_setup in platformio_override.ini.
global_env.SetDefault(TFT_DISPLAY_SETUP_DEFAULT="User_Setups/Setup25_TTGO_T_Display.h")
