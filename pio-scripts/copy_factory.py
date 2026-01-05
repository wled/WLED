Import('env')
import os
import shutil
import gzip
import json

OUTPUT_DIR = "build_output{}".format(os.path.sep)
#OUTPUT_DIR = os.path.join("build_output")

def _get_cpp_define_value(env, define):
    define_list = [item[-1] for item in env["CPPDEFINES"] if item[0] == define]

    if define_list:
        return define_list[0]

    return None

def _create_dirs(dirs=["factory"]):
    for d in dirs:
        os.makedirs(os.path.join(OUTPUT_DIR, d), exist_ok=True)

def get_release_basename(env):
    release_name_def = _get_cpp_define_value(env, "WLED_RELEASE_NAME")
    if not release_name_def:
        return None

    release_name = release_name_def.replace("\\\"", "")
    with open("package.json", "r") as package:
        version = json.load(package)["version"]

    return f"WLED_{version}_{release_name}"

def copy_factory(source, target, env):
    release_base = get_release_basename(env)
    _create_dirs()
    variant = env["PIOENV"]
    builddir = os.path.join(env["PROJECT_BUILD_DIR"], variant)
    source_factory = os.path.join(builddir, env["PROGNAME"] + ".factory.bin")

    # factory file naming
    if release_base:
        factory_file = os.path.join(
            OUTPUT_DIR, "factory", f"{release_base}.factory.bin"
        )
    else:
        factory_file = "{}factory{}{}.factory.bin".format(
            OUTPUT_DIR, os.path.sep, variant
        )

    # copy factory binary
    if os.path.isfile(source_factory):
        print(f"Found factory binary Copying {source_factory} to {factory_file}")
        shutil.copy(source_factory, factory_file)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_factory)