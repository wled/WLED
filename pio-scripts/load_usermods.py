Import('env')
import os.path
from collections import deque
from pathlib import Path   # For OS-agnostic path manipulation
from click import secho
from SCons.Script import Exit
from platformio.builder.tools.piolib import LibBuilderBase
from platformio.package.manager.library import LibraryPackageManager

usermod_dir = Path(env["PROJECT_DIR"]).resolve() / "usermods"

# "usermods" environment: expand list of usermods to everything in the folder
if env['PIOENV'] == "usermods":
   # Add all usermods
   all_usermods = [f for f in usermod_dir.iterdir() if f.is_dir() and f.joinpath('library.json').exists()]
   env.GetProjectConfig().set(f"env:usermods", 'custom_usermods', " ".join([f.name for f in all_usermods]))

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
if usermods:
  # Inject usermods in to project lib_deps
  proj = env.GetProjectConfig()
  deps = env.GetProjectOption('lib_deps')
  src_dir = proj.get("platformio", "src_dir")
  src_dir = src_dir.replace('\\','/')
  mod_paths = {mod: find_usermod(mod) for mod in usermods.split()}
  usermods = [f"{mod} = symlink://{path.resolve()}" for mod, path in mod_paths.items()]
  proj.set("env:" + env['PIOENV'], 'lib_deps', deps + usermods)
  # Force usermods to be installed in to the environment build state before the LDF runs
  # Otherwise we won't be able to see them until it's too late to change their paths for LDF
  # Logic is largely borrowed from PlaformIO internals
  not_found_specs = []
  for spec in usermods:
    found = False
    for storage_dir in env.GetLibSourceDirs():
      #print(f"Checking {storage_dir} for {spec}")
      lm = LibraryPackageManager(storage_dir)
      if lm.get_package(spec):
          #print("Found!")
          found = True
          break
    if not found:
        #print("Missing!")
        not_found_specs.append(spec)
  if not_found_specs:
      lm = LibraryPackageManager(
          env.subst(os.path.join("$PROJECT_LIBDEPS_DIR", "$PIOENV"))
      )
      for spec in not_found_specs:
        #print(f"LU: forcing install of {spec}")
        lm.install(spec)


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
