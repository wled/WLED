Import('env')
from collections import deque
from pathlib import Path   # For OS-agnostic path manipulation
import re
from urllib.parse import urlparse
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

# Names of external/registry deps listed in custom_usermods.
# Populated during parsing below; read by is_wled_module() at configure time.
_custom_usermod_names: set[str] = set()

# Matches any RFC-valid URL scheme (http, https, git, git+https, symlink, file, hg+ssh, etc.)
_URL_SCHEME_RE = re.compile(r'^[a-zA-Z][a-zA-Z0-9+.-]*://')
# SSH git URL: user@host:path  (e.g. git@github.com:user/repo.git#tag)
_SSH_URL_RE = re.compile(r'^[^@\s]+@[^@:\s]+:[^:\s]')
# Explicit custom name: "LibName = <spec>"  (PlatformIO [<name>=]<spec> form)
_NAME_EQ_RE = re.compile(r'^([A-Za-z0-9_.-]+)\s*=\s*(\S.*)')


def _is_external_entry(line: str) -> bool:
  """Return True if line is a lib_deps-style external/registry entry."""
  if _NAME_EQ_RE.match(line):              # "LibName = <spec>"
    return True
  if _URL_SCHEME_RE.match(line):           # https://, git://, symlink://, etc.
    return True
  if _SSH_URL_RE.match(line):              # git@github.com:user/repo.git
    return True
  if '@' in line:                          # "owner/Name @ ^1.0.0"
    return True
  if re.match(r'^[^/\s]+/[^/\s]+$', line):  # "owner/Name"
    return True
  return False


def _predict_dep_name(entry: str) -> str | None:
  """Predict the library name PlatformIO will assign to this dep (best-effort).

  Accuracy relies on the library's manifest "name" matching the repo/package
  name in the spec. This holds for well-authored libraries; the libArchive
  check (which requires library.json) provides an early-failure safety net.
  """
  entry = entry.strip()
  # "LibName = <spec>" — name is given explicitly; always use it
  m = _NAME_EQ_RE.match(entry)
  if m:
    return m.group(1).strip()
  # URL scheme: extract name from path
  if _URL_SCHEME_RE.match(entry):
    parsed = urlparse(entry)
    if parsed.netloc in ('github.com', 'gitlab.com', 'bitbucket.com'):
      parts = [p for p in parsed.path.split('/') if p]
      if len(parts) >= 2:
        name = parts[1]
      else:
        name = Path(parsed.path.rstrip('/')).name.strip()
      if name.endswith('.git'):
        name = name[:-4]
      return name or None
  # SSH git URL: git@github.com:user/repo.git#tag → repo
  if _SSH_URL_RE.match(entry):
    path_part = entry.split(':', 1)[1].split('#')[0].rstrip('/')
    name = Path(path_part).name
    return (name[:-4] if name.endswith('.git') else name) or None
  # Versioned registry: "owner/Name @ version" → Name
  if '@' in entry:
    name_part = entry.split('@')[0].strip()
    return name_part.split('/')[-1].strip() if '/' in name_part else name_part
  # Plain registry: "owner/Name" → Name
  if re.match(r'^[^/\s]+/[^/\s]+$', entry):
    return entry.split('/')[-1].strip()
  return None


def is_wled_module(dep: LibBuilderBase) -> bool:
  """Returns true if the specified library is a wled module."""
  return (
    usermod_dir in Path(dep.src_dir).parents
    or str(dep.name).startswith("wled-")
    or dep.name in _custom_usermod_names
  )


## Script starts here — parse custom_usermods
raw_usermods = env.GetProjectOption("custom_usermods", "")
usermods_libdeps: list[str] = []

for line in raw_usermods.splitlines():
  line = line.strip()
  if not line or line.startswith('#') or line.startswith(';'):
    continue

  if _is_external_entry(line):
    # External URL or registry entry: pass through to lib_deps unchanged.
    predicted = _predict_dep_name(line)
    if predicted:
      _custom_usermod_names.add(predicted)
    else:
      secho(
        f"WARNING: Cannot determine library name for custom_usermods entry "
        f"{line!r}. If it is not recognised as a WLED module at build time, "
        f"ensure its library.json 'name' matches the repo name.",
        fg="yellow", err=True)
    usermods_libdeps.append(line)
  else:
    # Bare name(s): split on whitespace for backwards compatibility.
    for token in line.split():
      if token == '*':
        for mod_path in sorted(usermod_dir.iterdir()):
          if mod_path.is_dir() and (mod_path / 'library.json').exists():
            _custom_usermod_names.add(mod_path.name)
            usermods_libdeps.append(f"symlink://{mod_path.resolve()}")
      else:
        resolved = find_usermod(token)
        _custom_usermod_names.add(resolved.name)
        usermods_libdeps.append(f"symlink://{resolved.resolve()}")

if usermods_libdeps:
  env.GetProjectConfig().set("env:" + env['PIOENV'], 'lib_deps', env.GetProjectOption('lib_deps') + usermods_libdeps)

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

  wled_deps = [dep for dep in result.depbuilders if is_wled_module(dep)]

  broken_usermods = []
  for dep in wled_deps:
    # Add the wled folder to the include path
    dep.env.PrependUnique(CPPPATH=str(wled_dir))
    # Add WLED's own dependencies
    for dir in extra_include_dirs:
      dep.env.PrependUnique(CPPPATH=str(dir))
    # Ensure debug info is emitted for this module's source files.
    # validate_modules.py uses `nm --defined-only -l` on the final ELF to check
    # that each module has at least one symbol placed in the binary.  The -l flag
    # reads DWARF debug sections to map placed symbols back to their original source
    # files; without -g those sections are absent and the check cannot attribute any
    # symbol to a specific module.  We scope this to usermods only — the main WLED
    # build and other libraries are unaffected.
    dep.env.AppendUnique(CCFLAGS=["-g"])
    # Enforce that libArchive is not set; we must link them directly to the executable
    if dep.lib_archive:
      broken_usermods.append(dep)

  if broken_usermods:
    broken_usermods = [usermod.name for usermod in broken_usermods]
    secho(
      f"ERROR: libArchive=false is missing on usermod(s) {' '.join(broken_usermods)} -- "
      f"modules will not compile in correctly. Add '\"build\": {{\"libArchive\": false}}' "
      f"to their library.json.",
      fg="red", err=True)
    Exit(1)

  # Save the depbuilders list for later validation
  xenv.Replace(WLED_MODULES=wled_deps)

  return result

# Apply the wrapper
env.AddMethod(wrapped_ConfigureProjectLibBuilder, "ConfigureProjectLibBuilder")
