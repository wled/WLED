Import("env")
import shutil
from pathlib import Path
from SCons.Script import Exit

# ── setup_matter_component.py ─────────────────────────────────────────────────
# Pre-build script for the esp32s3_matter_wifi environment.
#
# Copies the idf_component.yml manifest from the Matter usermod into wled00/
# so that pioarduino's built-in ComponentManager (and the IDF Component Manager)
# can resolve the espressif/esp_matter dependency.
#
# The destination file is listed in .gitignore and must NOT be committed.
# ─────────────────────────────────────────────────────────────────────────────

project_dir = Path(env["PROJECT_DIR"]).resolve()
src_yml     = project_dir / "usermods" / "matter_over_wifi" / "idf_component.yml"
dst_yml     = project_dir / "wled00"   / "idf_component.yml"

if not src_yml.exists():
    print(
        "\033[0;31;43m"
        f"Matter: idf_component.yml not found at {src_yml} – "
        "cannot resolve the esp_matter component."
        "\033[0m"
    )
    Exit(1)

shutil.copy2(str(src_yml), str(dst_yml))
print(
    "\033[6;33;42m"
    "Matter: copied idf_component.yml → wled00/idf_component.yml"
    "\033[0m"
)
