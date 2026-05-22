# FSEQ Player + FPP Connect usermods for WLED

This package contains two WLED usermods:

- **FSEQ** – local `.fseq` playback from the SD card as a WLED effect
- **FPP Connect** – FPP-compatible discovery, status, upload, and sync control

Together they let WLED play FSEQ files locally, expose a small SD card manager UI, and behave like a lightweight FPP-controlled receiver.

---

## What the current code does

### Local FSEQ playback

The **FSEQ Player** effect is added to WLED and plays `.fseq` files directly from the SD card.

Effect controls:

- **Index** → selects the file by index (`custom3` / `c3`)
- **Loop** → repeats playback continuously
- **Send Sync Multicast** → sends FPP sync packets to multicast while the local effect is playing
- **Send Sync Broadcast** → sends FPP sync packets to broadcast while the local effect is playing

Effect definition in code:

```cpp
"FSEQ Player@,,,,Index,Loop,Send Sync Multicast,Send Sync Broadcast;;;c3=0,o1=1,o2=0,o3=0"
```

When the selected file changes, the player loads the new sequence for the current segment and starts from the beginning.

---

## File indexing

The usermod builds an index of `.fseq` files found on the **root of the SD card**.

Current behavior:

- scans **root only** (`/`)
- ignores subfolders
- includes `.fseq` and `.FSEQ`
- sorts files **alphabetically, case-insensitive**
- caches up to **128** indexed FSEQ filenames internally

Example:

| Index | File |
|------:|------|
| 0 | `/00-snow.fseq` |
| 1 | `/01-christmas.fseq` |
| 2 | `/02-finale.fseq` |

### Important local UI limitation

The local WLED effect uses **`custom3` (`c3`)** for file selection. In practice this is intended for the usual WLED `c3` range, so **local manual selection is only practical for the first 32 entries**.

The internal filename cache is larger so that:

- FPP control can still start files by **filename**
- the web/API file list can still expose more entries

So:

- **local effect selection** → best for the first 32 indexed files
- **FPP/file-name control** → can still address files beyond that

---

## Multi-segment behavior

Local playback is **segment-aware**.

Each segment can run its own FSEQ file with its own:

- selected index
- loop state
- playback position

That means you can save presets where multiple segments each play different local FSEQ files.

### While FPP is active

When FPP override is active, local segment playback is temporarily suppressed so realtime/FPP playback can take over.

The override is considered active for about **3 seconds** after the last valid FPP control activity.

---

## FPP integration

The second usermod registers itself as **`FPP Connect`** and provides FPP-compatible discovery and control behavior.

### UDP listeners

The code listens on **UDP port `32320`** using both:

- normal UDP/broadcast listener
- multicast listener on **`239.70.80.80:32320`**

### Discovery / ping behavior

After Wi-Fi connects, the usermod sends a short discovery burst and then continues periodic announcements.

Current behavior:

- sends discovery to **subnet broadcast** and **global broadcast**
- sends an initial burst of **5** announcements
- burst interval: **1 second**
- regular ping/discovery interval: **10 seconds**

### Supported incoming UDP packet types

The code reacts to:

- **START** packets
- **SYNC** packets
- **PING** packets
- **BLANK** packets
- **STOP** packets

### Realtime playback behavior

Incoming FPP sync control uses the realtime FSEQ player.

Highlights from the current implementation:

- a **START** packet can start playback from a given filename/time
- a **SYNC** packet can also start playback if the current file is not already active
- sync updates use a **soft drift correction** / slew-based approach instead of constant hard jumps
- a **STOP** or **BLANK** packet stops realtime FPP playback

### Local effect as sync source

The local **FSEQ Player** effect can also send outgoing FPP sync packets while it is playing.

Current behavior:

- sends **START** when playback begins or settings change
- sends periodic **SYNC** updates every **500 ms** while enabled
- sends **STOP** when playback ends or sync sending is disabled
- can send to **multicast**, **broadcast**, or both depending on the effect checkboxes

---

## FPP-compatible HTTP endpoints

The code currently registers these endpoints:

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/system/info` | GET | FPP-style device info |
| `/api/system/status` | GET | FPP-style playback/status info |
| `/api/fppd/multiSyncSystems` | GET | multi-sync system info |
| `/fpp` | POST | file upload endpoint used by FPP |
| `/fseqfilelist` | GET | simple JSON list of `.fseq` files |

The status/info responses identify the device as a WLED-based remote-style FPP target.

---

## XLZ upload and auto-unpack support

The code includes **XLZ** support using `unzipLIB`.

### What happens with `.xlz` files

When a file is uploaded through the **FPP upload endpoint** (`POST /fpp`):

- `.fseq` uploads are stored directly
- `.xlz` uploads are stored first and unpacked later

### Deferred unpack behavior

For `.xlz` uploads received through `/fpp`:

- unpacking is **deferred**
- it starts after **10 seconds of upload inactivity**
- unpacking only runs when **no playback is active**
- the extracted file is written as `.fseq`
- the original `.xlz` archive is removed after successful extraction

### Boot-time XLZ scan

On boot, the code also checks the SD card root once after a short delay.

Current behavior:

- waits about **2 seconds** after startup
- scans the SD root for pending `.xlz` files
- unpacks them only when no playback is currently active

### XLZ extraction details

The extractor currently:

- scans the **SD root only**
- extracts **only the first file** from the archive
- normalizes names and rejects simple path traversal entries like `../...`
- checks available SD space before extracting

### Important note about the web UI upload

The built-in **web UI upload endpoint** (`/api/sd/upload`) uploads files **as-is**.

So if you upload an `.xlz` file through the browser UI:

- it is stored on the SD card
- it is **not immediately auto-unpacked by that endpoint**

Automatic deferred XLZ handling is implemented in the **FPP upload path** (`/fpp`).

---

## Web UI

The FSEQ usermod adds an entry in the WLED **Info** page that opens:

```text
/fsequi
```

This page is an SD manager for the usermod.

### What the page shows

- **SD Storage** usage bar
- **FSEQ Files** list with the indexed order used by the effect
- **Other SD Files** list
- **Upload** section
- **Delete** buttons for files
- short usage instructions

### Web UI API endpoints

These endpoints are registered by `WebUIManager`:

| Endpoint | Method | Purpose |
|---|---|---|
| `/fsequi` | GET | HTML UI |
| `/api/sd/list` | GET | full SD overview: indexed FSEQ files, other files, storage usage |
| `/api/fseq/list` | GET | indexed FSEQ list only |
| `/api/sd/upload` | POST | browser upload to SD |
| `/api/sd/delete` | POST | delete a file from SD |

---

## How to use local playback

1. Copy one or more `.fseq` files to the **root** of the SD card.
2. In WLED, select the **FSEQ Player** effect.
3. Choose the file using the **Index** control.
4. Enable **Loop** if needed.
5. Optionally enable **Send Sync Multicast** and/or **Send Sync Broadcast**.
6. Save the setup as a preset if you want to reuse it or start it on boot.

---

## Presets and boot preset

Because playback is driven by the WLED effect engine, normal WLED presets can store the local setup, including:

- selected effect
- selected index (`c3`)
- loop state
- sync send checkboxes
- segment configuration

To auto-start a local sequence after boot:

1. configure the effect
2. save it as a preset
3. assign that preset as the WLED **Boot preset**

---

## Requirements

Based on the code in this package, the usermod expects:

- a working **SD card backend** in WLED
- the standard **`sd_card` usermod** or equivalent SD setup already handling card initialization
- WLED built for a target that provides either:
  - `WLED_USE_SD_SPI`, or
  - `WLED_USE_SD_MMC`
- `unzipLIB` for XLZ extraction

This package uses `sd_adapter_compat.h` so it can talk to the available SD backend without directly depending on a specific SD object name in every source file.

---

## Practical notes / limitations

- Only the **SD root directory** is scanned for `.fseq` and `.xlz` files.
- Local effect selection is effectively best for the **first 32 indexed files**.
- The internal FSEQ index cache can hold up to **128** filenames.
- FPP realtime playback temporarily overrides local segmented playback.
- Browser upload and FPP upload do **not** behave exactly the same for `.xlz` files.
- Sequence rendering maps FSEQ data as **RGB triplets** across LEDs. Extra channel data is ignored once the target segment/strip is full; missing data is cleared to black.

---

## Summary

This codebase currently provides:

- local SD-card-based FSEQ playback as a WLED effect
- per-segment local playback
- optional outgoing FPP sync from the local effect
- incoming FPP discovery, ping, blank, start, stop, and sync handling
- FPP-style info/status HTTP endpoints
- a small SD management web UI
- deferred `.xlz` extraction for FPP uploads plus boot-time pending archive processing

If you rename, add, or remove `.fseq` files on the SD card, the alphabetical index order can change, so presets that rely on a specific local index may need to be updated.
