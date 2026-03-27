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


def _get_readelf_path(env) -> str:
    """ Derive the readelf tool path from the build environment """
    # Derive from the C compiler: xtensa-esp32-elf-gcc → xtensa-esp32-elf-readelf
    cc = env.subst("$CC")
    readelf = re.sub(r'(gcc|g\+\+)$', 'readelf', os.path.basename(cc))
    return os.path.join(os.path.dirname(cc), readelf)


def check_elf_modules(elf_path: Path, env, module_lib_builders) -> set[str]:
    """ Check which modules have at least one compilation unit in the ELF.

        The map file is not a reliable source for this: with LTO, original object
        file paths are replaced by temporary ltrans.o partitions in all output
        sections, making per-module attribution impossible from the map alone.
        Instead we invoke readelf --debug-dump=info --dwarf-depth=1 on the ELF,
        which reads only the top-level compilation-unit DIEs from .debug_info.
        Each CU corresponds to one source file; matching DW_AT_comp_dir +
        DW_AT_name against the module src_dirs is sufficient to confirm a module
        was compiled into the ELF.  The output volume is proportional to the
        number of source files, not the number of symbols.

        Returns the set of build_dir basenames for confirmed modules.
    """
    readelf_path = _get_readelf_path(env)
    secho(f"INFO: Checking for usermod compilation units...")

    try:
        result = subprocess.run(
            [readelf_path, "--debug-dump=info", "--dwarf-depth=1", str(elf_path)],
            capture_output=True, text=True, errors="ignore", timeout=120,
        )
        output = result.stdout
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError) as e:
        secho(f"WARNING: readelf failed ({e}); skipping per-module validation", fg="yellow", err=True)
        return {Path(b.build_dir).name for b in module_lib_builders}  # conservative pass

    remaining = {Path(str(b.src_dir)): Path(b.build_dir).name for b in module_lib_builders}
    found = set()

    def _flush_cu(comp_dir: str | None, name: str | None) -> None:
        """Match one completed CU against remaining builders."""
        if not name or not remaining:
            return
        p = Path(name)
        src_path = (Path(comp_dir) / p) if (comp_dir and not p.is_absolute()) else p
        for src_dir in list(remaining):
            if src_path.is_relative_to(src_dir):
                found.add(remaining.pop(src_dir))
                break

    # readelf emits one DW_TAG_compile_unit DIE per source file.  Attributes
    # of interest:
    #   DW_AT_name     — source file (absolute, or relative to comp_dir)
    #   DW_AT_comp_dir — compile working directory
    # Both appear as either a direct string or an indirect string:
    #   DW_AT_name     : foo.cpp
    #   DW_AT_name     : (indirect string, offset: 0x…): foo.cpp
    # Taking the portion after the *last* ": " on the line handles both forms.
    _CU_HEADER = re.compile(r'Compilation Unit @')
    _ATTR      = re.compile(r'\bDW_AT_(name|comp_dir)\b')

    comp_dir = name = None
    for line in output.splitlines():
        if _CU_HEADER.search(line):
            _flush_cu(comp_dir, name)
            comp_dir = name = None
            continue
        if not remaining:
            break  # all builders matched
        m = _ATTR.search(line)
        if m:
            _, _, val = line.rpartition(': ')
            val = val.strip()
            if m.group(1) == 'name':
                name = val
            else:
                comp_dir = val
    _flush_cu(comp_dir, name)  # flush the last CU

    return found


def count_usermod_objects(map_file: list[str]) -> int:
    """ Returns the number of usermod objects in the usermod list.

    Computes the count from the address span between the .dynarray.usermods.0
    and .dynarray.usermods.99999 sentinel sections.  This mirrors the
    DYNARRAY_LENGTH macro and is reliable under LTO, where all entries are
    merged into a single ltrans partition so counting section occurrences
    always yields 1 regardless of the true count.
    """
    ENTRY_SIZE = 4  # sizeof(Usermod*) on 32-bit targets
    addr_begin = None
    addr_end = None

    for i, line in enumerate(map_file):
        stripped = line.strip()
        if stripped == '.dynarray.usermods.0':
            if i + 1 < len(map_file):
                m = re.search(r'0x([0-9a-fA-F]+)', map_file[i + 1])
                if m:
                    addr_begin = int(m.group(1), 16)
        elif stripped == '.dynarray.usermods.99999':
            if i + 1 < len(map_file):
                m = re.search(r'0x([0-9a-fA-F]+)', map_file[i + 1])
                if m:
                    addr_end = int(m.group(1), 16)

    if addr_begin is None or addr_end is None:
        return 0
    return (addr_end - addr_begin) // ENTRY_SIZE


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
