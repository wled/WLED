Import("env")
import subprocess
import shutil
from pathlib import Path
from SCons.Script import Exit

# ── setup_matter_component.py ─────────────────────────────────────────────────
# Pre-build script for environments that need the esp_matter IDF component.
#
# 1. Copies idf_component.yml from the Matter usermod into wled00/ so the
#    IDF Component Manager knows what to fetch.
# 2. Invokes `idf_component_manager` (compote) to download esp_matter and its
#    transitive deps into managed_components/.
# 3. Adds the downloaded component include paths to CPPPATH so the compiler
#    can find <esp_matter.h> and friends.
# ─────────────────────────────────────────────────────────────────────────────

project_dir = Path(env["PROJECT_DIR"]).resolve()
src_dir     = project_dir / "wled00"
src_yml     = project_dir / "usermods" / "usermod_v2_matter" / "idf_component.yml"
dst_yml     = src_dir / "idf_component.yml"
managed_dir = project_dir / "managed_components"

# ── Step 1: copy the manifest ────────────────────────────────────────────────
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

# ── Step 2: run the IDF Component Manager if components aren't present ───────
esp_matter_dir = managed_dir / "espressif__esp_matter"

if not esp_matter_dir.exists():
    print(
        "\033[6;33;42m"
        "Matter: running IDF Component Manager to fetch esp_matter …"
        "\033[0m"
    )
    try:
        result = subprocess.run(
            [
                "python3", "-m", "idf_component_manager",
                "update-dependencies",
                "--project-dir", str(project_dir),
                "--path", str(src_dir),
            ],
            capture_output=True,
            text=True,
            timeout=300,
        )
        if result.returncode != 0:
            # Some versions use a different sub-command; try the older form
            result = subprocess.run(
                [
                    "python3", "-m", "idf_component_manager",
                    "fetch-dependencies",
                    "--project-dir", str(project_dir),
                    "--path", str(src_dir),
                ],
                capture_output=True,
                text=True,
                timeout=300,
            )
        if result.returncode != 0:
            print("\033[0;31;43m" "Matter: idf_component_manager failed:" "\033[0m")
            print(result.stdout)
            print(result.stderr)
            Exit(1)
        print(
            "\033[6;33;42m"
            "Matter: esp_matter component fetched successfully"
            "\033[0m"
        )
    except FileNotFoundError:
        print(
            "\033[0;31;43m"
            "Matter: idf_component_manager not installed. "
            "Install with: pip install idf-component-manager"
            "\033[0m"
        )
        Exit(1)
    except subprocess.TimeoutExpired:
        print(
            "\033[0;31;43m"
            "Matter: idf_component_manager timed out fetching components"
            "\033[0m"
        )
        Exit(1)

# ── Step 3: inject managed_components include paths into the build ───────────
if managed_dir.exists():
    for component in sorted(managed_dir.iterdir()):
        inc = component / "include"
        if inc.is_dir():
            env.Append(CPPPATH=[str(inc)])
        # Some components put headers at the root
        elif component.is_dir():
            env.Append(CPPPATH=[str(component)])
    print(
        "\033[6;33;42m"
        f"Matter: added managed_components include paths"
        "\033[0m"
    )
else:
    print(
        "\033[0;31;43m"
        "Matter: managed_components/ not found after fetch – build will likely fail"
        "\033[0m"
    )
