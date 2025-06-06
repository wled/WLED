Import('env')
from collections import deque
from pathlib import Path   # For OS-agnostic path manipulation
from click import secho
from SCons.Script import Exit
from platformio.builder.tools.piolib import LibBuilderBase

usermod_dir = Path(env["PROJECT_DIR"]).resolve() / "usermods"

# Utility functions
def find_usermod(mod: str) -> Path:
  """Locate this library in the usermods folder.
     We do this to avoid needing to rename a bunch of folders;
     this could be removed later
  """
  # Check name match
  mp = usermod_dir / mod
  if mp.exists():
    return mp
  mp = usermod_dir / f"{mod}_v2"
  if mp.exists():
    return mp
  mp = usermod_dir / f"usermod_v2_{mod}"
  if mp.exists():
    return mp
  raise RuntimeError(f"Couldn't locate module {mod} in usermods directory!")

def is_wled_module(dep: LibBuilderBase) -> bool:
  """Returns true if the specified library is a wled module
  """
  return usermod_dir in Path(dep.src_dir).parents or str(dep.name).startswith("wled-")

## Script starts here
# Process usermod option
usermods = env.GetProjectOption("custom_usermods","")

# Handle "all usermods" case
if usermods == '*':
  usermods = [f.name for f in usermod_dir.iterdir() if f.is_dir() and f.joinpath('library.json').exists()]
else:
  usermods = usermods.split()

if usermods:
  # Inject usermods in to project lib_deps
  symlinks = [f"symlink://{find_usermod(mod).resolve()}" for mod in usermods]
  env.GetProjectConfig().set("env:" + env['PIOENV'], 'lib_deps', env.GetProjectOption('lib_deps') + symlinks)

# Utility function for assembling usermod include paths
def cached_add_includes(dep, dep_cache: set, includes: deque):
  """ Add dep's include paths to includes if it's not in the cache """
  if dep not in dep_cache:
    dep_cache.add(dep)
    for include in dep.get_include_dirs():
      if include not in includes:
        includes.appendleft(include)
      if usermod_dir not in Path(dep.src_dir).parents:
        # Recurse, but only for NON-usermods
        for subdep in dep.depbuilders:
          cached_add_includes(subdep, dep_cache, includes)

# Monkey-patch ConfigureProjectLibBuilder to mark up the dependencies
# Save the old value
old_ConfigureProjectLibBuilder = env.ConfigureProjectLibBuilder

# Our new wrapper
def wrapped_ConfigureProjectLibBuilder(xenv):
  # Call the wrapped function
  result = old_ConfigureProjectLibBuilder.clone(xenv)()

  # Fix up include paths
  # In PlatformIO >=6.1.17, this could be done prior to ConfigureProjectLibBuilder
  wled_dir = xenv["PROJECT_SRC_DIR"]
  # Build a list of dependency include dirs
  # TODO: Find out if this is the order that PlatformIO/SCons puts them in??
  processed_deps = set()
  extra_include_dirs = deque()  # Deque used for fast prepend
  for dep in result.depbuilders:
     cached_add_includes(dep, processed_deps, extra_include_dirs)

  broken_usermods = []
  for dep in result.depbuilders:
    if is_wled_module(dep):
      # Add the wled folder to the include path
      dep.env.PrependUnique(CPPPATH=str(wled_dir))
      # Add WLED's own dependencies
      for dir in extra_include_dirs:
        dep.env.PrependUnique(CPPPATH=str(dir))
      # Enforce that libArchive is not set; we must link them directly to the executable
      if dep.lib_archive:
        broken_usermods.append(dep)

  if broken_usermods:
    broken_usermods = [usermod.name for usermod in broken_usermods]
    secho(
      f"ERROR: libArchive=false is missing on usermod(s) {' '.join(broken_usermods)} -- modules will not compile in correctly",
      fg="red",
      err=True)
    Exit(1)

  return result

# Apply the wrapper
env.AddMethod(wrapped_ConfigureProjectLibBuilder, "ConfigureProjectLibBuilder")
