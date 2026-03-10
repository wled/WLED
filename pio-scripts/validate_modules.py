import os
import re
import subprocess
from pathlib import Path   # For OS-agnostic path manipulation
from click import secho
from SCons.Script import Action, Exit
Import("env")


def read_lines(p: Path):
    """ Read in the contents of a file for analysis """
    with p.open("r", encoding="utf-8", errors="ignore") as f:
        return f.readlines()


def _get_nm_path(env) -> str:
    """ Derive the nm tool path from the build environment """
    if "NM" in env:
        return env.subst("$NM")
    # Derive from the C compiler: xtensa-esp32-elf-gcc → xtensa-esp32-elf-nm
    cc = env.subst("$CC")
    nm = re.sub(r'(gcc|g\+\+)$', 'nm', os.path.basename(cc))
    return os.path.join(os.path.dirname(cc), nm)


def check_elf_modules(elf_path: Path, env, module_lib_builders) -> set[str]:
    """ Check which modules have at least one defined symbol placed in the ELF.

        The map file is not a reliable source for this: with LTO, original object
        file paths are replaced by temporary ltrans.o partitions in all output
        sections, making per-module attribution impossible from the map alone.
        Instead we invoke nm --defined-only -l on the ELF, which uses DWARF debug
        info to attribute each placed symbol to its original source file.

        Requires usermod libraries to be compiled with -g so that DWARF sections
        are present in the ELF.  load_usermods.py injects -g for all WLED modules
        via dep.env.AppendUnique(CCFLAGS=["-g"]).

        Returns the set of build_dir basenames for confirmed modules.
    """
    nm_path = _get_nm_path(env)
    try:
        result = subprocess.run(
            [nm_path, "--defined-only", "-l", str(elf_path)],
            capture_output=True, text=True, errors="ignore", timeout=120,
        )
        nm_output = result.stdout
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError) as e:
        secho(f"WARNING: nm failed ({e}); skipping per-module validation", fg="yellow", err=True)
        return {Path(b.build_dir).name for b in module_lib_builders}  # conservative pass

    # Build a filtered set of lines that have a nonzero address.
    # nm --defined-only still includes debugging symbols (type 'N') such as the
    # per-CU markers GCC emits in .debug_info (e.g. "usermod_example_cpp_6734d48d").
    # These live at address 0x00000000 in their debug section — not in any load
    # segment — so filtering them out leaves only genuinely placed symbols.
    placed_lines = [
        line for line in nm_output.splitlines()
        if (parts := line.split(None, 1)) and parts[0].lstrip('0')
    ]
    placed_output = "\n".join(placed_lines)

    found = set()
    for builder in module_lib_builders:
        # builder.src_dir is the library source directory (used by is_wled_module() too)
        src_dir = str(builder.src_dir).rstrip("/\\")
        # Guard against prefix collisions (e.g. /path/to/mod vs /path/to/mod-extra)
        # by requiring a path separator immediately after the directory name.
        src_dir_pattern = re.escape(src_dir).replace(r'\\', r'[/\\]')
        if re.search(src_dir_pattern + r'[/\\]', placed_output):
            found.add(Path(builder.build_dir).name)
    return found


DYNARRAY_SECTION = ".dtors" if env.get("PIOPLATFORM") == "espressif8266" else ".dynarray"
USERMODS_SECTION = f"{DYNARRAY_SECTION}.usermods.1"

def count_usermod_objects(map_file: list[str]) -> int:
    """ Returns the number of usermod objects in the usermod list """
    # Count the number of entries in the usermods table section
    return len([x for x in map_file if USERMODS_SECTION in x])


def validate_map_file(source, target, env):
    """ Validate that all modules appear in the output build """
    build_dir = Path(env.subst("$BUILD_DIR"))
    map_file_path = build_dir /  env.subst("${PROGNAME}.map")

    if not map_file_path.exists():
        secho(f"ERROR: Map file not found: {map_file_path}", fg="red", err=True)
        Exit(1)

    # Identify the WLED module builders, set by load_usermods.py
    module_lib_builders = env['WLED_MODULES']

    # Extract the values we care about
    modules = {Path(builder.build_dir).name: builder.name for builder in module_lib_builders}
    secho(f"INFO: {len(modules)} libraries linked as WLED optional/user modules")

    # Now parse the map file
    map_file_contents = read_lines(map_file_path)
    usermod_object_count = count_usermod_objects(map_file_contents)
    secho(f"INFO: {usermod_object_count} usermod object entries")

    elf_path = build_dir / env.subst("${PROGNAME}.elf")
    confirmed_modules = check_elf_modules(elf_path, env, module_lib_builders)

    missing_modules = [modname for mdir, modname in modules.items() if mdir not in confirmed_modules]
    if missing_modules:
        secho(
            f"ERROR: No symbols from {missing_modules} found in linked output!",
            fg="red",
            err=True)
        Exit(1)
    return None

env.Append(LINKFLAGS=[env.subst("-Wl,--Map=${BUILD_DIR}/${PROGNAME}.map")])
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", Action(validate_map_file, cmdstr='Checking linked optional modules (usermods) in map file'))
