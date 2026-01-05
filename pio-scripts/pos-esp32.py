###############################################################
# This script is modified from the original post_esp32.py script used in ESPEasy/Tasmota
# All credit to the original authors.
##############################################################
# From: https://github.com/letscontrolit/ESPEasy/blob/mega/tools/pio/post_esp32.py
# Thanks TD-er :)

# Thanks @staars for safeboot and auto resizing LittleFS code and enhancements

# Combines separate bin files with their respective offsets into a single file
# This single file must then be flashed to an ESP32 node with 0 offset.
#
# Original implementation: Bartłomiej Zimoń (@uzi18)
#
# Special thanks to @Jason2866 for helping debug flashing to >4MB flash
# Thanks @jesserockz (esphome) for adapting to use esptool.py with merge_bin
#
# Typical layout of the generated file:
#    Offset | File
# -  0x1000 | ~\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32\bin\bootloader_dout_40m.bin
# -  0x8000 | ~\WLED\.pio\build\<env name>\partitions.bin
# -  0xe000 | ~\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin
# - 0x10000 | ~\WLED\.pio\build\<env name>/firmware.bin
# - 0x310000| ~\WLED\.pio\build\<env name>/littlefs.bin

from genericpath import exists
import os
import sys
from os.path import join, getsize
import csv
from urllib.parse import urlparse
import requests
import shutil
import subprocess
import codecs
from pathlib import Path
from colorama import Fore
from SCons.Script import COMMAND_LINE_TARGETS
from platformio.project.config import ProjectConfig

esptoolpy = os.path.join(ProjectConfig.get_instance().get("platformio", "packages_dir"), "tool-esptoolpy")
sys.path.append(esptoolpy)

env = DefaultEnvironment()
platform = env.PioPlatform()
config = env.GetProjectConfig()
variants_dir = env.BoardConfig().get("build.variants_dir", "")
variant = env.BoardConfig().get("build.variant", "")
sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
chip = env.get("BOARD_MCU")
mcu_build_variant = env.BoardConfig().get("build.variant", "").lower()
flag_custom_sdkconfig = config.has_option("env:"+env["PIOENV"], "custom_sdkconfig")
flag_board_sdkconfig = env.BoardConfig().get("espidf.custom_sdkconfig", "")

# Copy safeboots firmwares in place when running in Github
github_actions = os.getenv('GITHUB_ACTIONS')

def normalize_paths(cmd):
    for i, arg in enumerate(cmd):
        if isinstance(arg, str) and '/' in arg:
            cmd[i] = os.path.normpath(arg)
    return cmd

def esp32_detect_flashsize():
    uploader = env.subst("$UPLOADER")
    if not "upload" in COMMAND_LINE_TARGETS:
        return "4MB",False
    if not "esptool" in uploader:
        return "4MB",False
    else:
        esptool_flags = ["flash-id"]
        esptool_cmd = [env["PYTHONEXE"], esptoolpy] + esptool_flags
        try:
            output = subprocess.run(esptool_cmd, capture_output=True).stdout.splitlines()
            for l in output:
                if l.decode().startswith("Detected flash size: "):
                    size = (l.decode().split(": ")[1])
                    print("Did get flash size:", size)
                    stored_flash_size_mb = env.BoardConfig().get("upload.flash_size")
                    stored_flash_size = int(stored_flash_size_mb.split("MB")[0]) * 0x100000
                    detected_flash_size = int(size.split("MB")[0]) * 0x100000
                    if detected_flash_size > stored_flash_size:
                        env.BoardConfig().update("upload.flash_size", size)
                        return size, True
            return "4MB",False
        except subprocess.CalledProcessError as exc:
            print(Fore.YELLOW + "Did get chip info failed with " + str(exc))
            return "4MB",False

flash_size_from_esp, flash_size_was_overridden = esp32_detect_flashsize()

def patch_partitions_bin(size_string):
    partition_bin_path = os.path.normpath(join(env.subst("$BUILD_DIR"), "partitions.bin"))
    with open(partition_bin_path, 'r+b') as file:
        binary_data = file.read(0xb0)
        import hashlib
        bin_list = list(binary_data)
        size_string = int(size_string[2:],16)
        size_string = f"{size_string:08X}"
        size = codecs.decode(size_string, 'hex_codec') # 0xc50000 -> [00,c5,00,00]
        bin_list[0x89] = size[2]
        bin_list[0x8a] = size[1]
        bin_list[0x8b] = size[0]
        result = hashlib.md5(bytes(bin_list[0:0xa0]))
        partition_data = bytes(bin_list) + result.digest()
        file.seek(0)
        file.write(partition_data)
        print("New partition hash:",result.digest().hex())


def esp32_build_filesystem(fs_size):
    try:
        files = env.GetProjectOption("custom_files_upload").splitlines()
    except Exception:
        print()
        print(Fore.RED + "custom_files_upload: NOT DEFINED will NOT create littlefs.bin and NOT overwrite fs partition! ")
        print()
        return False

    if not any(f.strip() for f in files):
        print()
        print(Fore.RED + "custom_files_upload: empty will NOT create littlefs.bin and NOT overwrite fs partition! ")
        print()
        return False

    # files = env.GetProjectOption("custom_files_upload").splitlines()
    num_entries = len([f for f in files if f.strip()])
    filesystem_dir = os.path.normpath(join(env.subst("$BUILD_DIR"), "littlefs_data"))
    if not os.path.exists(filesystem_dir):
        os.makedirs(filesystem_dir)
    if num_entries > 1:
        print()
        print(Fore.GREEN + "Will create filesystem with the following file(s):")
        print()
    for file in files:
        if "no_files" in file:
            print()
            print(Fore.GREEN + "No files added -> will NOT create littlefs.bin and NOT overwrite fs partition!")
            print()
            continue
        if "://" in file:
            parts = file.split()
            url = parts[0]

            response = requests.get(url)
            if response.ok:
                # Extract filename safely from URL
                parsed = urlparse(url)
                filename = os.path.basename(parsed.path)

                # Optional rename support: "<url> <targetname>"
                if len(parts) > 1:
                    print("Renaming", filename, "to", parts[1])
                    filename = parts[1]
                else:
                    print(filename)

                target = os.path.normpath(join(filesystem_dir, filename))
                open(target, "wb").write(response.content)
            else:
                print(Fore.RED + "Failed to download: ", file)
            continue
        if os.path.isdir(file):
            print(f"{file}/ (directory)")
            shutil.copytree(file, filesystem_dir, dirs_exist_ok=True)
        else:
            print(file)
            shutil.copy(file, filesystem_dir)
    if not os.listdir(filesystem_dir):
        #print("No files added -> will NOT create littlefs.bin and NOT overwrite fs partition!")
        return False
    
    tool = env.subst(env["MKFSTOOL"])
    cmd = (tool,"-c",filesystem_dir,"-s",fs_size,join(env.subst("$BUILD_DIR"),"littlefs.bin"))
    returncode = subprocess.call(cmd, shell=False)
    # print(returncode)
    return True


def esp32_create_combined_bin(source, target, env):
    #print("Generating combined binary for serial flashing")
    # The offset from begin of the file where the app0 partition starts
    # This is defined in the partition .csv file
    # factory_offset = -1      # error code value - currently unused
    app_offset = 0x10000     # default value for "old" scheme
    fs_offset = -1           # error code value

    with open(env.BoardConfig().get("build.partitions")) as csv_file:
        print()
        print("Read partitions from ",env.BoardConfig().get("build.partitions"))
        print("--------------------------------------------------------------------")
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        for row in csv_reader:
                # Skip empty, comment, or malformed rows
            if not row or row[0].strip().startswith("#") or len(row) < 5:
                continue
            if line_count == 0:
                print(f'{",  ".join(row)}')
                line_count += 1
            else:
                print(f'{row[0]}   {row[1]}   {row[2]}   {row[3]}   {row[4]}')
                line_count += 1
                if(row[0] == 'app0'):
                    app_offset = int(row[3],base=16)
                # elif(row[0] == 'factory'):
                #     factory_offset = int(row[3],base=16)
                elif(row[0] == 'spiffs'):
                    partition_size = row[4]
                    if flash_size_was_overridden:
                        print(f"Will override fixed FS partition size from {env.BoardConfig().get('build.partitions')}: {partition_size} ...")
                        partition_size =  hex(int(flash_size_from_esp.split("MB")[0]) * 0x100000 - int(row[3],base=16))
                        print(f"... with computed maximum size from connected {env.get('BOARD_MCU')}: {partition_size}")
                        patch_partitions_bin(partition_size)
                    if esp32_build_filesystem(partition_size):
                        fs_offset = int(row[3],base=16)

    print()
    new_file_name = os.path.normpath(env.subst("$BUILD_DIR/${PROGNAME}.factory.bin"))
    firmware_name = os.path.normpath(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    s_flag = True


    if s_flag:  # check if safeboot firmware is existing
        flash_size = env.BoardConfig().get("upload.flash_size", "4MB")
        flash_mode = env["__get_board_flash_mode"](env)
        flash_freq = env["__get_board_f_flash"](env)

        cmd = [
            "--chip",
            chip,
            "merge_bin",
            "-o",
            new_file_name,
            "--flash_mode",
            flash_mode,
            "--flash_freq",
            flash_freq,
            "--flash_size",
            flash_size,
        ]
        # platformio estimates the flash space used to store the firmware.
        # the estimation is inaccurate. perform a final check on the firmware
        # size by comparing it against the partition size.
        max_size = env.BoardConfig().get("upload.maximum_size", 1)
        fw_size = getsize(firmware_name)
        if (fw_size > max_size):
            raise Exception(Fore.RED + "firmware binary too large: %d > %d" % (fw_size, max_size))

        print()
        print("    Offset   | File")
        for section in sections:
            sect_adr, sect_file = section.split(" ", 1)
            print(f" -  {sect_adr.ljust(8)} | {sect_file}")
            cmd += [sect_adr, sect_file]

        # "main" firmware to app0 - mandatory, except we just built a new safeboot bin locally
        if ("safeboot" not in firmware_name):
            print(f" -  {hex(app_offset).ljust(8)} | {firmware_name}")
            cmd += [hex(app_offset), firmware_name]

        else:
            print()
            print(Fore.GREEN + "Upload new safeboot binary only")

        upload_protocol = env.subst("$UPLOAD_PROTOCOL")
        if(upload_protocol == "esptool") and (fs_offset != -1):
            fs_bin = os.path.normpath(join(env.subst("$BUILD_DIR"), "littlefs.bin"))
            if exists(fs_bin):
                before_reset = env.BoardConfig().get("upload.before_reset", "default_reset")
                after_reset = env.BoardConfig().get("upload.after_reset", "hard_reset")
                print(f" -  {hex(fs_offset).ljust(8)} | {fs_bin}")
                print()
                cmd += [hex(fs_offset), fs_bin]
                env.Replace(
                UPLOADERFLAGS=[
                "--chip", chip,
                "--port", '"$UPLOAD_PORT"',
                "--baud", "$UPLOAD_SPEED",
                "--before", before_reset,
                "--after", after_reset,
                "write_flash", "-z",
                "--flash_mode", "${__get_board_flash_mode(__env__)}",
                "--flash_freq", "${__get_board_f_flash(__env__)}",
                "--flash_size", flash_size
                ],
                UPLOADCMD='"$PYTHONEXE" "$UPLOADER" $UPLOADERFLAGS ' + " ".join(cmd[7:])
                )
                print(Fore.GREEN + "Will use custom upload command for flashing operation to add file system defined for this build target.")
                print()

        if("safeboot" not in firmware_name):
            cmdline = [env.subst("$PYTHONEXE")] + [env.subst("$OBJCOPY")] + normalize_paths(cmd)
            # print('Command Line: %s' % cmdline)
            result = subprocess.run(cmdline, text=True, check=False, stdout=subprocess.DEVNULL)
            if result.returncode != 0:
                print(Fore.RED + f"esptool create firmware failed with exit code: {result.returncode}")

silent_action = env.Action(esp32_create_combined_bin)
silent_action.strfunction = lambda target, source, env: '' # hack to silence scons command output
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", silent_action)