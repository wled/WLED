#!/usr/bin/env python3
"""
WLED OTA Bootloader Regression Suite (asset-based)
===================================================
Tests every WLED release tag to determine which historic bootloaders can
boot a v16.x (ESP32-IDF V4) app image — the known regression where old
bootloaders cannot parse the new IDF V4 app image header.

Approach
--------
For each old tag:
  * App firmware:    downloaded from the GitHub release page
                     (asset `WLED_<version>_ESP32.bin`).
  * Bootloader:      built once per unique espressif32 platform version
                     by compiling a minimal Arduino sketch inside a
                     linux/amd64 Docker container.  The bootloader is
                     extracted from the SDK / build output.
  * Partition table: generated once per (platform, partition CSV) combo
                     using the same Docker container (the CSV content is
                     read from the tag with `git show <tag>:<csv>`).

The new IDF V4 app image is built once from the current tree and reused.

Per-tag cycle:
  1. Resolve plan: platform version + partition CSV from `git show <tag>:platformio.ini`.
  2. Download `WLED_<ver>_ESP32.bin` from the GitHub release.
  3. Build (or load from cache) bootloader.bin + partitions.bin for this
     (platform, csv) combination via Docker.
  4. Erase device.
  5. Flash bootloader + partition table + old app via serial.
  6. Read serial; confirm old WLED boots.
  7. Flash new IDF V4 app to the app0 offset; erase otadata.
  8. Read serial; classify.

Requirements
------------
  pip install pyserial esptool
  docker  (with linux/amd64 QEMU binfmt support on ARM hosts)
  git, gh (GitHub CLI authenticated)

Usage
-----
  python3 tools/ota_bootloader_regression.py --port /dev/ttyACM1
  python3 tools/ota_bootloader_regression.py --port /dev/ttyACM1 --stable-only
  python3 tools/ota_bootloader_regression.py --port /dev/ttyACM1 \\
      --tags v0.14.4 v0.15.5
"""

import argparse
import configparser
import csv
import enum
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

try:
    import serial
except ImportError:
    print("ERROR: pyserial is required.  Run: pip install pyserial")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

DOCKER_IMAGE_TAG = "wled-ota-test-builder:latest"
PIO_CACHE_VOLUME = "wled-ota-test-pio-cache"

DOCKERFILE = """\
FROM --platform=linux/amd64 python:3.11
RUN apt-get update -qq \\
 && apt-get install -y --no-install-recommends git \\
 && pip install --quiet platformio \\
 && apt-get clean \\
 && rm -rf /var/lib/apt/lists/*
"""

DEFAULT_NEW_TAG       = "v16.0.0-beta"
DEFAULT_ENV           = "esp32dev"
DEFAULT_GH_REPO       = "Aircoookie/WLED"
SERIAL_BAUD           = 115200
BOOT_OBSERVE_SECONDS  = 30

RELEASE_TAG_RE = re.compile(r"^v?\d+\.\d+")
PRERELEASE_RE  = re.compile(r"[-.](?:b\d+|beta|rc\d*|alpha)", re.IGNORECASE)

# Hints that the firmware appears WLED-ish (used only for nicer notes).
WLED_HINT_PATTERNS = [
    re.compile(rb"WLED\b"),
    re.compile(rb"Starting WLED", re.IGNORECASE),
    re.compile(rb"[Ss]etup\(\)"),
    re.compile(rb"Ada\r?\n"),
]
# Bootloader handed control to the application image.
ENTRY_RE = re.compile(rb"entry 0x[0-9a-fA-F]+")
# Bootloader / early-boot fatal error markers.
BOOTLOADER_ERROR_PATTERNS = [
    re.compile(rb"invalid header"),
    re.compile(rb"image header magic"),
    re.compile(rb"bad magic"),
    re.compile(rb"checksum failed", re.IGNORECASE),
    re.compile(rb"flash read err", re.IGNORECASE),
    re.compile(rb"unsupported chip rev", re.IGNORECASE),
    re.compile(rb"fatal exception", re.IGNORECASE),
    re.compile(rb"Guru Meditation", re.IGNORECASE),
    re.compile(rb"abort\(\) was called"),
    re.compile(rb"assert failed"),
    re.compile(rb"CORRUPT HEAP", re.IGNORECASE),
]
# Reset reasons indicating an unhealthy reboot loop (everything except a
# clean cold start).
BAD_RESET_RE = re.compile(
    rb"rst:0x[0-9a-fA-F]+\s*\((?:SW_CPU_RESET|RTCWDT_RTC_RESET|"
    rb"TG\d+WDT_(?:SYS|CPU)_RESET|RTCWDT_BROWN_OUT_RESET|"
    rb"RTCWDT_SYS_RESET|TASK_WDT|INT_WDT|PANIC)",
    re.IGNORECASE,
)
RESET_REASON_RE = re.compile(rb"rst:0x[0-9a-fA-F]+")

REPO_ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Result types
# ---------------------------------------------------------------------------

class Result(enum.Enum):
    PASS  = "PASS"
    FAIL  = "FAIL"
    SKIP  = "SKIP"
    ERROR = "ERROR"


@dataclass
class TagResult:
    tag:    str
    result: Result
    notes:  str = ""


@dataclass
class TagPlan:
    tag:           str
    platform_ver:  str          # e.g. "3.5.0", "2024.06.10", or "" if unpinned
    csv_text:      str          # raw CSV file contents
    csv_name:      str          # filename for display
    asset_name:    str          # GitHub release asset name


@dataclass
class PartitionLayout:
    app0_offset:    int
    otadata_offset: int
    otadata_size:   int


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

class StepError(Exception):
    """Hard failure in test infrastructure."""


def _run(cmd: list, *, cwd: Path = REPO_ROOT, label: str = "",
         timeout: int = 1800, capture: bool = False) -> Optional[str]:
    display = label or " ".join(str(c) for c in cmd)
    print(f"    $ {display}")
    if capture:
        r = subprocess.run(cmd, cwd=cwd, timeout=timeout,
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(r.stderr)
            raise StepError(f"Command failed (exit {r.returncode}): {display}")
        return r.stdout
    r = subprocess.run(cmd, cwd=cwd, timeout=timeout)
    if r.returncode != 0:
        raise StepError(f"Command failed (exit {r.returncode}): {display}")
    return None


def _section(title: str) -> None:
    print(f"\n{'─' * 60}\n{title}\n{'─' * 60}")


def _git_show(tag: str, path: str) -> str:
    """Read file content from a tag without checking out."""
    try:
        return subprocess.check_output(
            ["git", "show", f"{tag}:{path}"],
            cwd=REPO_ROOT, text=True, stderr=subprocess.PIPE,
        )
    except subprocess.CalledProcessError as exc:
        raise StepError(f"git show {tag}:{path} failed: {exc.stderr.strip()}")


# ---------------------------------------------------------------------------
# Docker image management
# ---------------------------------------------------------------------------

def _image_exists() -> bool:
    return subprocess.run(
        ["docker", "image", "inspect", DOCKER_IMAGE_TAG],
        capture_output=True,
    ).returncode == 0


def _build_docker_image(force: bool = False) -> None:
    if not force and _image_exists():
        print(f"  Builder image {DOCKER_IMAGE_TAG} already exists.")
        return
    print(f"  Building Docker builder image: {DOCKER_IMAGE_TAG} …")
    with tempfile.TemporaryDirectory() as ctx:
        (Path(ctx) / "Dockerfile").write_text(DOCKERFILE)
        _run(["docker", "build", "--platform", "linux/amd64",
              "-t", DOCKER_IMAGE_TAG, ctx],
             label=f"docker build -t {DOCKER_IMAGE_TAG}", timeout=600)


# ---------------------------------------------------------------------------
# platformio.ini parsing (against in-memory text)
# ---------------------------------------------------------------------------

def _parse_ini(text: str) -> configparser.ConfigParser:
    cfg = configparser.ConfigParser(interpolation=None, strict=False)
    cfg.read_file(io.StringIO(text))
    return cfg


def _resolve_value(cfg: configparser.ConfigParser, env: str, key: str) -> Optional[str]:
    """Get key from [env:NAME] with simple ${section.key} interpolation."""
    sections = [f"env:{env}", "env", "common"]
    raw = None
    for s in sections:
        if cfg.has_section(s) and cfg.has_option(s, key):
            raw = cfg.get(s, key)
            break
    if raw is None:
        return None
    for _ in range(8):
        m = re.search(r"\$\{([^.}]+)\.([^}]+)\}", raw)
        if not m:
            break
        sec, k = m.group(1), m.group(2)
        if not cfg.has_section(sec) or not cfg.has_option(sec, k):
            break
        raw = raw.replace(m.group(0), cfg.get(sec, k))
    return raw.split(";", 1)[0].strip()


def _extract_platform_version(platform_str: str) -> str:
    """
    Given e.g. 'espressif32@3.5.0', 'espressif32 @ 3.5.0',
    'https://github.com/.../#release/v6.5.0', 'tasmota/platform-espressif32 @ 2024.06.10',
    return a string that uniquely identifies the platform package.
    Empty string ⇒ unpinned ⇒ caller picks default.
    """
    s = platform_str.strip()
    # Pin via @ version
    m = re.search(r"@\s*([A-Za-z0-9._\-+]+)", s)
    if m:
        return m.group(1)
    # URL with #ref
    m = re.search(r"#([A-Za-z0-9._\-/]+)", s)
    if m:
        return m.group(1)
    return ""


def _resolve_plan(tag: str, env: str, repo: str) -> TagPlan:
    """Build a TagPlan from git-show of platformio.ini at the tag + GH release."""
    ini_text = _git_show(tag, "platformio.ini")
    cfg = _parse_ini(ini_text)

    section = f"env:{env}"
    if not cfg.has_section(section):
        raise StepError(f"No [env:{env}] in {tag}'s platformio.ini")

    platform_raw = _resolve_value(cfg, env, "platform") or ""
    platform_ver = _extract_platform_version(platform_raw)

    csv_value = _resolve_value(cfg, env, "board_build.partitions")
    if csv_value:
        csv_text = _git_show(tag, csv_value)
        csv_name = Path(csv_value).name
    else:
        # Fall back to the espressif32 board's default partition table.
        # For board=esp32dev (and most 4MB ESP32 boards) PlatformIO uses
        # `default.csv` at this layout, identical to what we already
        # support (otadata @ 0xe000, app0 @ 0x10000).
        csv_text = (
            "# Name,   Type, SubType, Offset,  Size, Flags\n"
            "nvs,      data, nvs,     0x9000,  0x5000,\n"
            "otadata,  data, ota,     0xe000,  0x2000,\n"
            "app0,     app,  ota_0,   0x10000, 0x140000,\n"
            "app1,     app,  ota_1,   0x150000,0x140000,\n"
            "spiffs,   data, spiffs,  0x290000,0x170000,\n"
        )
        csv_name = "default.csv"

    # Determine asset
    ver_str = tag.lstrip("v")
    asset_name = f"WLED_{ver_str}_ESP32.bin"

    # Verify asset exists; if not, raise (caller maps to SKIP)
    avail = _list_release_assets(tag, repo)
    if asset_name not in avail:
        # try lenient: any asset matching ESP32 plain pattern
        candidates = [a for a in avail
                      if re.fullmatch(rf"WLED_{re.escape(ver_str)}_ESP32\.bin", a)]
        if candidates:
            asset_name = candidates[0]
        else:
            raise StepError(
                f"GitHub release {tag} has no '{asset_name}' "
                f"(assets: {avail or 'none'})"
            )

    return TagPlan(
        tag=tag, platform_ver=platform_ver,
        csv_text=csv_text, csv_name=csv_name,
        asset_name=asset_name,
    )


# ---------------------------------------------------------------------------
# Partition CSV parsing
# ---------------------------------------------------------------------------

def _parse_offset(s: str) -> int:
    s = s.strip()
    if not s:
        return 0
    if s[-1].lower() == "k":
        return int(s[:-1], 0) * 1024
    if s[-1].lower() == "m":
        return int(s[:-1], 0) * 1024 * 1024
    return int(s, 0)


def _parse_partition_layout(csv_text: str) -> PartitionLayout:
    app0 = otadata = otadata_sz = None
    for row in csv.reader(io.StringIO(csv_text)):
        row = [c.strip() for c in row]
        if not row or row[0].startswith("#") or len(row) < 5:
            continue
        _, ptype, subtype, offset, size = row[0], row[1], row[2], row[3], row[4]
        if ptype == "app" and app0 is None:
            app0 = _parse_offset(offset)
        if ptype == "data" and subtype == "ota":
            otadata    = _parse_offset(offset)
            otadata_sz = _parse_offset(size)
    if app0 is None:
        raise StepError("No app partition in CSV")
    return PartitionLayout(app0, otadata or 0, otadata_sz or 0)


# ---------------------------------------------------------------------------
# GitHub release asset handling
# ---------------------------------------------------------------------------

_assets_cache: dict[str, list[str]] = {}


def _list_release_assets(tag: str, repo: str) -> list[str]:
    if tag in _assets_cache:
        return _assets_cache[tag]
    r = subprocess.run(
        ["gh", "release", "view", tag, "--repo", repo,
         "--json", "assets", "-q", ".assets[].name"],
        capture_output=True, text=True, timeout=30,
    )
    if r.returncode != 0:
        _assets_cache[tag] = []
        return []
    names = [ln.strip() for ln in r.stdout.splitlines() if ln.strip()]
    _assets_cache[tag] = names
    return names


def _download_asset(tag: str, asset: str, dest_dir: Path, repo: str) -> Path:
    dest_dir.mkdir(parents=True, exist_ok=True)
    out = dest_dir / asset
    if out.exists() and out.stat().st_size > 0:
        print(f"    cached: {out}")
        return out
    _run(["gh", "release", "download", tag, "--repo", repo,
          "--pattern", asset, "--dir", str(dest_dir),
          "--clobber"],
         label=f"gh release download {tag} {asset}", timeout=120)
    if not out.exists():
        raise StepError(f"Asset download failed: {out}")
    print(f"    downloaded: {out} ({out.stat().st_size:,} bytes)")
    return out


# ---------------------------------------------------------------------------
# Bootloader + partition build (per platform_ver, csv pair)
# ---------------------------------------------------------------------------

# Mapping of WLED platform tokens we may see → an actual platformio platform
# spec usable in a fresh Docker build of a hello-world sketch.
# WLED uses the `tasmota/platform-espressif32` fork from ~v0.15 onwards;
# falling back to upstream `espressif32` is fine when the version exists.
def _normalise_platform(spec_or_ver: str) -> str:
    s = spec_or_ver.strip()
    if not s:
        # Pick a recent stable upstream version as default
        return "espressif32@6.7.0"
    # Already a full PIO spec (contains @ or http or /)
    if "@" in s or s.startswith("http") or "/" in s:
        return s
    # Tasmota-fork dated tag like 2024.06.10 or 2025.03.30
    if re.fullmatch(r"\d{4}\.\d{2}\.\d{2}", s):
        return f"https://github.com/tasmota/platform-espressif32.git#{s}"
    # Otherwise assume upstream
    return f"espressif32@{s}"


def _platform_cache_key(platform_spec: str, csv_text: str) -> str:
    h = hashlib.sha256()
    h.update(platform_spec.encode())
    h.update(b"\0")
    h.update(csv_text.encode())
    return h.hexdigest()[:16]


def _build_bootloader_and_partitions(
    platform_spec: str, csv_text: str, cache_dir: Path,
) -> tuple[Path, Path]:
    """
    Build a minimal Arduino hello-world for esp32dev with the given platform
    version and partition CSV.  Returns (bootloader.bin, partitions.bin).
    Cached by hash of (platform_spec, csv_text).
    """
    key       = _platform_cache_key(platform_spec, csv_text)
    out_dir   = cache_dir / key
    boot_bin  = out_dir / "bootloader.bin"
    parts_bin = out_dir / "partitions.bin"
    if boot_bin.exists() and parts_bin.exists():
        print(f"    cache hit ({key}): {out_dir}")
        return boot_bin, parts_bin

    out_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="wled_bl_build_") as proj:
        proj_path = Path(proj)
        (proj_path / "src").mkdir()
        (proj_path / "src" / "main.cpp").write_text(
            "#include <Arduino.h>\n"
            "void setup(){ Serial.begin(115200); }\n"
            "void loop(){ delay(1000); }\n"
        )
        (proj_path / "partitions.csv").write_text(csv_text)
        (proj_path / "platformio.ini").write_text(
            f"[env:e32]\n"
            f"platform = {platform_spec}\n"
            f"board    = esp32dev\n"
            f"framework = arduino\n"
            f"board_build.partitions = partitions.csv\n"
        )

        uid, gid = os.getuid(), os.getgid()
        build_sh = (
            "set -e\n"
            f"trap 'chown -R {uid}:{gid} .pio 2>/dev/null || true' EXIT\n"
            "pio run -e e32\n"
            "B=.pio/build/e32/bootloader.bin\n"
            "if [ ! -f \"$B\" ]; then\n"
            "  CAND=$(find /root/.platformio/packages/framework-arduinoespressif32* "
            "-name 'bootloader_dio_40m.bin' 2>/dev/null | head -1)\n"
            "  if [ -n \"$CAND\" ]; then cp \"$CAND\" \"$B\"; fi\n"
            "fi\n"
        )
        _run(
            ["docker", "run", "--rm",
             "--platform", "linux/amd64",
             "-v", f"{proj_path}:/build",
             "-v", f"{PIO_CACHE_VOLUME}:/root/.platformio",
             "-w", "/build",
             DOCKER_IMAGE_TAG,
             "bash", "-c", build_sh],
            label=f"docker build hello-world ({platform_spec})",
            timeout=900,
        )

        bb = proj_path / ".pio" / "build" / "e32" / "bootloader.bin"
        pp = proj_path / ".pio" / "build" / "e32" / "partitions.bin"
        if not bb.exists():
            raise StepError(f"hello-world build did not produce bootloader.bin "
                            f"for platform {platform_spec}")
        if not pp.exists():
            raise StepError(f"hello-world build did not produce partitions.bin "
                            f"for platform {platform_spec}")
        shutil.copy2(bb, boot_bin)
        shutil.copy2(pp, parts_bin)

    print(f"    cached → {out_dir}")
    return boot_bin, parts_bin


# ---------------------------------------------------------------------------
# New-app build (current tree, IDF V4)
# ---------------------------------------------------------------------------

def _build_new_app(env: str, work_dir: Path) -> Path:
    """
    Build the current tree's firmware for *env* via Docker.  Returns path to
    the resulting firmware.bin.  Web-UI assets (html_*.h) are pre-built on
    the host so Node.js never runs in the QEMU-emulated container.
    """
    # Mirror the current tree into work_dir as a worktree.
    if not (work_dir / ".git").exists():
        if work_dir.exists():
            shutil.rmtree(work_dir, ignore_errors=True)
        _run(["git", "worktree", "add", "--detach", str(work_dir), "HEAD"],
             label=f"git worktree add {work_dir}", timeout=60)

    # Build web UI on host (native).
    if (work_dir / "package.json").exists():
        _run(["npm", "ci"], cwd=work_dir, label="npm ci (host)", timeout=180)
        _run(["npm", "run", "build"], cwd=work_dir,
             label="npm run build (host)", timeout=180)

    uid, gid = os.getuid(), os.getgid()
    build_sh = (
        "set -e\n"
        f"trap 'chown -R {uid}:{gid} .pio 2>/dev/null || true' EXIT\n"
        f"pio run -e {env}\n"
    )
    _run(["docker", "run", "--rm",
          "--platform", "linux/amd64",
          "-v", f"{work_dir}:/build",
          "-v", f"{PIO_CACHE_VOLUME}:/root/.platformio",
          "-w", "/build",
          DOCKER_IMAGE_TAG,
          "bash", "-c", build_sh],
         label=f"docker build new-app ({env})", timeout=1800)

    bin_path = work_dir / ".pio" / "build" / env / "firmware.bin"
    if not bin_path.exists():
        raise StepError(f"firmware.bin not found: {bin_path}")
    return bin_path


# ---------------------------------------------------------------------------
# esptool / serial wrappers
# ---------------------------------------------------------------------------

def _esptool(*args: str, timeout: int = 300, label: str = "") -> None:
    cmd = [sys.executable, "-m", "esptool"] + list(args)
    _run(cmd, label=label or "esptool " + " ".join(args), timeout=timeout)


def _read_serial(port: str, seconds: int, label: str,
                 reset_first: bool = False) -> bytes:
    """Read up to `seconds` of serial. Always reads the full window so the
    classifier can detect boot loops. We deliberately do NOT early-break on
    a banner pattern, because that would hide reboots that occur after a
    seemingly-successful first print."""
    bar = "──── serial[%s] ────" % label
    print(f"    Serial {port}  {seconds}s  [{label}] …")
    print(f"    {bar}")
    buf = bytearray()
    deadline = time.monotonic() + seconds
    try:
        with serial.Serial(port, SERIAL_BAUD, timeout=0.5,
                           dsrdtr=False, rtscts=False) as ser:
            ser.dtr = False
            ser.rts = False
            if reset_first:
                ser.rts = True
                time.sleep(0.1)
                ser.rts = False
                time.sleep(0.05)
            while time.monotonic() < deadline:
                chunk = ser.read(4096)
                if chunk:
                    buf.extend(chunk)
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
    except serial.SerialException as exc:
        raise StepError(f"Serial error on {port}: {exc}")
    sys.stdout.write(f"\n    {'─' * len(bar)}\n")
    return bytes(buf)


def _classify(out: bytes) -> tuple[bool, str]:
    """Classify a serial capture window.

    PASS = chip booted, app handed control, no fatal errors, not in a reset
    loop. We do NOT require any specific banner string, because older WLED
    releases may not implement Adalight or print a recognisable banner.
    FAIL = bootloader-level error, or repeated unhealthy resets, or app was
    never reached.
    """
    errors = [p.pattern.decode() for p in BOOTLOADER_ERROR_PATTERNS
              if p.search(out)]
    if errors:
        return False, f"bootloader/app errors: {errors}"

    bad_resets = BAD_RESET_RE.findall(out)
    if len(bad_resets) >= 2:
        return False, f"boot loop ({len(bad_resets)} unhealthy resets)"

    all_resets = RESET_REASON_RE.findall(out)
    entries    = ENTRY_RE.findall(out)

    # Multiple `entry 0x...` lines mean the bootloader keeps re-handing
    # control to the app, i.e. the app crashes and reboots.
    if len(entries) >= 2:
        return False, f"boot loop ({len(entries)} entry hand-offs)"

    if not out.strip():
        return False, "no serial output (port issue?)"

    if not entries:
        # Bootloader never reached `entry` — image rejected or stuck.
        return False, "bootloader did not reach app entry"

    # App entry reached, no errors, no loop. Note whether the output looks
    # WLED-ish for the human-readable summary.
    if any(p.search(out) for p in WLED_HINT_PATTERNS):
        note = "app booted (WLED markers seen)"
    else:
        note = "app booted (no obvious crash, no WLED markers)"

    # If we saw multiple total resets but only one `entry`, it usually means
    # the chip was reset (e.g. by us) before the app started. Tolerate up
    # to 2 resets; flag more.
    if len(all_resets) > 2:
        note += f" [warning: {len(all_resets)} reset events]"
    return True, note


# ---------------------------------------------------------------------------
# Tag discovery
# ---------------------------------------------------------------------------

def _discover_tags(new_tag: str, stable_only: bool) -> list[str]:
    raw = subprocess.check_output(
        ["git", "tag", "--sort=version:refname"],
        cwd=REPO_ROOT, text=True,
    ).splitlines()
    tags = [t for t in raw if RELEASE_TAG_RE.match(t)]
    if stable_only:
        tags = [t for t in tags if not PRERELEASE_RE.search(t)]
    if new_tag in tags:
        tags = tags[:tags.index(new_tag)]
    return tags


# ---------------------------------------------------------------------------
# Per-tag execution
# ---------------------------------------------------------------------------

def _run_one(plan: TagPlan, layout: PartitionLayout,
             boot_bin: Path, parts_bin: Path, app_bin: Path,
             new_app_bin: Path, port: str, chip: str,
             boot_observe: int) -> TagResult:
    tag = plan.tag
    print(f"\n{'=' * 60}\n  TAG: {tag}\n{'=' * 60}")
    print(f"    platform: {plan.platform_ver or '(unpinned)'}")
    print(f"    csv:      {plan.csv_name}")
    print(f"    asset:    {plan.asset_name}")
    print(f"    app0:     0x{layout.app0_offset:X}")
    print(f"    otadata:  0x{layout.otadata_offset:X} size 0x{layout.otadata_size:X}")

    # Erase
    _section(f"[{tag}] Erase device flash")
    try:
        _esptool("--chip", chip, "--port", port, "erase_flash",
                 label="esptool erase_flash", timeout=120)
    except Exception as exc:
        return TagResult(tag, Result.ERROR, f"erase failed: {exc}")

    # Flash bootloader + partition table + old app
    _section(f"[{tag}] Flash old bootloader + partitions + old app")
    try:
        _esptool("--chip", chip, "--port", port,
                 "--before", "default_reset", "--after", "no_reset",
                 "write_flash",
                 "--flash_mode", "dio", "--flash_freq", "40m",
                 "--flash_size", "4MB",
                 "0x1000", str(boot_bin),
                 "0x8000", str(parts_bin),
                 f"0x{layout.app0_offset:X}", str(app_bin),
                 label="esptool write_flash (boot+parts+old app)",
                 timeout=300)
    except Exception as exc:
        return TagResult(tag, Result.ERROR, f"old-flash failed: {exc}")

    # Verify old firmware boots
    _section(f"[{tag}] Verify old firmware boots")
    out = _read_serial(port, boot_observe, f"old fw {tag}", reset_first=True)
    booted, why = _classify(out)
    if not booted:
        return TagResult(tag, Result.SKIP,
                         f"old fw did not boot (cannot test OTA): {why}")

    # Flash new app to app0; erase otadata
    _section(f"[{tag}] Flash NEW app to app0 (bootloader untouched)")
    try:
        _esptool("--chip", chip, "--port", port,
                 "--before", "default_reset", "--after", "no_reset",
                 "write_flash", f"0x{layout.app0_offset:X}", str(new_app_bin),
                 label=f"esptool write_flash 0x{layout.app0_offset:X} (new app)",
                 timeout=300)
        if layout.otadata_size:
            _esptool("--chip", chip, "--port", port,
                     "--before", "no_reset", "--after", "no_reset",
                     "erase_region",
                     f"0x{layout.otadata_offset:X}",
                     f"0x{layout.otadata_size:X}",
                     label="esptool erase_region otadata", timeout=60)
    except Exception as exc:
        return TagResult(tag, Result.ERROR, f"new-app flash failed: {exc}")

    out = _read_serial(port, boot_observe, "new fw post-flash",
                       reset_first=True)
    booted, why = _classify(out)
    if booted:
        return TagResult(tag, Result.PASS, why)
    return TagResult(tag, Result.FAIL,
                     f"old bootloader rejected new app: {why}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="WLED OTA bootloader regression suite (asset-based)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("--port", default=None,
                   help="Serial port (e.g. /dev/ttyACM0). "
                        "Not required with --pre-build-only.")
    p.add_argument("--new-tag", default=DEFAULT_NEW_TAG,
                   help=f"Reserved for future use; the new app is built from "
                        f"the current tree (default new-tag label: {DEFAULT_NEW_TAG})")
    p.add_argument("--env", default=DEFAULT_ENV,
                   help=f"PlatformIO environment (default: {DEFAULT_ENV})")
    p.add_argument("--chip", default="esp32",
                   help="esptool chip type (default: esp32)")
    p.add_argument("--repo", default=DEFAULT_GH_REPO,
                   help=f"GitHub repo for releases (default: {DEFAULT_GH_REPO})")
    p.add_argument("--boot-observe", type=int, default=BOOT_OBSERVE_SECONDS,
                   help=f"Serial read seconds per boot check (default: {BOOT_OBSERVE_SECONDS})")
    p.add_argument("--tags", nargs="+", metavar="TAG",
                   help="Explicit old tags to test (skips auto-discovery)")
    p.add_argument("--stable-only", action="store_true",
                   help="Auto-discovery: skip -beta/-rc/-bN tags")
    p.add_argument("--work-dir",
                   help="Persistent work dir for caches & worktree (default: temp)")
    p.add_argument("--keep-work-dir", action="store_true",
                   help="Keep working directory after the run")
    p.add_argument("--stop-on-fail", action="store_true",
                   help="Stop after the first FAIL")
    p.add_argument("--rebuild-image", action="store_true",
                   help="Force rebuild of Docker builder image")
    p.add_argument("--pre-build-only", action="store_true",
                   help="Resolve plans, fetch assets, build bootloaders & new app; "
                        "skip all hardware steps")
    p.add_argument("--reuse-new-app", action="store_true",
                   help="Reuse cached new-app firmware.bin if present")
    return p.parse_args()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    # Force line-buffered stdout so structured `print()` headers stay in
    # order with the binary serial bytes we write via sys.stdout.buffer.
    try:
        sys.stdout.reconfigure(line_buffering=True)  # type: ignore[attr-defined]
    except (AttributeError, io.UnsupportedOperation):
        pass

    args = _parse_args()

    # Tool sanity
    for tool, probe in [
        ("docker",  ["docker", "info"]),
        ("esptool", [sys.executable, "-m", "esptool", "version"]),
        ("gh",      ["gh", "--version"]),
    ]:
        if subprocess.run(probe, capture_output=True, timeout=15).returncode != 0:
            print(f"ERROR: {tool} not available.")
            return 2

    if not args.pre_build_only and not args.port:
        print("ERROR: --port is required unless --pre-build-only is set.")
        return 2

    # Tag list
    if args.tags:
        old_tags = args.tags
    else:
        old_tags = _discover_tags(args.new_tag, args.stable_only)
    if not old_tags:
        print("ERROR: No old tags to test.")
        return 2

    print(f"Tags ({len(old_tags)}): {', '.join(old_tags)}")
    print(f"Env: {args.env}    Chip: {args.chip}    Repo: {args.repo}")

    _section("Docker builder image")
    _build_docker_image(force=args.rebuild_image)

    use_temp = args.work_dir is None
    work_dir: Path = Path(tempfile.mkdtemp(prefix="wled_ota_suite_")) \
                    if use_temp else Path(args.work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)
    asset_dir    = work_dir / "assets"
    bl_cache_dir = work_dir / "bootloaders"
    new_dir      = work_dir / "new_app"

    worktree_added = False
    try:
        # ----- Resolve plans -------------------------------------------------
        _section("Resolve plans (platform + partitions + asset) per tag")
        plans:   list[TagPlan]   = []
        results: list[TagResult] = []
        for t in old_tags:
            try:
                plan = _resolve_plan(t, args.env, args.repo)
                plans.append(plan)
                print(f"  {t}: platform={plan.platform_ver or '(unpinned)'} "
                      f"csv={plan.csv_name} asset={plan.asset_name}")
            except StepError as exc:
                print(f"  {t}: SKIP — {exc}")
                results.append(TagResult(t, Result.SKIP, str(exc)))

        # ----- Download release assets --------------------------------------
        _section("Download release assets")
        plan_assets: dict[str, Path] = {}
        for plan in list(plans):
            try:
                p = _download_asset(plan.tag, plan.asset_name, asset_dir, args.repo)
                plan_assets[plan.tag] = p
            except StepError as exc:
                print(f"  {plan.tag}: SKIP — {exc}")
                results.append(TagResult(plan.tag, Result.SKIP, str(exc)))
                plans.remove(plan)

        # ----- Build / cache bootloader + partitions per (platform, csv) ----
        _section("Build bootloader + partitions per (platform, partitions) combo")
        plan_boot:  dict[str, Path] = {}
        plan_parts: dict[str, Path] = {}
        plan_layout: dict[str, PartitionLayout] = {}
        for plan in list(plans):
            try:
                layout = _parse_partition_layout(plan.csv_text)
                plan_layout[plan.tag] = layout
                spec = _normalise_platform(plan.platform_ver)
                print(f"  {plan.tag}: platform spec → {spec}")
                bb, pp = _build_bootloader_and_partitions(
                    spec, plan.csv_text, bl_cache_dir,
                )
                plan_boot[plan.tag]  = bb
                plan_parts[plan.tag] = pp
            except StepError as exc:
                print(f"  {plan.tag}: SKIP — {exc}")
                results.append(TagResult(plan.tag, Result.SKIP, str(exc)))
                plans.remove(plan)

        # ----- Build the new app once ---------------------------------------
        _section("Build NEW app from current tree (IDF V4 / v16.x)")
        new_dir.mkdir(parents=True, exist_ok=True)
        new_app_bin = new_dir / "firmware.bin"
        if args.reuse_new_app and new_app_bin.exists():
            print(f"  reusing {new_app_bin}")
        else:
            wt = work_dir / "wled_worktree"
            if not (wt / ".git").exists():
                if wt.exists():
                    shutil.rmtree(wt, ignore_errors=True)
                _run(["git", "worktree", "add", "--detach", str(wt), "HEAD"],
                     label=f"git worktree add {wt}", timeout=60)
                worktree_added = True
            else:
                worktree_added = True
            try:
                produced = _build_new_app(args.env, wt)
                shutil.copy2(produced, new_app_bin)
            except Exception as exc:
                print(f"FATAL: cannot build new app: {exc}")
                return 2

        if args.pre_build_only:
            print("\nPre-build complete.")
            print(f"  Assets:      {asset_dir}")
            print(f"  Bootloaders: {bl_cache_dir}")
            print(f"  New app:     {new_app_bin}")
            return 0

        # ----- Per-tag hardware loop ---------------------------------------
        for plan in plans:
            tr = _run_one(
                plan=plan,
                layout=plan_layout[plan.tag],
                boot_bin=plan_boot[plan.tag],
                parts_bin=plan_parts[plan.tag],
                app_bin=plan_assets[plan.tag],
                new_app_bin=new_app_bin,
                port=args.port, chip=args.chip,
                boot_observe=args.boot_observe,
            )
            results.append(tr)
            marker = {"PASS":"✓","FAIL":"✗","SKIP":"–","ERROR":"!"}[tr.result.value]
            print(f"\n  [{marker}] {plan.tag}: {tr.result.value}"
                  + (f"  — {tr.notes}" if tr.notes else ""))
            if args.stop_on_fail and tr.result == Result.FAIL:
                print("  --stop-on-fail: aborting.")
                break

        # ----- Summary -----------------------------------------------------
        # Order results by original tag order
        by_tag = {r.tag: r for r in results}
        ordered = [by_tag[t] for t in old_tags if t in by_tag]
        print(f"\n\n{'=' * 60}\nREGRESSION SUITE SUMMARY\n{'=' * 60}")
        col = 32
        print(f"  {'Tag':<{col}}  {'Result':<6}  Notes")
        print(f"  {'-'*col}  {'------'}  -----")
        for r in ordered:
            print(f"  {r.tag:<{col}}  {r.result.value:<6}  {r.notes}")
        counts = {x: sum(1 for r in ordered if r.result == x) for x in Result}
        print(f"\n  PASS={counts[Result.PASS]}  FAIL={counts[Result.FAIL]}"
              f"  SKIP={counts[Result.SKIP]}  ERROR={counts[Result.ERROR]}")
        if counts[Result.FAIL]:
            print("\n  Tags whose bootloader cannot boot the new app:")
            for r in ordered:
                if r.result == Result.FAIL:
                    print(f"    {r.tag}")
        return (1 if counts[Result.FAIL] else
                2 if counts[Result.ERROR] else 0)

    except StepError as exc:
        print(f"\nFATAL: {exc}")
        return 3
    except KeyboardInterrupt:
        print("\nInterrupted.")
        return 3
    finally:
        keep = args.keep_work_dir or (not use_temp)
        if not keep and use_temp:
            print(f"\nRemoving temp work dir {work_dir} …")
            shutil.rmtree(work_dir, ignore_errors=True)
        if worktree_added:
            subprocess.run(["git", "worktree", "prune"],
                           cwd=REPO_ROOT, timeout=15, capture_output=True)


if __name__ == "__main__":
    sys.exit(main())
