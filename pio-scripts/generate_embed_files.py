Import("env")
from pathlib import Path

# ── generate_embed_files.py ───────────────────────────────────────────────────
# Pre-build script for the esp32s3_matter_wifi environment.
#
# Generates .S assembly files for binary data that ESP-IDF components
# expect to be embedded via CMake's target_add_binary_data().
# PlatformIO's SCons build doesn't execute those CMake commands,
# so we replicate the output here.
# ─────────────────────────────────────────────────────────────────────────────

project_dir = Path(env["PROJECT_DIR"]).resolve()
build_dir   = Path(env.subst("$BUILD_DIR")).resolve()

# Map of (source cert path, symbol name) pairs that need embedding.
# These correspond to all target_add_binary_data() calls in the managed
# components' CMakeLists.txt files (which PlatformIO's SCons build skips).
EMBED_FILES = [
    (
        project_dir / "managed_components" / "espressif__esp_insights" / "server_certs" / "https_server.crt",
        "https_server_crt",
    ),
    (
        project_dir / "managed_components" / "espressif__esp_insights" / "server_certs" / "mqtt_server.crt",
        "mqtt_server_crt",
    ),
    (
        project_dir / "managed_components" / "espressif__esp_rainmaker" / "server_certs" / "rmaker_mqtt_server.crt",
        "rmaker_mqtt_server_crt",
    ),
    (
        project_dir / "managed_components" / "espressif__esp_rainmaker" / "server_certs" / "rmaker_claim_service_server.crt",
        "rmaker_claim_service_server_crt",
    ),
    (
        project_dir / "managed_components" / "espressif__esp_rainmaker" / "server_certs" / "rmaker_ota_server.crt",
        "rmaker_ota_server_crt",
    ),
]


def generate_asm(src_path: Path, symbol: str, out_dir: Path):
    """Generate an assembly .S file that embeds binary data, matching the
    format produced by ESP-IDF's ``target_add_binary_data()``."""
    if not src_path.exists():
        print(f"  [embed] WARNING: {src_path} not found – skipping")
        return

    out_file = out_dir / f"{src_path.name}.S"
    data = src_path.read_bytes()

    lines = [
        f"/* Data converted from {src_path} */",
        ".data",
        "#if !defined (__APPLE__) && !defined (__linux__)",
        ".section .rodata.embedded",
        "#endif",
        "",
        f".global {symbol}",
        f"{symbol}:",
        "",
        f".global _binary_{symbol}_start",
        f"_binary_{symbol}_start: /* for objcopy compatibility */",
    ]

    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        hexvals = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f".byte {hexvals}")

    lines += [
        "",
        f".global _binary_{symbol}_end",
        f"_binary_{symbol}_end: /* for objcopy compatibility */",
        ".byte 0x00",  # null terminator for TEXT mode
    ]

    out_dir.mkdir(parents=True, exist_ok=True)
    out_file.write_text("\n".join(lines) + "\n")
    print(f"  [embed] Generated {out_file} ({len(data)} bytes)")


for src_path, symbol in EMBED_FILES:
    generate_asm(src_path, symbol, build_dir)

