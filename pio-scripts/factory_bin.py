Import("env")
import csv
import json
import os
import subprocess

OUTPUT_DIR = "build_output{}".format(os.path.sep)

def _get_cpp_define_value(env, define):
    define_list = [item[-1] for item in env["CPPDEFINES"] if item[0] == define]
    if define_list:
        return define_list[0]
    return None

def _get_app_offset(env):
    try:
        with open(env.BoardConfig().get("build.partitions")) as f:
            for row in csv.reader(f):
                row = [c.strip() for c in row]
                if len(row) >= 4 and row[0] == "app0":
                    return int(row[3], 16)
    except Exception:
        pass
    return 0x10000

def create_factory_bin(source, target, env):
    if env["PIOPLATFORM"] == "espressif8266":
        return

    app_bin = os.path.normpath(str(target[0]))
    chip    = env.get("BOARD_MCU")
    flash_size = env.BoardConfig().get("upload.flash_size", "4MB")
    flash_mode = env["__get_board_flash_mode"](env)
    flash_freq = env["__get_board_f_flash"](env)
    app_offset = _get_app_offset(env)

    release_name_def = _get_cpp_define_value(env, "WLED_RELEASE_NAME")
    if release_name_def:
        release_name = release_name_def.replace("\\\"", "")
        with open("package.json") as f:
            version = json.load(f)["version"]
        out_file = os.path.join(OUTPUT_DIR, "release", f"WLED_{version}_{release_name}.factory.bin")
    else:
        out_file = os.path.join(OUTPUT_DIR, "firmware", f"{env['PIOENV']}.factory.bin")

    os.makedirs(os.path.dirname(out_file), exist_ok=True)

    cmd = [
        env.subst("$OBJCOPY"),
        "--chip", chip,
        "merge_bin",
        "--output", os.path.normpath(out_file),
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
    ]

    for section in env.subst(env.get("FLASH_EXTRA_IMAGES")):
        addr, path = section.split(" ", 1)
        cmd += [addr, os.path.normpath(path)]

    cmd += [hex(app_offset), app_bin]

    print(f"Generating factory binary: {out_file}")
    result = subprocess.run(cmd, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        print(f"Warning: factory binary generation failed:\n{result.stderr}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", create_factory_bin)
