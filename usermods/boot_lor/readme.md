# Usermods API v2 boot_lor usermod

## Installation 

Add `boot_lor` to `custom_usermods` in your PlatformIO environment and compile!

## Overview

`boot_lor` is a usermod that sets the WLED realtime override mode (`lor`) at boot.

It is designed for setups where WLED is primarily controlled via external APIs (e.g. HomeKit, Home Assistant, or custom integrations), and realtime streaming protocols (such as DDP) are only used occasionally.

By default, WLED enables realtime streaming at boot when data is received. This can interfere with API-driven control flows. This usermod ensures that a desired `lor` mode (typically `lor:2`) is enforced during startup.

---

## Use Case

This usermod is intended for environments where:

- WLED is primarily controlled via an external system (e.g. HomeKit via Homebridge, Home Assistant, or direct API usage)
- Multiple controllers should behave as **independent devices** under normal operation
- Realtime streaming (DDP, E1.31, etc.) is used **only when explicitly enabled**

### Example scenario

- Two WLED controllers are used as separate lights in HomeKit
- A secondary setup uses WLED "virtual LEDs" to mirror one controller to another via DDP
- This works well for effects and synchronized control

**Problem:**  
- At boot, realtime communication may take control as soon as packets arrive, overriding API-based control unexpectedly.
- Using JSON or HTTP API in a boot preset does not reliably set lor

**Solution:**  
Set `lor:2` at boot to disable realtime takeover by default.  
When realtime control is desired, manually switch back to `lor:0`.

---

## Behavior

The usermod applies the configured `lor` value during startup using the following sequence:

### Default behavior

```text
WiFi connected → (optional delay) → apply lor → assert for N seconds → stop
```

- Waits for network connectivity
- Waits an additional configurable delay (if set)
- Applies the configured `lor` value
- Reasserts it for a short period to allow system "settling"
- Stops running after completion

---

## Configuration

The following options are available under `"boot_lor"` in the WLED config:

```json
"boot_lor": {
  "bootLor": 2,
  "additionalWaitSec": 0,
  "assertForSec": 10
}
```

### Options

| Name                | Type | Default | Description |
|---------------------|------|---------|-------------|
| `bootLor`           | int  | `2`     | Realtime override mode to apply. Valid values: `-1` (disabled), `0`, `1`, `2` |
| `additionalWaitSec` | int  | `0`     | Additional delay (in seconds) after trigger (connection) before applying |
| `assertForSec`      | int  | `10`    | Duration (in seconds) to reassert the value after first application |

---

## Recommended Settings

For most DDP / API-first setups:

```json
{
  "bootLor": 2,
  "additionalWaitSec": 0,
  "assertForSec": 10
}
```

This ensures:

- Realtime streaming does not take control unexpectedly at boot
- The system waits for network connectivity before acting
- The setting is reinforced briefly to avoid race conditions

---

## Notes

- It does **not** interfere with realtime streaming once the assertion period is over (see startup sequence)
- It simply ensures a predictable startup state

---

## Status

After completion, the usermod stops running and has no ongoing impact on system performance.

---

## JSON Info

The current state is exposed under `info -> u`:

```text
Boot LOR: [bootLor, state, realtimeOverride]
```

Where:
- `state` is one of `waiting`, `applied`, or `finished`

---

## Tested
- Build: esp32dev
- Runtime: tested on ESP32-D0WD-V3

---

## Summary

This usermod provides a simple and reliable way to:

- Default to API-based control at boot
- Avoid unintended realtime takeover
- Retain full flexibility to enable realtime modes when desired
