# M5Stack CoreS3 upload reset stabilization
#
# Purpose:
#   ESP32-S3 native USB / USB-Serial-JTAG uploads can occasionally remain
#   in download mode after PlatformIO's default RTS hard reset.
#
# PlatformIO's ESP32 builder constructs esptool's --after argument from:
#   board.get("upload.after_reset", "hard_reset")
#
# Set that board option early, before the platform builder creates
# UPLOADERFLAGS. This changes only the post-upload reset method.
#
# Expected esptool behavior:
#   --after watchdog-reset
#
# No firmware source, build flag, partition, PSRAM, flash, or runtime
# behavior is modified by this script.

Import("env")

TARGET_RESET_METHOD = "watchdog-reset"


def configure_upload_reset():
    board = env.BoardConfig()
    previous = board.get("upload.after_reset", "hard_reset")

    if previous == TARGET_RESET_METHOD:
        print(
            "[CoreS3 Upload] post-upload reset already set to "
            f"{TARGET_RESET_METHOD}"
        )
        return

    board.update("upload.after_reset", TARGET_RESET_METHOD)

    effective = board.get("upload.after_reset", "")
    if effective != TARGET_RESET_METHOD:
        raise RuntimeError(
            "CoreS3 upload reset configuration failed: "
            f"expected {TARGET_RESET_METHOD}, got {effective}"
        )

    print(
        "[CoreS3 Upload] post-upload reset: "
        f"{previous} -> {TARGET_RESET_METHOD}"
    )


if not env.IsIntegrationDump():
    configure_upload_reset()
