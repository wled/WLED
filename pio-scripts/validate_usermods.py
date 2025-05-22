import re
import sys
from pathlib import Path   # For OS-agnostic path manipulation
from click import secho
from SCons.Script import Action, Exit
from platformio import util

def read_lines(p: Path):
    """ Read in the contents of a file for analysis """
    with p.open("r", encoding="utf-8", errors="ignore") as f:
        return f.readlines()

def check_map_file_objects(map_file: list[str], usermod_dirs: list[str]) -> set[str]:
    """ Checks that an object file from each usermod_dir appears in the linked output

        Returns the (sub)set of usermod_dirs that are found in the output ELF
    """
    # Pattern to match symbols in object directories
    # Join directories into alternation
    usermod_dir_regex = "|".join([re.escape(dir) for dir in usermod_dirs])
    # Matches nonzero address, any size, and any path in a matching directory
    object_path_regex = re.compile(r"0x0*[1-9a-f][0-9a-f]*\s+0x[0-9a-f]+\s+\S+[/\\](" + usermod_dir_regex + r")[/\\]\S+\.o")

    found = set()
    for line in map_file:
        matches = object_path_regex.findall(line)
        for m in matches:
            found.add(m)
    return found

def count_registered_usermods(map_file: list[str]) -> int:
    """ Returns the number of usermod objects in the usermod list """
    # Count the number of entries in the usermods table section
    return len([x for x in map_file if ".dtors.tbl.usermods.1" in x])


def validate_map_file(source, target, env):
    """ Validate that all usermods appear in the output build """
    build_dir = Path(env.subst("$BUILD_DIR"))
    map_file_path = build_dir /  env.subst("${PROGNAME}.map")

    if not map_file_path.exists():
        secho(f"ERROR: Map file not found: {map_file_path}", fg="red", err=True)
        Exit(1)

    # Load project settings
    usermods = env.GetProjectOption("custom_usermods","").split()
    libdeps = env.GetProjectOption("lib_deps", [])
    lib_builders = env.GetLibBuilders()

    secho(f"INFO: Expecting {len(usermods)} usermods: {', '.join(usermods)}")

    # Map the usermods to libdeps; every usermod should have one
    usermod_dirs = []
    for mod in usermods:
        modstr = f"{mod} = symlink://"
        this_mod_libdeps = [libdep[len(modstr):] for libdep in libdeps if libdep.startswith(modstr)]
        if not this_mod_libdeps:
            secho(
                f"ERROR: Usermod {mod} not found in build libdeps!",
                fg="red",
                err=True)
            Exit(1)
        # Save only the final folder name
        usermod_dir = Path(this_mod_libdeps[0]).name
        # Search lib_builders
        this_mod_builders = [builder for builder in lib_builders if Path(builder.src_dir).name == usermod_dir]
        if not this_mod_builders:
            secho(
                f"ERROR: Usermod {mod} not found in library builders!",
                fg="red",
                err=True)
            Exit(1)
        usermod_dirs.append(usermod_dir)

    # Now parse the map file
    map_file_contents = read_lines(map_file_path)
    confirmed_usermods = check_map_file_objects(map_file_contents, usermod_dirs)
    usermod_object_count = count_registered_usermods(map_file_contents)

    secho(f"INFO: {len(usermod_dirs)}/{len(usermods)} libraries linked via custom_usermods, producing {usermod_object_count} usermod object entries")
    missing_usermods = set(usermod_dirs).difference(confirmed_usermods)
    if missing_usermods:
        secho(
            f"ERROR: No object files from {missing_usermods} found in linked output!",
            fg="red",
            err=True)
        Exit(1)
    return None

Import("env")
env.Append(LINKFLAGS=[env.subst("-Wl,--Map=${BUILD_DIR}/${PROGNAME}.map")])
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", Action(validate_map_file, cmdstr='Checking linked usermods in map file...'))
