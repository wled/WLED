Import("env")
import os
import re
from pathlib import Path   # For OS-agnostic path manipulation
import shutil
from SCons.Script import Exit

# The web UI build banner lives here (Python), not in cdata.js, so it prints
# once per build during script evaluation -- before any compilation output --
# instead of appearing partway through the build (or not at all) whenever the
# node process happened to run.
WLED_BANNER = (
    "\n"
    "\t\x1b[34m  ##  ##      ##        ######    ######\n"
    "\t\x1b[34m##      ##    ##      ##        ##  ##  ##\n"
    "\t\x1b[34m##  ##  ##  ##        ######        ##  ##\n"
    "\t\x1b[34m##  ##  ##  ##        ##            ##  ##\n"
    "\t\x1b[34m  ##  ##      ######    ######    ######\n"
    "\t\t\x1b[36m build script for web UI\n"
    "\x1b[0m"
)
print(WLED_BANNER)

node_ex = shutil.which("node")
# Check if Node.js is installed and present in PATH if it failed, abort the build
if node_ex is None:
    print('\x1b[0;31;43m' + 'Node.js is not installed or missing from PATH html css js will not be processed check https://kno.wled.ge/advanced/compiling-wled/' + '\x1b[0m')
    exitCode = env.Execute("null")
    exit(exitCode)

PROJECT_DIR = Path(env["PROJECT_DIR"]).resolve()
CDATA_JS = PROJECT_DIR / "tools" / "cdata.js"


# --- Dependency graph -----------------------------------------------------------
#
# cdata.js is the single source of truth for which web UI headers exist and what
# they depend on.  Every time it builds, it leaves behind a Makefile-style ".d"
# depfile describing that graph.  We treat that file purely as a *cache from the
# previous build*: if it is present we feed its targets/sources to a single SCons
# Command (covering the main UI and every usermod) so SCons tracks the web UI's
# inputs and outputs natively; if it is absent -- the first build, or it was
# deleted -- we simply force the build once and read the depfile it leaves behind.
#
def _resolve_dep(token):
    """Un-escape a depfile token ("\\ " -> " ") and resolve it against the project."""
    token = token.strip().replace('\\ ', ' ')
    return os.path.normpath(os.path.join(str(PROJECT_DIR), token))


def _expand_source(ap):
    """Map one dependency token to concrete source files.  An html job records its
    whole source *folder* as the dependency; expanding it to the folder's current
    files -- walked fresh every build -- means SCons sees today's file set, so
    adding or removing a file there changes the dependency set and marks the UI
    build out of date.  A regular file maps to itself.  A missing path yields
    nothing: a stale token for a just-removed file must not become a non-existent
    SCons source (which would abort the build)."""
    if os.path.isdir(ap):
        return [os.path.join(root, n) for root, _dirs, names in os.walk(ap) for n in names]
    return [ap] if os.path.exists(ap) else []


def _parse_depfile(depfile):
    """Read a Makefile-style depfile into (targets, sources) lists of absolute
    paths.  Directory tokens are expanded to their current files (see
    _expand_source).  Paths are project-relative with forward slashes (no Windows
    drive-letter colons); spaces within a path are escaped as "\\ ", so the
    dependency list is split only on *unescaped* whitespace."""
    targets, sources, seen = [], [], set()
    for line in depfile.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith('#') or ':' not in line:
            continue
        target, deps = line.split(':', 1)
        targets.append(_resolve_dep(target))
        for dep in re.split(r'(?<!\\)\s+', deps.strip()):
            if not dep:
                continue
            for ap in _expand_source(_resolve_dep(dep)):
                if ap not in seen:
                    seen.add(ap)
                    sources.append(ap)
    return targets, sources


def _ensure_node_modules(env):
    """Install node packages only when needed, rather than on every UI build."""
    node_modules = PROJECT_DIR / "node_modules"
    lock = PROJECT_DIR / "package-lock.json"
    stale = (not node_modules.exists()) or (
        lock.exists() and lock.stat().st_mtime > node_modules.stat().st_mtime
    )
    if stale:
        print('\x1b[6;33;42m' + 'Installing node packages' + '\x1b[0m')
        rc = env.Execute("npm ci")
        if rc:
            print('\x1b[0;31;43m' + 'npm ci failed check https://kno.wled.ge/advanced/compiling-wled/' + '\x1b[0m')
            Exit(rc)


def _wire_object_methods(target_env, cmd):
    """Make every object compiled by target_env require the UI build command, so
    the generated headers exist before anything #including them compiles -- even
    under a parallel (-j) build."""
    for name in ("Object", "StaticObject", "SharedObject"):
        if not hasattr(target_env, name):
            continue
        orig = getattr(target_env, name)
        def wrapped(*args, _orig=orig, _env=target_env, **kwargs):
            nodes = _orig(*args, **kwargs)
            _env.Requires(nodes, cmd)
            return nodes
        setattr(target_env, name, wrapped)


# Guard so the UI build is registered at most once per environment.
_registered = {"done": False}


def _register_ui_build(xenv, result):
    if _registered["done"]:
        return
    _registered["done"] = True

    # Usermod HTML manifests discovered by load_usermods.py (may be empty).
    manifests = list(xenv.get("WLED_UI_MANIFESTS", []))
    depfile = Path(xenv.subst("$BUILD_DIR")).resolve() / "ui_deps.d"

    # The single node invocation that builds the whole web UI (main + usermods)
    # and refreshes the depfile.  The manifest set is baked into the action, so
    # adding or removing a usermod changes the command's build signature and
    # re-runs it -- which is how a new usermod's headers get built and recorded.
    node_cmd = " ".join(
        ['node', '"%s"' % CDATA_JS, '--depfile', '"%s"' % depfile]
        + ['--manifest "%s"' % m for m in manifests]
    )

    def _build_ui(target, source, env, _cmd=node_cmd):
        _ensure_node_modules(env)
        rc = env.Execute(_cmd)
        if rc:
            print('\x1b[0;31;43m' + 'Web UI build failed check https://kno.wled.ge/advanced/compiling-wled/' + '\x1b[0m')
        return rc

    action = env.VerboseAction(_build_ui, "Building web UI")

    # Feed the previous build's depfile to SCons when we have one; otherwise force
    # a single build and pick up the depfile it leaves behind for next time.
    targets, sources = _parse_depfile(depfile) if depfile.exists() else ([], [])
    if targets:
        ui_cmd = env.Command(
            target=[env.File(t) for t in targets],
            source=[env.File(s) for s in sources],
            action=action,
        )
    else:
        # First build (or a deleted depfile): we know cdata.js must run and do not
        # yet know what it produces.  Make the depfile the target and force the
        # run; the order-only prerequisite wired below still guarantees the
        # generated headers land before any C++ that #includes them compiles.
        ui_cmd = env.Command(target=[env.File(str(depfile))], source=[], action=action)
        env.AlwaysBuild(ui_cmd)

    # By default SCons removes a builder's targets before running its action.
    # Because every header is a target of this one Command, editing any single
    # UI source would delete ALL of them, cdata.js would then see them missing
    # and rebuild every one (each with a fresh WEB_BUILD_TIME), and everything
    # that #includes a generated header would needlessly recompile.  Mark the
    # targets Precious so SCons leaves them in place and cdata.js's own
    # per-job staleness check does the incremental rebuild.  (Precious only
    # affects pre-build removal, not `pio run -t clean`.)
    env.Precious(ui_cmd)

    # Order the build ahead of compilation.  PlatformIO compiles the main WLED
    # sources with result.env (the project-as-library builder's env; see
    # ProcessProjectDeps -> plb.env.BuildSources), and each usermod's sources
    # with that module's own env -- so we wire both.
    _wire_object_methods(result.env, ui_cmd)
    # Match on realpath both sides: manifests come from load_usermods.py already
    # resolved, and a symlink:// usermod's src_dir may reach through a symlink,
    # so a plain normpath comparison could miss and leave its objects unwired.
    manifest_dirs = {os.path.realpath(os.path.dirname(m)) for m in manifests}
    for dep in result.depbuilders:
        if os.path.realpath(str(dep.src_dir)) in manifest_dirs:
            _wire_object_methods(dep.env, ui_cmd)

    # Belt-and-suspenders: also an explicit prerequisite of the final firmware.
    xenv.Depends("$BUILD_DIR/$PROGNAME$PROGSUFFIX", ui_cmd)


# Hook ConfigureProjectLibBuilder *after* load_usermods.py's wrapper, so the
# usermod manifest list is already populated and the returned builder (whose env
# compiles the main sources) is available.  This script is listed after
# load_usermods.py in platformio.ini, so our wrapper is outermost and runs last.
# We only register nodes here; the build runs later, at build time.
_old_ConfigureProjectLibBuilder = env.ConfigureProjectLibBuilder

def _wrapped_ConfigureProjectLibBuilder(xenv):
    result = _old_ConfigureProjectLibBuilder.clone(xenv)()
    _register_ui_build(xenv, result)
    return result

env.AddMethod(_wrapped_ConfigureProjectLibBuilder, "ConfigureProjectLibBuilder")
