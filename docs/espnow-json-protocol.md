# Bidirectional ESP-NOW API

This document specifies the wire protocol WLED uses for **bidirectional** ESP-NOW communication with a remote. It is aimed at authors of remote firmware (e.g. a touch-screen controller) that want WLED JSON state control. [See a working remote that uses this API](https://github.com/figamore/wled-touch-remote)

The classic ESP-NOW remote support is one-way: WizMote-style remotes can only *send* button presses. This API adds a two-way channel so a remote can issue WLED JSON state API commands (`/json/state` parity), receive responses, subscribe to binary LED peek frames, and be **pushed** state updates whenever WLED's state changes - including changes made from the WebUI or another remote.

## Relationship to the WebSocket / HTTP JSON API

A `REQUEST` payload is byte-for-byte the same JSON you would `POST` to `/json/state` or send over the `/ws` WebSocket for state control. It is applied through the same `deserializeState()` path, so every state field (`on`, `bri`, `seg`, `ps`, `playlist`, …) behaves identically. A verbose `RESPONSE` carries `{"state":…, "info":…}`, the same object the WebSocket broadcasts. Routine companion polling and `PUSH` messages use the compact state form described below.

The WebSocket live LED peek command `{"lv":true}` is also supported. It subscribes the remote to `LIVE` frames carrying the same binary payload used by WebSocket liveview: `L`, version byte, optional 2D dimensions, then RGB triples. `{"lv":false}` unsubscribes.

### Catalog requests

`deserializeState()` only *applies* state, so the effect/palette/preset catalogs (which the WebUI fetches from `/json` and `/presets.json`) are exposed through a `{"get":…}` REQUEST. The `RESPONSE` lets a remote populate its lists instead of hardcoding them:

| Request          | Response |
|------------------|----------|
| `{"get":"fx"}`   | `{"effects":["Solid","Blink",…]}` - array index = effect id |
| `{"get":"pal"}`  | `{"palettes":["Default","* Random Cycle",…]}` - array index = palette id |
| `{"get":"ps"}`   | `{"presets":{"1":"Sunset","2":"Party",…}}` - only existing presets, by id |

The preset catalog covers the full id range 1–250 in a single pass over `presets.json`. Catalog responses are large; on an ESP8266 host they may exceed `ESPNOW_API_MAX_JSON` and return `{"error":8}`, in which case a remote should fall back to a built-in list. An unknown `get` value returns `{"error":10}`.

## Security model

There is **no encryption or authentication on the wire in v1**. Access is gated by the existing **MAC allow-list** (`linked_remotes`, configured under *WiFi Settings → ESP-NOW Wireless*). Only frames from a whitelisted MAC are processed - identical to how the existing WizMote remote path already grants whitelisted MACs full JSON-API access via `remote.json`. Optional ESP-NOW PMK/LMK encryption is possible future work.

No extra setting is required: as with the WizMote path, the API is active whenever ESP-NOW is enabled (*WiFi Settings → ESP-NOW Wireless*) and the remote's MAC is in the allow-list. WLED only broadcasts state `PUSH` frames once it has actually seen an API frame from a remote, so a WizMote-only deployment never emits them.

## Frame format

ESP-NOW limits a frame to 250 bytes. Each frame is a 6-byte header followed by up to 244 bytes of a possibly fragmented payload:

| Offset | Field       | Size | Description                                                        |
|-------:|-------------|-----:|--------------------------------------------------------------------|
| 0      | `magic`     | 1    | `0x4E` (`'N'`). Distinct from WizMote (`0x80/0x81/0x91`) and sync (`'W'`/`0x57`). |
| 1      | `version`   | 1    | Protocol version, currently `0x01`.                                |
| 2      | `msgType`   | 1    | Message type (see below).                                          |
| 3      | `msgId`     | 1    | Sender-chosen id; a `RESPONSE` echoes the `REQUEST`'s `msgId`.     |
| 4      | `fragIndex` | 1    | 0-based fragment index.                                            |
| 5      | `fragTotal` | 1    | Total number of fragments (`>= 1`).                                |
| 6…     | `data`      | ≤244 | Raw payload bytes for this fragment (not NUL-terminated).          |

### Message types (`msgType`)

| Value | Name       | Direction        | Payload |
|------:|------------|------------------|---------|
| 0x01  | `REQUEST`  | remote → WLED    | A JSON command (same as a `/json/state` body). `{"v":true}` requests current state without changing anything. `{"lv":true}` starts live LED peek frames (keepalive, see below); `{"lv":false}` stops them. |
| 0x02  | `RESPONSE` | WLED → remote    | Reply to a `REQUEST`. `{"state":…,"info":…}` when verbose (the request set `"v":true`), otherwise `{"success":true}`. When the request **changed** state, a compact broadcast `PUSH` is imminent, so the direct reply is `{"success":true}` even if `"v":true` was set (same rule as the WebSocket API). On error, `{"error":<code>}` - see the error codes below. `msgId` echoes the request. |
| 0x03  | `PUSH`     | WLED → remotes   | Unsolicited compact `{"state":…}` broadcast whenever WLED's state changes. `msgId` is a free-running counter. No reply expected. |
| 0x04  | `DISCOVER` | remote → WLED    | Empty discovery query, broadcast on the channel being scanned. `msgId` identifies that scan. The source must already be in WLED's MAC allow-list. The constant retains its historical `ESPNOW_API_HELLO` name in source for compatibility. |
| 0x05  | `LIVE`     | WLED → remote    | Binary LED peek frame sent after `{"lv":true}`. Payload is the same as WebSocket liveview binary frames: `L`, version `1` or `2`, optional 2D width/height for version `2`, then sampled RGB triples. |
| 0x06  | `ANNOUNCE` | WLED → remote    | Reliable unicast response to `DISCOVER`. It echoes the discovery `msgId` and carries `{"announce":{"name":…,"mac":…,"ver":…,"ch":…,"proto":1,"cap":15}}`. The link-layer source MAC is authoritative. |

`announce.cap` is a bit mask: bit 0 = compact state, bit 1 = catalogs, bit 2 = unsolicited state push, and bit 3 = live peek. A remote must ignore unknown capability bits and must not use an optional feature whose bit is absent.

For low-overhead polling, `{"v":"compact"}` returns power, brightness, preset, and the main-segment effect controls in a single-frame state document. `{"v":true}` retains the full WebSocket-compatible state response. Unsolicited `PUSH` messages use the compact form.

### Error codes

| Code | Meaning | Remote should |
|-----:|---------|---------------|
| 3    | Transient: JSON buffer, TX slot or heap busy | Retry after a short delay |
| 8    | Response exceeds `ESPNOW_API_MAX_JSON` | Not retry; fall back (non-verbose polling / built-in list) |
| 9    | Request failed to parse as JSON | Fix the request |
| 10   | Unknown `{"get":…}` catalog key | Fix the request |

Every `REQUEST` - including catalog requests - is answered with either its payload or an `{"error":…}`. `ANNOUNCE` and `RESPONSE` are reliable unicasts; `PUSH` and `LIVE` remain best-effort because a later poll or frame supersedes them.

**Live keepalive.** ESP-NOW has no connection or disconnect signal, so a `{"lv":true}` subscription is a keepalive: WLED stops streaming `LIVE` frames **30 s** after the last `{"lv":true}` unless the remote re-arms it. A remote that wants a continuous live view should re-send `{"lv":true}` every ~10 s; send `{"lv":false}` to stop immediately. Only one live subscriber is served at a time (a `LIVE` payload is binary - branch on `msgType == 0x05` before parsing reassembled bytes as JSON).

> **ESP8266 size cap.** A full `{"state","info"}` response is often 1.5–3 KB, which can exceed
> `ESPNOW_API_MAX_JSON` (2048 bytes on ESP8266; 8192 on ESP32). When it does, a verbose
> `RESPONSE` returns `{"error":8}`. Compact polling and compact `PUSH` messages remain
> available. On ESP8266 prefer compact polling or non-verbose commands.

## Fragmentation & reassembly

- A logical message is identified by the tuple `(source MAC, msgType, msgId)`. All of its fragments share the same `magic`, `version`, `msgType`, `msgId` and `fragTotal`; `fragIndex` runs `0 … fragTotal-1`.
- Every fragment except the last **must** carry a full 244-byte payload so byte offsets line up; the last fragment carries the remainder.
- The receiver allocates a buffer of `fragTotal × 244` bytes on the first fragment and writes each fragment at `fragIndex × 244`. The message is complete once all fragments have arrived.
- **Fail-closed:** any out-of-order start, mismatched `(MAC,msgType,msgId,fragTotal)`, oversized payload, or duplicate is discarded; partial buffers are abandoned after **500 ms**, even if no additional ESP-NOW API frames arrive.
- WLED bounds a single message to `ESPNOW_API_MAX_JSON` (2048 bytes on ESP8266, 8192 on ESP32). A remote should do the same for JSON requests. A full multi-segment `{"state","info"}` is typically 1–4 KB, i.e. several fragments. If a verbose response is too large to send, WLED replies with `{"error":8}` instead of silently dropping the response.

WLED's native transport copies callback frames into a bounded fixed queue, then reassembles and applies JSON in the main loop. It processes requests serially; a remote should wait for a matching `RESPONSE` (or retry after a short timeout) before sending the next `REQUEST`.

## Multiple WLED instances from one remote

No explicit instance id is needed. Because every ESP-NOW frame carries the sender's MAC at the link layer, a remote:

- **targets** a specific WLED by unicasting a `REQUEST` to that WLED's MAC, and
- **distinguishes** instances by the source MAC of the `RESPONSE`/`PUSH` frames it receives.

`PUSH` frames are broadcast (one transmission reaches all paired remotes) but still carry the originating WLED's source MAC, so a remote tracking several instances can route each push to the right one.

## Discovery & channel

ESP-NOW peers must be on the **same WiFi channel**. A WLED instance uses the channel of the network it joined (STA mode) or its AP channel (AP mode). To discover instances, a remote broadcasts a `DISCOVER` on a channel and collects unicast `ANNOUNCE` replies whose `msgId` matches that scan; it can iterate channels until replies arrive. Matching the transaction ID prevents delayed or unrelated discovery traffic from changing the registry. The remote learns each WLED's MAC from the reply's link-layer source address (the `mac` field is convenience metadata).

WLED learns the remote's MAC from a valid inbound control `REQUEST`, remote `DISCOVER`, or WiZ Mote frame; the WiFi-settings *"Last device seen"* control surfaces it so the user can add it to the allow-list. Outbound-direction frames such as `ANNOUNCE` are deliberately ignored as bonding candidates.

`ANNOUNCE` replies use a short MAC-derived delay. This staggers several WLED instances responding to one discovery event, while unicast delivery supplies a MAC-level acknowledgement. Remotes should keep the discovery window open long enough to collect every reply rather than locking onto the first responder.

## Reliability notes (v1)

- A remote sends one `REQUEST` at a time and waits for the matching `(source MAC,msgId)` `RESPONSE`. It retries with the **same message ID and identical payload** after timeout.
- WLED caches recently completed mutations by `(source MAC,msgId,payload hash)` for ten seconds, longer than the bounded retry horizon but short enough to avoid normal 8-bit `msgId` wrap. A retry receives another success response without applying a toggle or other non-idempotent mutation twice. Read-only polls and catalog requests may safely be regenerated.
- A dropped fragment loses the logical message. The request timeout/retry recovers it; fragmented best-effort `PUSH` and `LIVE` data are simply superseded by a later poll/frame.
- `PUSH` is best-effort: a missed push is corrected by the next state change or an explicit poll.
- WLED transmits one outbound message at a time, a few fragments per main-loop pass, so large responses never stall LED rendering. A `RESPONSE` or `ANNOUNCE` reply preempts a pending `PUSH`/`LIVE` transmission; a `PUSH` or `LIVE` frame that would have to wait is skipped instead (the next state change or live frame supersedes it).
- Only one ESP-NOW live LED peek subscriber is tracked at a time, matching the existing WebSocket liveview behavior.
- Live preview is capped at 256 pixels and 10 frames per second so fragmented preview traffic does not starve request/response control traffic.
- While an API remote is actively communicating (any API frame within the last 2 minutes), WLED keeps its fallback AP and ESP-NOW channel stable instead of performing a destructive STA retry. Normal WiFi retries resume after the remote becomes inactive; an explicit settings change or manual reconnect may still restart the radio. A remote should treat silence as a hint to re-`DISCOVER`/re-subscribe.

## Examples

Set power and brightness (single fragment):

```text
header: 4E 01 01 07 00 01          msgType=REQUEST msgId=7 frag 0/1
data  : {"on":true,"bri":128,"v":true}
```
WLED replies with `{"success":true}` and then broadcasts the updated compact state in a
`PUSH` frame.

Poll current state without changing anything:

```text
header: 4E 01 01 08 00 01          REQUEST msgId=8
data  : {"v":true}
```
WLED replies with one or more `RESPONSE` (0x02) frames, `msgId=8`, reassembling to
`{"state":{…},"info":{…}}`.

Companion remotes should poll with `{"v":"compact"}` instead. Its response contains only the power, brightness, preset, and main-segment controls and fits in one ESP-NOW frame.

Subscribe to LED peek frames:

```text
header: 4E 01 01 09 00 01          REQUEST msgId=9
data  : {"lv":true}
```
WLED replies with `{"success":true}`, then sends `LIVE` (0x05) frames about every 100 ms. Reassemble each `LIVE` message by `msgId`; its payload starts with the WebSocket liveview binary header (`L`, version byte) followed by sampled RGB triples.

Discovery:

```text
header: 4E 01 04 2A 00 01          DISCOVER msgId=42, broadcast
data  : <empty>
```
Each WLED replies by unicast with type `ANNOUNCE` (`0x06`), `msgId=42`, and `{"announce":{"name":"Living Room","mac":"a1b2c3d4e5f6","ver":2605011,"ch":6,"proto":1,"cap":15}}`.