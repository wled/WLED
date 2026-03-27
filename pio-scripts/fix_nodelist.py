Import("env")
# ── fix_nodelist.py ───────────────────────────────────────────────────────────
# Pre-build monkey-patch that prevents a clean-build crash in dual-framework
# (arduino + espidf) PlatformIO builds.
#
# Root cause
# ----------
# platformio/builder/tools/piobuild.py::CollectBuildFiles() calls
#   env.File(os.path.join(_var_dir, os.path.basename(item)))
# for each matched source file.  When multiple VariantDir mappings overlap
# (which happens with libraries that have sub-directories such as the
# Arduino WebServer library's detail/, middleware/, uri/ sub-trees), SCons
# can return a NodeList instead of a single Node.
#
# Additionally, middlewares such as arduino.py's smart_include_length_shorten
# call env.Object(node) which also returns a NodeList (SCons Object builder
# returns a list).  When the next middleware (_skip_prj_source_files from
# espidf.py) receives a NodeList, it calls node.srcnode().get_path() which
# fails with:
#   AttributeError: 'NodeList' object has no attribute 'srcnode'
#   (or produces a wrong result when NodeList.__getattr__ proxies the call).
#
# Fix
# ---
# We monkey-patch CollectBuildFiles to:
#  1. Flatten NodeList items produced by env.File() BEFORE the middleware loop.
#  2. Flatten NodeList values returned BY middleware callbacks during the loop.
#     When a callback returns a NodeList, all elements pass the remaining
#     middlewares individually.
# ─────────────────────────────────────────────────────────────────────────────

import fnmatch as _fnmatch
import os as _os

from platformio.builder.tools import piobuild as _piobuild
from platformio.builder.tools.piobuild import SRC_BUILD_EXT


def _is_nodelist(node):
    """Return True if node is a SCons NodeList (a UserList subclass that
    proxies attribute access but does not have get_path() directly)."""
    return hasattr(node, 'data') and not hasattr(node, 'get_path')


def _iter_nodes(node):
    """Yield individual SCons nodes from node, recursively unwrapping NodeList."""
    if _is_nodelist(node):
        for child in node.data:
            yield from _iter_nodes(child)
    else:
        yield node


def _run_middlewares(env, node, middlewares):
    """Run all middlewares on a single node.  Returns a list of result nodes
    (empty if the node was filtered out, >1 if a middleware returned a NodeList).
    """
    current_nodes = [node]
    for callback, pattern in middlewares:
        next_nodes = []
        for n in current_nodes:
            if pattern and not _fnmatch.fnmatch(n.srcnode().get_path(), pattern):
                next_nodes.append(n)  # pattern didn't match – pass through unchanged
                continue
            if callback.__code__.co_argcount == 2:
                result = callback(env, n)
            else:
                result = callback(n)
            if result is None:
                pass  # filtered out
            elif _is_nodelist(result):
                next_nodes.extend(_iter_nodes(result))
            else:
                next_nodes.append(result)
        current_nodes = next_nodes
        if not current_nodes:
            break  # all nodes filtered out
    return current_nodes


def _patched_CollectBuildFiles(env, variant_dir, src_dir, src_filter=None, duplicate=False):
    """Drop-in replacement for piobuild.CollectBuildFiles that handles NodeList
    nodes both from env.File() and from middleware callbacks."""
    from platformio.builder.tools.piobuild import SRC_BUILD_EXT as _SRC_BUILD_EXT

    sources = []
    variants = []

    src_dir = env.subst(src_dir)
    if src_dir.endswith(_os.sep):
        src_dir = src_dir[:-1]

    for item in env.MatchSourceFiles(src_dir, src_filter, _SRC_BUILD_EXT):
        _reldir = _os.path.dirname(item)
        _src_dir = _os.path.join(src_dir, _reldir) if _reldir else src_dir
        _var_dir = _os.path.join(variant_dir, _reldir) if _reldir else variant_dir

        if _var_dir not in variants:
            variants.append(_var_dir)
            env.VariantDir(_var_dir, _src_dir, duplicate)

        raw_node = env.File(_os.path.join(_var_dir, _os.path.basename(item)))
        # Flatten: a NodeList becomes individual nodes; a plain Node stays as-is
        for n in _iter_nodes(raw_node):
            sources.append(n)

    middlewares = env.get("__PIO_BUILD_MIDDLEWARES")
    if not middlewares:
        return sources

    new_sources = []
    for node in sources:
        for result_node in _run_middlewares(env, node, middlewares):
            new_sources.append(result_node)

    return new_sources


# Replace the function in the piobuild module so all subsequent callers
# (espidf.py, etc.) pick up the patched version.
_piobuild.CollectBuildFiles = _patched_CollectBuildFiles
# Also replace the env-bound method so env.CollectBuildFiles() uses our version.
env.AddMethod(_patched_CollectBuildFiles, "CollectBuildFiles")

print("[fix_nodelist] CollectBuildFiles patched – NodeList flattening enabled")
