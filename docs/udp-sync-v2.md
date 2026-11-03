# WLED sync protocol v2 (prototype)

Status: **prototype / proposal**, see [#5952](https://github.com/wled/WLED/issues/5952).
Code: `wled00/sync_proto.h` (wire format), `wled00/sync_v2.cpp` (protocol), hooks in `wled00/udp.cpp`.

*Partially AI-generated (Claude Code); reviewed against the source.*

## Why

### What the legacy notifier does
The legacy notifier (UDP 21324, "version 12") broadcasts a full snapshot of the state when something changes. It may repeat that snapshot blindly (`udpNumRetries`).
- It has no sender identity and no sequence numbers.
- It never sends state periodically.

### What follows from that
- **Missed changes:** a lost packet, or a node that boots late, stays out of sync until the next change.
- **Overwrites:** a delayed packet or retry overwrites newer state.
- **Retries are re-snapshots:** each retry re-reads the current state rather than repeating the message it is retrying.
- **Hard to parse and extend:** the format is an implicit byte layout, so parsing it safely is difficult, and every new field means another `if (version > N)` branch.

## Model: soft-state replication, last writer wins

LED state is *idempotent state*, not a log of commands. Only convergence to the latest state matters; intermediate states never need to be delivered. So v2 versions the state and lets receivers drop anything that isn't newer. It does not implement reliable delivery.

| Concept | Implementation |
|---|---|
| Identity | `nodeId` = lower 32 bits of the MAC address |
| Version | `(seq, origin)`. `seq` is a Lamport clock: the highest seq seen or created, plus 1 for a new local change. `origin` is the `nodeId` where the change was made. `seq` is compared first; `origin` breaks ties. |
| Apply rule | A receiver applies a STATE only if its version is newer than the last version applied for that sync group. Duplicates, retries and heartbeats are therefore harmless. |
| Echo suppression | `sender == self` (replaces the legacy 1 s "deaf" window for v2) |
| Repair / late join | The **origin** of the latest change re-sends it every `WLED_SYNC_V2_HEARTBEAT` ms (default 15 s, plus up to 1 s random jitter). A joining node sends `STATE_REQ`, and the origin answers within about 300 ms. Nodes that only applied a state don't re-advertise their *own copy*: it can differ from the original (receive filters, shorter strip, segment options not synced), and because an equal version is never re-applied, a late joiner could otherwise lock onto a wrong copy. Instead, every receiver keeps the origin's datagrams of the latest state and **relays them byte-identical** (only `sender` is patched, and `flags` = `RELAY`) in answer to a `STATE_REQ`. A late joiner therefore gets the state even after the origin is gone. Each relayer waits a random 20–300 ms and cancels if it hears the same version first (the suppression mDNS and IGMP use), so typically only one node answers. |
| Reboot / clock catch-up | A rebooted node learns the clock from heartbeats. If its first change is still older than what a peer has applied, the peer replies with `CLOCK`. The origin then re-sends its change once with a newer version. |
| Divergence | A local change that is *not* sent stops heartbeats until the node next sends a change of its own. Examples: call mode filtered out, `nn` / `CALL_MODE_NO_NOTIFY`, sending disabled, state applied from a legacy packet. Without this, a node would advertise a state that doesn't match its version. |
| Retries | Retries keep the version of the change and carry `FLAG_RETRY`. They are suppressed if the state has diverged since. |

### Why not a JGroups-style stack

JGroups (PING, GMS, NAKACK2, STABLE, FD_ALL, STATE_TRANSFER) provides group membership, ordered and reliable multicast, and state transfer. v2 takes over these ideas from it:
- Node identity.
- Sequence numbers.
- Membership and failure detection via heartbeats.
- State transfer on join.

It deliberately leaves out these parts:
- **NAKACK** (retransmission on negative ack): it needs a per-sender retransmit buffer and unicast NAKs. A newer state already supersedes a lost one, so retransmitting the lost one is pointless.
- **GMS** (a coordinator and agreed membership views): it brings coordinator election and view changes. Nothing in LED sync needs agreement on membership.
- **STABLE** (digest gossip): it exists only to garbage-collect NAKACK buffers.

RAM is the binding constraint on ESP8266; the node list is already capped at 24 there. The soft-state model needs a fixed ~150 bytes of protocol state, however many nodes there are.

## Wire format

These are packed little-endian structs, defined in `wled00/sync_proto.h` (`static_assert` checks the sizes). A datagram is laid out as follows:

```
SyncV2Header (24 B) | TLV | TLV | ...      (TLV = SyncV2Tlv {type, len} + len bytes)
```

| Header field | Size | |
|---|---|---|
| `magic` | 4 | `0xA5 'W' 'S' '2'`. The first byte is non-ASCII and unused by any other protocol on the port, so legacy receivers ignore v2 |
| `version` | 1 | 1. Later versions keep this header layout |
| `msgType` | 1 | `STATE` = 1, `STATE_REQ` = 2, `CLOCK` = 3 |
| `flags` | 1 | `HEARTBEAT` = 0x01, `RETRY` = 0x02, `RELAY` = 0x04 (another node's datagram, re-sent unchanged. Receivers ignore its `NODEINFO`, because the IP is the relayer's) |
| `groups` | 1 | sync groups (bit mapped) |
| `sender` | 4 | node id of the sender |
| `origin` | 4 | node id where the change was made |
| `seq` | 4 | Lamport clock of the change |
| `part`, `parts` | 1+1 | a state that doesn't fit one datagram is split by segments. Every part is self-contained |
| `payloadLen` | 2 | TLV bytes that follow |

| TLV | Size | Content |
|---|---|---|
| `GLOBAL` (1) | 28 | bri, transition, nightlight, main segment id, CCT-bus flag, timebase, system time and source, `segMask` (ids of all active segments). Sent in every part |
| `SEGMENT` (2) | 37 | one per segment, keyed by its **real segment id**: geometry, options, effect, sliders, checks, palette, 3 colors, CCT, blend mode |
| `NODEINFO` (3) | 38 | name, build, board, on/off. Part 0 of UDP STATE messages; feeds the existing node list, with the IP taken from the datagram source |

**Extensibility:** receivers skip unknown TLV types. For a TLV longer than expected, they read only the known prefix. New fields are appended to a TLV, or added as a new TLV, without a version ladder.

**Sizes:**
- UDP datagrams are at most 1460 bytes, the arduino-esp32 `WiFiUDP` flush size. That fits 35 segments per part, so 64 segments take 2 parts.
- ESP-NOW datagrams are at most 250 bytes, which fits 5 segments per part.
- A typical 1-segment state is 24 + 30 + 40 + 39 = 133 bytes, against 617–2345 bytes for the legacy packet.

**Validation:**
- magic and version;
- `payloadLen` must not exceed the datagram;
- every TLV must fit inside the payload;
- a STATE must contain a `GLOBAL` TLV;
- `part` must be less than `parts`.

No application CRC is used: the UDP checksum and the 802.11 and ESP-NOW FCS already detect corruption. There is no authentication either, per `docs/securecode.instructions.md` (broadcast sync is exempt).

## Transport and compatibility

- v2 uses the same port (21324) and the same broadcast address as the legacy notifier, and is dispatched on the first byte.
  - **New firmware sends v2 only, and still receives legacy v12.**
  - Old firmware ignores v2 datagrams; they fall through to `UsermodManager::onUdpPacket`.
  - During a transition, mixed fleets therefore sync in one direction only, old → new.
- ESP-NOW carries the same v2 datagrams; there is no separate fragmentation layer.
  - Received datagrams are queued, through a single-producer/single-consumer queue of 8 entries, from the ESP-NOW receive task to the main loop.
  - Like legacy ESP-NOW sync, they are accepted only from linked remotes and only when WiFi is not connected.
- Receive filters work as in the legacy notifier, and so do sync groups and segment options/bounds. Two differences:
  - "Simple" sync (segment options off) also applies the palette when no filter is selected; legacy required the palette filter to be ticked.
  - Bounds sync deletes local segments whose *id* the sender doesn't have, using `segMask`. Before, it compared segment *counts*.
- **Behaviour change: boot presets.** A node with receive enabled now takes the group state within about 2 s of connecting, via `STATE_REQ`, replacing its boot preset. Users who rely on different boot presets per node will notice.
- The legacy node info broadcast on 65506 is unchanged. v2 `NODEINFO` additionally updates the same node list.
- There is no new UI. The existing send/receive settings and groups are used, and the heartbeat interval is a build flag (`-D WLED_SYNC_V2_HEARTBEAT=<ms>`).

## Open points

1. **Mixed fleets:** should v2 nodes also *send* legacy for a release or two?
2. **Port:** stay on 21324 or move to a dedicated port?
3. **Membership view:** expose v2 peers (node id, last heartbeat, groups) in `/json/nodes`, and replace the 65506 beacon?
4. **Heartbeat interval:** make it a UI setting?
5. **Relay cache cost:** each node keeps the latest state's datagrams on the heap. That's 133–718 bytes on ESP8266 over UDP, about 1 KB over ESP-NOW, and up to about 3.2 KB on PSRAM builds over ESP-NOW (13 parts). Relays go out on the transport the state arrived on. Periodic repair after the origin leaves is still heartbeat-only (by the origin); relays only answer `STATE_REQ`.
6. **Late join opt-out:** make the boot-preset behaviour above a setting?
7. **Nightlight and receive filters:** the receiver applies nightlight state whatever the receive filters say (legacy behaviour, kept for now).
