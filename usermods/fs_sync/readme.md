## FS Sync usermod

Synchronizes image files (`.gif`, `.bmp`) on the WLED file system (LittleFS) between all instances that share a sync group. Upload an image to one instance and every instance in the same sync group picks it up automatically.

**ESP32 only** — packet authentication uses mbedtls, which is not available on ESP8266.

### How does it work?

- Each instance periodically scans its file system and builds a manifest (path, size, modification time and CRC32 of every file).
- The manifest checksum is announced to the subnet broadcast address on a dedicated UDP port (default 21325). Announcements carry the sender's sync groups and are only accepted by instances whose receive groups intersect — the same semantics as the built-in WLED state sync.
- An instance seeing a different manifest requests the full manifest, diffs it against its local file system and pulls every file that is missing locally or older than the remote copy. The newest modification time wins; ties are broken by the higher unit ID (last IP octet), so both sides always agree on the winner.
- Files are transferred in ~1300 byte UDP chunks, written to `<path>.part` and renamed over the target once size and CRC32 are verified. The file system API cannot set a file's modification time, so the winning modification time of a received file is recorded in a small local sidecar file (`/fssync.meta`) and reused while the file is unchanged — this keeps the manifests of instances that synced the same file identical.

### What is synced

Only image files — `.gif` and `.bmp`. The "Image" effect can display lowercase `.gif` files only (see `image_loader.cpp`); `.bmp` files are synced as plain file distribution. The extension match is case-insensitive and applies to files in subdirectories as well.

Everything else on the file system is never sent or received: the device configuration (`cfg.json`), security settings (`wsec.json`, which holds the WiFi passwords), presets, WLED backup files (`/bkp.*`), this usermod's metadata sidecar (`/fssync.meta`) and temporary files (including its own `.part` files).

File **deletions** are not propagated: deleting a file on one instance does not delete it on the others.

### Configuration

On the Usermod Settings page:

| Setting | Default | Description |
|---|---|---|
| enabled | on | Master switch for the usermod. |
| port | 21325 | UDP port. Must not be 21324 or 65506 (used by WLED itself). |
| scan | 60 | File system scan interval in seconds. |
| pass | (empty) | Shared passphrase. **Required for file transfers** — without it, instances only discover each other. When set, every packet carries a unix timestamp and a truncated HMAC-SHA256 tag keyed by the passphrase, so instances only sync when the passphrase matches. |

#### Compile-time passphrase

The passphrase can also be baked into the firmware as a default (used when none is configured at runtime), e.g. by adding `-D USERMOD_FS_SYNC_PASSPHRASE='"my-secret"'` to the build flags of the environment (e.g. via `platformio_override.ini`). Note that a compile-time passphrase is embedded in the firmware binary and can be extracted from it — it is a fleet-deployment convenience, not protection against someone who has the binary. A passphrase configured at runtime always takes precedence.

Status (peers, files, sent/received counters, errors) is shown on the Info page as `FS Sync`-prefixed rows in `/json/info` under `u`, along with a "Sync now" button that triggers a manual rescan. A rescan can also be triggered via the JSON API: `{"FS Sync":{"syncNow":true}}`.

### Requirements

- All instances must run firmware with this usermod enabled (ESP32 only), be on the same LAN subnet, share a sync group (Sync settings → send/receive groups) and run the same protocol version, exactly like the built-in state sync.
- A shared passphrase is required for file transfers (see Configuration).
- NTP time is recommended: modification times drive conflict resolution, and the packet timestamp replay protection only applies when both sides have a synced clock. The later modification time wins even if a device clock is ahead, so the winning file may be the older edit; the unit ID breaks only exact modification-time ties.

### Limitations

- LittleFS has no change notifications; changes are detected by the periodic scan (or the `syncNow` API call).
- Without NTP, the timestamp replay protection is skipped (HMAC authentication still applies).
- While the OTA lock is enabled, the instance does not accept (or request) file transfers; it can still serve files to peers.
- Concurrent edits of the same file on two instances converge to the newest modification time; a file being written while it is transferred may fail the CRC check and is retried on the next cycle.
- Flash wear: files are only written when their content actually differs.

### Security note

File transfers require a shared passphrase. When set, every packet is authenticated with a truncated HMAC-SHA256 tag (keyed by the passphrase) over the header, payload and a unix timestamp; the timestamp bounds replay when both sides have a synced clock (NTP). Without a passphrase, no files are transferred in either direction, and while the OTA lock is enabled, incoming file writes are rejected. Sync groups already scope traffic to instances you chose to sync with.

This is still a LAN protocol without TLS (see `docs/hardening.instructions.md` — WLED assumes a firewall-isolated deployment). The passphrase is a shared secret: it keeps out peers that do not know it, but it is not strong authentication — an attacker who captures traffic can attempt dictionary attacks against it, and the passphrase is stored in `cfg.json` on the device itself (like other usermod settings). Prefer a long random passphrase.
