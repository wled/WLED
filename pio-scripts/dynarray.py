# Add a section to the linker script to store our dynamic arrays
# This is implemented as a pio post-script to ensure that we can
# place our linker script at the correct point in the command arguments.
Import("env")
import shutil
from pathlib import Path

# Linker script fragment injected into the rodata output section of whichever
# platform we're building for.  Placed just before the end-of-rodata marker so
# that the dynarray entries land in flash rodata and are correctly sorted.
DYNARRAY_INJECTION = (
    "\n    /* dynarray: WLED dynamic module arrays */\n"
    "    . = ALIGN(0x10);\n"
    "    KEEP(*(SORT_BY_INIT_PRIORITY(.dynarray.*)))\n"
    "    "
)


def inject_before_marker(path, marker):
    """Patch a linker script file in-place, inserting DYNARRAY_INJECTION before marker."""
    original = path.read_text()
    marker_pos = original.find(marker)
    if marker_pos < 0:
        raise RuntimeError(
            f"DYNARRAY injection marker not found in linker script: path={path}, marker={marker!r}"
        )
    patched = original[:marker_pos] + DYNARRAY_INJECTION + original[marker_pos:]
    path.write_text(patched)


if env.get("PIOPLATFORM") == "espressif32":
    # Find sections.ld on the linker search path (LIBPATH), or fall back to
    # BUILD_DIR (the dual-framework espidf+arduino build generates sections.ld
    # directly in BUILD_DIR and passes it as a bare filename in LINKFLAGS).
    sections_ld_path = None
    for ld_dir in env.get("LIBPATH", []):
        candidate = Path(str(ld_dir)) / "sections.ld"
        if candidate.exists():
            sections_ld_path = candidate
            break
    if sections_ld_path is None:
        build_dir = Path(env.subst("$BUILD_DIR"))
        candidate = build_dir / "sections.ld"
        if candidate.exists():
            sections_ld_path = candidate

    if sections_ld_path is not None:
        # Inject inside the existing .flash.rodata output section, just before
        # _rodata_end.  IDF v5 enforces zero gaps between adjacent output
        # sections via ASSERT statements, so INSERT AFTER .flash.rodata would
        # fail.  Injecting inside the section creates no new output section and
        # leaves the ASSERTs satisfied.
        build_dir = Path(env.subst("$BUILD_DIR"))
        patched_path = build_dir / "dynarray_sections.ld"
        shutil.copy(sections_ld_path, patched_path)
        inject_before_marker(patched_path, "_rodata_end = ABSOLUTE(.);")

        # Replace "sections.ld" in LINKFLAGS with an absolute path to our
        # patched copy.  The flag may appear as a bare token, combined as
        # "-Tsections.ld", or split across two tokens ("-T", "sections.ld").
        patched_str = str(patched_path)
        new_flags = []
        skip_next = False
        for flag in env.get("LINKFLAGS", []):
            if skip_next:
                new_flags.append(patched_str if flag == "sections.ld" else flag)
                skip_next = False
            elif flag == "-T":
                new_flags.append(flag)
                skip_next = True
            else:
                new_flags.append(flag.replace("sections.ld", patched_str))
        env.Replace(LINKFLAGS=new_flags)
    else:
        # Assume sections.ld will be built (ESP-IDF format); add a post-action to patch it
        # TODO: consider using ESP-IDF linker fragment (https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/linker-script-generation.html)
        # For now, patch after building
        sections_ld = Path(env.subst("$BUILD_DIR")) / "sections.ld"
        def patch_sections_ld(target, source, env):
            inject_before_marker(sections_ld, "_rodata_end = ABSOLUTE(.);")
        env.AddPostAction(str(sections_ld), patch_sections_ld)
    
elif env.get("PIOPLATFORM") == "espressif8266":
    # The ESP8266 framework preprocesses eagle.app.v6.common.ld.h into
    # local.eagle.app.v6.common.ld in $BUILD_DIR/ld/ at build time.  Register
    # a post-action on that generated file so the injection happens after
    # C-preprocessing but before linking.
    build_ld = Path(env.subst("$BUILD_DIR")) / "ld" / "local.eagle.app.v6.common.ld"

    def patch_esp8266_ld(target, source, env):
        inject_before_marker(build_ld, "_irom0_text_end = ABSOLUTE(.);")

    env.AddPostAction(str(build_ld), patch_esp8266_ld)
