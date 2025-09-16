Import("env")
import os
from os.path import join

print(">>> Running force_littlefs.py")

# Get base packages dir from PlatformIO config
platformio_pkg_dir = env.get("PROJECT_PACKAGES_DIR") or env["PROJECT_DIR"].joinpath(".pio", "packages")

mklittlefs_path = join(platformio_pkg_dir, "tool-mklittlefs", "mklittlefs.exe" if os.name == "nt" else "mklittlefs")

print(">>> Forcing mklittlefs tool:", mklittlefs_path)

env.Replace(
    FS_IMAGE_TOOL=mklittlefs_path,
    FS_IMAGE_NAME="littlefs"
)
