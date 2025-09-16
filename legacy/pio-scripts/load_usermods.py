Import('env')
import os.path
from collections import deque
from pathlib import Path   # For OS-agnostic path manipulation
from platformio.package.manager.library import LibraryPackageManager

usermod_dir = Path(env["PROJECT_DIR"]) / "usermods"
all_usermods = [f for f in usermod_dir.iterdir() if f.is_dir() and f.joinpath('library.json').exists()]

if env['PIOENV'] == "usermods":
   # Add all usermods
   env.GetProjectConfig().set(f"env:usermods", 'custom_usermods', " ".join([f.name for f in all_usermods]))

def find_usermod(mod: str):
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

usermods = env.GetProjectOption("custom_usermods","")
if usermods:
  # Inject usermods in to project lib_deps
  proj = env.GetProjectConfig()
  deps = env.GetProjectOption('lib_deps')
  src_dir = proj.get("platformio", "src_dir")
  src_dir = src_dir.replace('\\','/')
  mod_paths = {mod: find_usermod(mod) for mod in usermods.split()}
  usermods = [f"{mod} = symlink://{path}" for mod, path in mod_paths.items()]
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
  # Update usermod properties
  # Set libArchive before build actions are added
  for um in (um for um in xenv.GetLibBuilders() if usermod_dir in Path(um.src_dir).parents):
    build = um._manifest.get("build", {})
    build["libArchive"] = False
    um._manifest["build"] = build

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

  for um in [dep for dep in result.depbuilders if usermod_dir in Path(dep.src_dir).parents]:
    # Add the wled folder to the include path
    um.env.PrependUnique(CPPPATH=wled_dir)
    # Add WLED's own dependencies
    for dir in extra_include_dirs:
      um.env.PrependUnique(CPPPATH=dir)

  return result

# Apply the wrapper
env.AddMethod(wrapped_ConfigureProjectLibBuilder, "ConfigureProjectLibBuilder")
