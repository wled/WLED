# ESP32-P4 Ethernet network helper compatibility

This branch keeps WLED's network helper on the unique symbol `WLEDNetwork` while preserving existing WLED source call sites that use `Network`.

## What changed

WLED previously had its own helper object named `Network`. The ESP32 Arduino 3.x / ESP-IDF 5 stack also provides a global `Network` object for its `NetworkManager`, which creates a name collision when building the ESP32-P4/V5 stack.

To avoid that collision while minimizing churn in the branch:

- WLED's helper class/object are named `WLEDNetworkClass` and `WLEDNetwork`.
- `Network.h` maps legacy WLED helper calls with `#define Network WLEDNetwork` after including the framework WiFi/Ethernet headers.
- `Network.cpp` defines the `WLEDNetwork` object rather than a WLED-owned object named `Network`.
- The shared `DNSlookup` state used by `BusNetwork::resolveHostname()` is declared/defined so the current `bus_manager.cpp` code has a valid object to use.

## Consequences and tradeoffs

This is intended to keep both relevant ESP32 build stacks viable:

- the older/current WLED ESP32 stack where WLED code historically used `Network`, and
- the ESP32-P4/V5 stack where Arduino-ESP32 provides its own global `Network` object.

The compatibility macro keeps the diff small because most WLED call sites can remain as `Network.isConnected()`, `Network.localIP()`, etc. Those calls are redirected to WLED's `WLEDNetwork` helper after `Network.h` is included.

The main downside is that `Network` becomes a preprocessor alias in translation units that include WLED's `Network.h`. Code in those translation units that intentionally needs Arduino-ESP32's own `NetworkManager Network` after including WLED's helper header may need to include/use it before this header or locally `#undef Network`. This branch does not currently add such a use case.

The helper object name exposed by WLED is now `WLEDNetwork`. Any external code or usermod that directly declared or linked against WLED's old `NetworkClass Network` symbol, instead of using normal WLED headers and call sites, may need adjustment.

No runtime behavior change is intended for Wi-Fi, Ethernet, Art-Net, DDP, Improv, JSON info, or Espalexa. The change is primarily a build/link compatibility fix for the network helper name collision.

## Test note

ETH and DDP tested only.

PlatformIO build validation should still be run for at least:

- `esp32dev_V4`
- `esp32dev`
- `esp32p4_32MB`
