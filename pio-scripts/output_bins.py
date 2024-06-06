Import('env')
import os
import shutil
import gzip

OUTPUT_DIR = "build_output{}".format(os.path.sep)

def _get_cpp_define_value(env, define):
    define_list = [item[-1] for item in env["CPPDEFINES"] if item[0] == define]

    if define_list:
        return define_list[0]

    return None

def _create_dirs(dirs=["firmware", "map"]):
    # check if output directories exist and create if necessary
    if not os.path.isdir(OUTPUT_DIR):
        os.mkdir(OUTPUT_DIR)

    for d in dirs:
        if not os.path.isdir("{}{}".format(OUTPUT_DIR, d)):
            os.mkdir("{}{}".format(OUTPUT_DIR, d))


# trick for py2/3 compatibility
if 'basestring' not in globals():
    basestring = str

# WLEDMM : custom print function
def print_my_item(items, flag = False):
    if flag: print("  -D", end='')
    else:    print("  ", end='')
    if isinstance(items, basestring):
        # print a single string
        print(items, end='')
    else:
        # print a list
        first = True
        for item in items:
            if not first: print("=", end='')
            print(item, end='')
            first = False

# WLEDMM : dump out buildflags : usermods, disable, enable, use_..
def wledmm_print_build_info(env):
    all_flags = env["CPPDEFINES"]
    first = True

    found = False
    for item in all_flags:
        if  'WLED_RELEASE_NAME' in item[0] or 'WLED_VERSION' in item[0] or 'ARDUINO_USB_CDC_ON_BOOT' in item[0]:
            if first: print("\nUsermods and Features:")
            print_my_item(item)
            first = False
            found = True
    if found: print("")

    found = False
    for item in all_flags: 
        if 'USERMOD_' in item or 'UM_' in item:
            if first: print("\nUsermods and Features:")
            print_my_item(item)
            first = False
            found = True
    if found: print("")

    found = False
    for item in all_flags: 
        if 'WLED_DISABLE' in item or 'WIFI_FIX' in item:
            if first: print("\nUsermods and Features:")
            print_my_item(item)
            first = False
            found = True
    if found: print("")

    found = False
    for item in all_flags:
        if 'WLED_' in item or 'WLED_' in item[0] or 'MAX_LED' in item[0]:
            if not 'WLED_RELEASE_NAME' in item[0] and not 'WLED_VERSION' in item[0] and not 'WLED_WATCHDOG_TIMEOUT' in item[0] and not 'WLED_DISABLE' in item and not 'WLED_USE_MY_CONFIG' in item and not 'ARDUINO_PARTITION' in item:
                if first: print("\nUsermods and Features:")
                print_my_item(item)
                first = False
                found = True
    if found: print("")

    first = True
    found = False
    for item in all_flags: 
        if 'WLEDMM_' in item[0] or 'WLEDMM_' in item or 'TROYHACKS' in item:
            if first: print("\nWLEDMM Features:")
            print_my_item(item)
            first = False
            found = True
    if found: print("\n")

def wledmm_print_all_defines(env):
    all_flags = env["CPPDEFINES"]
    found = False
    for item in all_flags:
        if not found: print("\nBuild Flags:")
        print_my_item(item, True)
        found = True
    if found: print("\n")


def bin_rename_copy(source, target, env):
    _create_dirs()
    variant = env["PIOENV"]
    builddir = os.path.join(env["PROJECT_BUILD_DIR"],  variant)
    source_map = os.path.join(builddir, env["PROGNAME"] + ".map")

    # create string with location and file names based on variant
    map_file = "{}map{}{}.map".format(OUTPUT_DIR, os.path.sep, variant)
    bin_file = "{}firmware{}{}.bin".format(OUTPUT_DIR, os.path.sep, variant)

    release_name = _get_cpp_define_value(env, "WLED_RELEASE_NAME")

    if release_name:
        _create_dirs(["release"])
        version = _get_cpp_define_value(env, "WLED_VERSION")
        release_file = "{}release{}WLEDMM_{}_{}.bin".format(OUTPUT_DIR, os.path.sep, version, release_name) #WLEDMM: add MM postfix to WLED
        shutil.copy(str(target[0]), release_file)

    # check if new target files exist and remove if necessary
    for f in [map_file, bin_file]:
        if os.path.isfile(f):
            os.remove(f)

    # copy firmware.bin to firmware/<variant>.bin
    shutil.copy(str(target[0]), bin_file)

    # copy firmware.map to map/<variant>.map
    if os.path.isfile("firmware.map"):
        print("Found linker mapfile firmware.map")
        shutil.copy("firmware.map", map_file)
    if os.path.isfile(source_map):
        print(f"Found linker mapfile {source_map}")
        shutil.copy(source_map, map_file)

    # wledmm_print_all_defines(env)
    # wledmm_print_build_info(env)

def bin_gzip(source, target, env):
    _create_dirs()
    variant = env["PIOENV"]

    # create string with location and file names based on variant
    bin_file = "{}firmware{}{}.bin".format(OUTPUT_DIR, os.path.sep, variant)
    gzip_file = "{}firmware{}{}.bin.gz".format(OUTPUT_DIR, os.path.sep, variant)

    # check if new target files exist and remove if necessary
    if os.path.isfile(gzip_file): os.remove(gzip_file)

    # write gzip firmware file
    with open(bin_file,"rb") as fp:
        with gzip.open(gzip_file, "wb", compresslevel = 9) as f:
            shutil.copyfileobj(fp, f)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [bin_rename_copy, bin_gzip])
