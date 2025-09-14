Import('env')
import os
import shutil
import gzip

OUTPUT_DIR = "build_output{}".format(os.path.sep)
#OUTPUT_DIR = os.path.join("build_output")

def _get_cpp_define_value(env, define):
    define_list = [item[-1] for item in env["CPPDEFINES"] if item[0] == define]

    if define_list:
        return define_list[0]

    return None

def _create_dirs(dirs=["map", "release", "firmware"]):
    for d in dirs:
        os.makedirs(os.path.join(OUTPUT_DIR, d), exist_ok=True)

def add_metadata_header(binary_file, release_name):
    """Add WLED release metadata header to binary file"""
    # Metadata format: "WLED_META:" + release_name + null terminator + padding to 64 bytes + original binary
    header_prefix = b"WLED_META:"
    release_bytes = release_name.encode('utf-8')
    
    # Ensure total header is exactly 64 bytes for alignment
    header_size = 64
    header_data_size = len(header_prefix) + len(release_bytes) + 1  # +1 for null terminator
    
    if header_data_size > header_size:
        print(f"Warning: Release name '{release_name}' too long, truncating")
        max_release_len = header_size - len(header_prefix) - 1
        release_bytes = release_bytes[:max_release_len]
        header_data_size = len(header_prefix) + len(release_bytes) + 1
    
    # Create header with padding
    header = header_prefix + release_bytes + b'\0'
    header += b'\xFF' * (header_size - header_data_size)  # Pad with 0xFF (erased flash pattern)
    
    # Read original binary
    with open(binary_file, 'rb') as f:
        original_data = f.read()
    
    # Write header + original binary
    with open(binary_file, 'wb') as f:
        f.write(header)
        f.write(original_data)
    
    print(f"Added WLED metadata header with release name '{release_name}' to {binary_file}")

def create_release(source):
    release_name_def = _get_cpp_define_value(env, "WLED_RELEASE_NAME")
    if release_name_def:
        release_name = release_name_def.replace("\\\"", "")
        version = _get_cpp_define_value(env, "WLED_VERSION")
        release_file = os.path.join(OUTPUT_DIR, "release", f"WLED_{version}_{release_name}.bin")
        release_gz_file = release_file + ".gz"
        print(f"Copying {source} to {release_file}")
        shutil.copy(source, release_file)
        
        # Add metadata header to the release binary
        add_metadata_header(release_file, release_name)
        
        bin_gzip(release_file, release_gz_file)
    else:
        variant = env["PIOENV"]
        bin_file = "{}firmware{}{}.bin".format(OUTPUT_DIR, os.path.sep, variant)
        print(f"Copying {source} to {bin_file}")
        shutil.copy(source, bin_file)

def bin_rename_copy(source, target, env):
    _create_dirs()
    variant = env["PIOENV"]
    builddir = os.path.join(env["PROJECT_BUILD_DIR"],  variant)
    source_map = os.path.join(builddir, env["PROGNAME"] + ".map")

    # create string with location and file names based on variant
    map_file = "{}map{}{}.map".format(OUTPUT_DIR, os.path.sep, variant)

    create_release(str(target[0]))

    # copy firmware.map to map/<variant>.map
    if os.path.isfile("firmware.map"):
        print("Found linker mapfile firmware.map")
        shutil.copy("firmware.map", map_file)
    if os.path.isfile(source_map):
        print(f"Found linker mapfile {source_map}")
        shutil.copy(source_map, map_file)

def bin_gzip(source, target):
    # only create gzip for esp8266
    if not env["PIOPLATFORM"] == "espressif8266":
        return
    
    print(f"Creating gzip file {target} from {source}")
    with open(source,"rb") as fp:
        with gzip.open(target, "wb", compresslevel = 9) as f:
            shutil.copyfileobj(fp, f)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", bin_rename_copy)
