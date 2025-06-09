import re
from pathlib import Path   # For OS-agnostic path manipulation
from typing import Iterable
from click import secho
from SCons.Script import Action, Exit
from platformio.builder.tools.piolib import LibBuilderBase


def is_wled_module(env, dep: LibBuilderBase) -> bool:
  """Returns true if the specified library is a wled module
  """
  usermod_dir = Path(env["PROJECT_DIR"]).resolve() / "usermods"
  return usermod_dir in Path(dep.src_dir).parents or str(dep.name).startswith("wled-")


def read_lines(p: Path):
    """ Read in the contents of a file for analysis """
    with p.open("r", encoding="utf-8", errors="ignore") as f:
        return f.readlines()


def check_map_file_objects(map_file: list[str], dirs: Iterable[str]) -> set[str]:
    """ Identify which dirs contributed to the final build

        Returns the (sub)set of dirs that are found in the output ELF
    """
    # Pattern to match symbols in object directories
    # Join directories into alternation
    usermod_dir_regex = "|".join([re.escape(dir) for dir in dirs])
    # Matches nonzero address, any size, and any path in a matching directory
    object_path_regex = re.compile(r"0x0*[1-9a-f][0-9a-f]*\s+0x[0-9a-f]+\s+\S+[/\\](" + usermod_dir_regex + r")[/\\]\S+\.o")

    found = set()
    for line in map_file:
        matches = object_path_regex.findall(line)
        for m in matches:
            found.add(m)
    return found


def count_usermod_objects(map_file: list[str]) -> int:
    """ Returns the number of usermod objects in the usermod list """
    # Count the number of entries in the usermods table section
    return len([x for x in map_file if ".dtors.tbl.usermods.1" in x])


def validate_map_file(source, target, env):
    """ Validate that all modules appear in the output build """
    build_dir = Path(env.subst("$BUILD_DIR"))
    map_file_path = build_dir /  env.subst("${PROGNAME}.map")

    if not map_file_path.exists():
        secho(f"ERROR: Map file not found: {map_file_path}", fg="red", err=True)
        Exit(1)

    # Identify the WLED module source directories
    module_lib_builders = [builder for builder in env.GetLibBuilders() if is_wled_module(env, builder)]

    if env.GetProjectOption("custom_usermods","") == "*":
        # All usermods build; filter non-platform-OK modules
        module_lib_builders = [builder for builder in module_lib_builders if env.IsCompatibleLibBuilder(builder)]
    else:
        incompatible_builders = [builder for builder in module_lib_builders if not env.IsCompatibleLibBuilder(builder)]
        if incompatible_builders:
            secho(
                f"ERROR: Modules {[b.name for b in incompatible_builders]} are not compatible with this platform!",
                fg="red",
                err=True)
            Exit(1)

    # Extract the values we care about
    modules = {Path(builder.build_dir).name: builder.name for builder in module_lib_builders}
    secho(f"INFO: {len(modules)} libraries linked as WLED optional/user modules")

    # Now parse the map file
    map_file_contents = read_lines(map_file_path)
    usermod_object_count = count_usermod_objects(map_file_contents)
    secho(f"INFO: {usermod_object_count} usermod object entries")

    confirmed_modules = check_map_file_objects(map_file_contents, modules.keys())
    missing_modules = [modname for mdir, modname in modules.items() if mdir not in confirmed_modules]
    if missing_modules:
        secho(
            f"ERROR: No object files from {missing_modules} found in linked output!",
            fg="red",
            err=True)
        Exit(1)
    return None

Import("env")
env.Append(LINKFLAGS=[env.subst("-Wl,--Map=${BUILD_DIR}/${PROGNAME}.map")])
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", Action(validate_map_file, cmdstr='Checking linked optional modules (usermods) in map file'))
