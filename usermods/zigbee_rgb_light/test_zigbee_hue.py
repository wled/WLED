#!/usr/bin/env python3
"""
Integration test suite for the WLED Zigbee RGB Light usermod.

Tests control from a Philips Hue Bridge v2 to WLED via the Hue REST API
and WLED JSON API, plus WiFi/Zigbee coexistence verification.  Covers:

  - Hue -> WLED: on/off, brightness (full range), 6 named colors, combined commands
  - WiFi/Zigbee coexistence: HTTP connectivity before, during, and after Zigbee traffic
  - Rapid color change stress test
  - WLED info JSON Zigbee debug fields

Note: WLED->coordinator state sync uses Zigbee attribute reports (via
esp_zb_zcl_set_attribute_val from a scheduler_alarm callback).  This works
with Home Assistant (ZHA), Zigbee2MQTT, and other standards-compliant
coordinators, but the Philips Hue Bridge ignores unsolicited attribute
reports from non-certified devices.  Therefore there are no WLED->Hue
direction tests in this suite.

Prerequisites:
  - WLED device flashed with the zigbee_rgb_light usermod and paired with
    the Hue bridge (appears as a light in the Hue app).
  - The Hue bridge API key (obtain via:
    curl -sk -X POST https://<bridge>/api -d '{"devicetype":"test"}')
  - Both the Hue bridge and WLED device reachable on the local network.
  - pip install requests

Usage:
  python3 test_zigbee_hue.py \\
      --bridge-ip 192.168.178.216 \\
      --api-key XBfT0n000WWp2FV6DxcOnbhcxV5X7SFlKpB53Bix \\
      --light-id 23 \\
      --wled-ip 192.168.178.107

  # Run a single test:
  python3 test_zigbee_hue.py ... -k test_hue_on_off

  # Increase settle time for slow networks:
  python3 test_zigbee_hue.py ... --settle 5
"""

import argparse
import json
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Any, Optional

try:
    import requests
    # Suppress InsecureRequestWarning for self-signed Hue bridge cert
    import urllib3
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
except ImportError:
    print("ERROR: 'requests' package is required. Install with: pip install requests",
          file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
#  Configuration
# ---------------------------------------------------------------------------

@dataclass
class Config:
    bridge_ip: str
    api_key: str
    light_id: str
    wled_ip: str
    settle_time: float  # seconds to wait after sending a command
    verbose: bool


# ---------------------------------------------------------------------------
#  API Helpers
# ---------------------------------------------------------------------------

class HueAPI:
    """Minimal Philips Hue Bridge REST API client."""

    def __init__(self, bridge_ip: str, api_key: str, verify_ssl: bool = False):
        self.base_url = f"https://{bridge_ip}/api/{api_key}"
        self.verify = verify_ssl
        self.timeout = 10

    def get_light(self, light_id: str) -> dict:
        r = requests.get(f"{self.base_url}/lights/{light_id}",
                         verify=self.verify, timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def set_light_state(self, light_id: str, state: dict) -> list:
        r = requests.put(f"{self.base_url}/lights/{light_id}/state",
                         json=state, verify=self.verify, timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def get_light_state(self, light_id: str) -> dict:
        data = self.get_light(light_id)
        return data.get("state", {})

    def search_lights(self) -> list:
        """Trigger a new light search (permit-join)."""
        r = requests.post(f"{self.base_url}/lights",
                          json={}, verify=self.verify, timeout=self.timeout)
        r.raise_for_status()
        return r.json()


class WLEDAPI:
    """Minimal WLED JSON API client."""

    def __init__(self, wled_ip: str):
        self.base_url = f"http://{wled_ip}"
        self.timeout = 10

    def get_state(self) -> dict:
        r = requests.get(f"{self.base_url}/json/state", timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def set_state(self, state: dict) -> dict:
        r = requests.post(f"{self.base_url}/json/state",
                          json=state, timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def get_info(self) -> dict:
        r = requests.get(f"{self.base_url}/json/info", timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def is_reachable(self) -> bool:
        try:
            self.get_state()
            return True
        except Exception:
            return False


# ---------------------------------------------------------------------------
#  Test Framework
# ---------------------------------------------------------------------------

class TestResult:
    def __init__(self, name: str):
        self.name = name
        self.passed = False
        self.skipped = False
        self.error: Optional[str] = None
        self.details: list[str] = []

    def add_detail(self, msg: str):
        self.details.append(msg)


class TestRunner:
    def __init__(self, cfg: Config):
        self.cfg = cfg
        self.hue = HueAPI(cfg.bridge_ip, cfg.api_key)
        self.wled = WLEDAPI(cfg.wled_ip)
        self.results: list[TestResult] = []
        self._filter: Optional[str] = None

    def set_filter(self, pattern: Optional[str]):
        self._filter = pattern

    def settle(self, multiplier: float = 1.0):
        """Wait for commands to propagate through the Zigbee network."""
        time.sleep(self.cfg.settle_time * multiplier)

    def run_all(self) -> bool:
        """Run all tests and return True if all passed."""
        tests = [
            # --- Prerequisites ---
            self.test_prerequisites,
            self.test_hue_light_reachable,
            # --- WLED HTTP connectivity ---
            self.test_wled_http_tcp_connect,
            self.test_wled_http_json_state,
            self.test_wled_http_json_info,
            self.test_wled_http_win_endpoint,
            self.test_wled_info_zigbee_fields,
            # --- Hue -> WLED: on/off & brightness ---
            self.test_hue_on,
            self.test_hue_off,
            self.test_hue_on_off_cycle,
            self.test_hue_brightness,
            self.test_hue_brightness_range,
            self.test_hue_brightness_min,
            # --- Hue -> WLED: colors ---
            self.test_hue_color_red,
            self.test_hue_color_green,
            self.test_hue_color_blue,
            self.test_hue_color_white,
            self.test_hue_color_yellow,
            self.test_hue_color_cyan,
            self.test_hue_color_magenta,
            self.test_hue_combined_on_bri_color,
            # --- HTTP after Zigbee traffic ---
            self.test_wled_http_after_zigbee_traffic,
            # --- Stress / rapid changes ---
            self.test_rapid_color_changes,
            # --- HTTP sustained (last, after all Zigbee activity) ---
            self.test_wled_http_sustained,
        ]

        for test_fn in tests:
            name = test_fn.__name__
            if self._filter and self._filter not in name:
                continue
            result = TestResult(name)
            try:
                test_fn(result)
                if not result.skipped:
                    result.passed = True
            except AssertionError as e:
                result.error = str(e)
            except Exception as e:
                result.error = f"Exception: {type(e).__name__}: {e}"
            self.results.append(result)
            self._print_result(result)

        return self._print_summary()

    def _print_result(self, r: TestResult):
        if r.skipped:
            status = "SKIP"
        elif r.passed:
            status = "PASS"
        else:
            status = "FAIL"
        print(f"  [{status}] {r.name}")
        if self.cfg.verbose:
            for d in r.details:
                print(f"         {d}")
        if r.error:
            print(f"         ERROR: {r.error}")

    def _print_summary(self) -> bool:
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = sum(1 for r in self.results if not r.passed and not r.skipped)
        skipped = sum(1 for r in self.results if r.skipped)
        print()
        print(f"Results: {passed} passed, {failed} failed, {skipped} skipped out of {total} tests")
        return failed == 0

    # ------------------------------------------------------------------
    #  Helper assertions
    # ------------------------------------------------------------------

    def assert_wled_on(self, result: TestResult, expected: bool):
        """Assert that WLED's 'on' state matches expected."""
        state = self.wled.get_state()
        actual = state.get("on", None)
        result.add_detail(f"WLED on={actual}, expected={expected}")
        assert actual == expected, f"WLED on={actual}, expected {expected}"

    def assert_wled_bri_approx(self, result: TestResult, expected: int, tolerance: int = 10):
        """Assert WLED brightness is approximately expected (Zigbee 0-254 -> WLED 0-255)."""
        state = self.wled.get_state()
        actual = state.get("bri", -1)
        result.add_detail(f"WLED bri={actual}, expected~={expected} (+/-{tolerance})")
        assert abs(actual - expected) <= tolerance, \
            f"WLED bri={actual}, expected ~{expected} (+/-{tolerance})"

    def assert_wled_color_approx(self, result: TestResult,
                                  expected_r: int, expected_g: int, expected_b: int,
                                  tolerance: int = 40):
        """Assert WLED primary color is approximately expected RGB."""
        state = self.wled.get_state()
        # WLED state has seg[0].col[0] = [R, G, B, W]
        segs = state.get("seg", [])
        if not segs:
            raise AssertionError("No segments in WLED state")
        col = segs[0].get("col", [[0, 0, 0]])[0]
        r, g, b = col[0], col[1], col[2]
        result.add_detail(
            f"WLED RGB=({r},{g},{b}), expected~=({expected_r},{expected_g},{expected_b}) "
            f"+/-{tolerance}")
        assert abs(r - expected_r) <= tolerance, \
            f"Red: got {r}, expected ~{expected_r}"
        assert abs(g - expected_g) <= tolerance, \
            f"Green: got {g}, expected ~{expected_g}"
        assert abs(b - expected_b) <= tolerance, \
            f"Blue: got {b}, expected ~{expected_b}"

    def assert_hue_reachable(self, result: TestResult):
        state = self.hue.get_light_state(self.cfg.light_id)
        reachable = state.get("reachable", False)
        result.add_detail(f"Hue reachable={reachable}")
        assert reachable, "Hue light is not reachable"

    # ------------------------------------------------------------------
    #  Test methods
    # ------------------------------------------------------------------

    def test_prerequisites(self, result: TestResult):
        """Verify both APIs are reachable before running tests."""
        # Check WLED
        assert self.wled.is_reachable(), \
            f"WLED device at {self.cfg.wled_ip} is not reachable"
        result.add_detail(f"WLED at {self.cfg.wled_ip} -- OK")

        # Check Hue bridge
        try:
            light = self.hue.get_light(self.cfg.light_id)
            result.add_detail(
                f"Hue light #{self.cfg.light_id}: "
                f"{light.get('name', '?')}, "
                f"type={light.get('type', '?')}, "
                f"reachable={light.get('state', {}).get('reachable', '?')}")
        except Exception as e:
            raise AssertionError(
                f"Cannot reach Hue bridge at {self.cfg.bridge_ip}: {e}")

    def test_hue_light_reachable(self, result: TestResult):
        """Verify the Zigbee light is marked as reachable on the Hue bridge."""
        self.assert_hue_reachable(result)

    # ------------------------------------------------------------------
    #  WLED HTTP connectivity tests
    # ------------------------------------------------------------------

    def test_wled_http_tcp_connect(self, result: TestResult):
        """Verify TCP connection to WLED port 80 succeeds.

        This is the most basic network connectivity test — if this fails,
        the device is not reachable at all (WiFi/coex issue).
        """
        # Try ICMP ping first (informational, may fail without raw socket perms)
        try:
            ping = subprocess.run(
                ["ping", "-c", "2", "-W", "2", self.cfg.wled_ip],
                capture_output=True, text=True, timeout=10)
            result.add_detail(f"ICMP ping: rc={ping.returncode}")
            if ping.returncode == 0:
                # Extract round-trip time from ping output
                for line in ping.stdout.splitlines():
                    if "rtt" in line or "round-trip" in line:
                        result.add_detail(f"  {line.strip()}")
            else:
                result.add_detail(f"  ping failed (may be filtered, trying TCP)")
        except Exception as e:
            result.add_detail(f"ICMP ping skipped: {e}")

        # TCP connect to port 80 — this is the definitive test
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        try:
            sock.connect((self.cfg.wled_ip, 80))
            result.add_detail(f"TCP connect to {self.cfg.wled_ip}:80 -- OK")
        except (socket.timeout, ConnectionRefusedError, OSError) as e:
            raise AssertionError(
                f"TCP connect to {self.cfg.wled_ip}:80 failed: {e}. "
                f"Device is not reachable — likely a WiFi/coex issue.")
        finally:
            sock.close()

    def test_wled_http_json_state(self, result: TestResult):
        """Verify WLED /json/state returns valid JSON with expected fields."""
        try:
            state = self.wled.get_state()
        except Exception as e:
            raise AssertionError(f"GET /json/state failed: {e}")

        # Verify essential fields exist
        assert "on" in state, f"'on' field missing from state: {list(state.keys())}"
        assert "bri" in state, f"'bri' field missing from state: {list(state.keys())}"
        assert "seg" in state, f"'seg' field missing from state: {list(state.keys())}"
        result.add_detail(f"on={state['on']}, bri={state['bri']}, "
                          f"segs={len(state['seg'])}")

    def test_wled_http_json_info(self, result: TestResult):
        """Verify WLED /json/info returns valid JSON with expected fields."""
        try:
            info = self.wled.get_info()
        except Exception as e:
            raise AssertionError(f"GET /json/info failed: {e}")

        assert "ver" in info, f"'ver' field missing from info: {list(info.keys())}"
        assert "arch" in info, f"'arch' field missing from info: {list(info.keys())}"
        result.add_detail(f"ver={info.get('ver')}, arch={info.get('arch')}, "
                          f"freeheap={info.get('freeheap')}")

        # Verify WiFi info is present
        wifi = info.get("wifi", {})
        result.add_detail(f"WiFi: rssi={wifi.get('rssi')}, signal={wifi.get('signal')}, "
                          f"channel={wifi.get('channel')}")

    def test_wled_http_win_endpoint(self, result: TestResult):
        """Verify WLED /win endpoint responds to HTTP GET."""
        try:
            r = requests.get(f"http://{self.cfg.wled_ip}/win&T=2",
                             timeout=5)
            r.raise_for_status()
            result.add_detail(f"/win response: status={r.status_code}, "
                              f"len={len(r.content)}")
        except Exception as e:
            raise AssertionError(f"GET /win failed: {e}")

    def test_wled_info_zigbee_fields(self, result: TestResult):
        """Verify WLED info JSON contains Zigbee debug fields."""
        info = self.wled.get_info()
        # The usermod adds fields under "u" (user info)
        user = info.get("u", {})
        result.add_detail(f"User info keys: {list(user.keys())}")

        # Should have ZigbeeRGBLight status (either "paired" or "searching...")
        assert "ZigbeeRGBLight" in user, \
            f"ZigbeeRGBLight not in user info. Keys: {list(user.keys())}"
        result.add_detail(f"ZigbeeRGBLight: {user['ZigbeeRGBLight']}")

        # Should have ZB Parent (short address of parent node)
        assert "ZB Parent" in user, "ZB Parent debug field not found"
        result.add_detail(f"ZB Parent: {user['ZB Parent']}")

        # Should have ZB Binds (N/3 binding count)
        assert "ZB Binds" in user, "ZB Binds debug field not found"
        result.add_detail(f"ZB Binds: {user['ZB Binds']}")

        # Should have ZB Reports T/S (trigger/scheduler counters)
        assert "ZB Reports T/S" in user, "ZB Reports T/S debug field not found"
        result.add_detail(f"ZB Reports T/S: {user['ZB Reports T/S']}")

    def test_hue_on(self, result: TestResult):
        """Send ON via Hue API, verify WLED turns on."""
        self.hue.set_light_state(self.cfg.light_id, {"on": True, "bri": 200})
        self.settle()
        self.assert_wled_on(result, True)

    def test_hue_off(self, result: TestResult):
        """Send OFF via Hue API, verify WLED turns off."""
        # Ensure it's on first
        self.hue.set_light_state(self.cfg.light_id, {"on": True, "bri": 200})
        self.settle()
        # Now turn off
        self.hue.set_light_state(self.cfg.light_id, {"on": False})
        self.settle()
        self.assert_wled_on(result, False)

    def test_hue_on_off_cycle(self, result: TestResult):
        """Rapid on/off/on cycle to test reliability."""
        for i, on_state in enumerate([True, False, True, False, True]):
            self.hue.set_light_state(self.cfg.light_id,
                                      {"on": on_state} if not on_state
                                      else {"on": on_state, "bri": 200})
            self.settle()
            self.assert_wled_on(result, on_state)
            result.add_detail(f"Cycle {i+1}: on={on_state} -- OK")

    def test_hue_brightness(self, result: TestResult):
        """Set brightness via Hue API, verify WLED brightness matches."""
        self.hue.set_light_state(self.cfg.light_id, {"on": True, "bri": 127})
        self.settle()
        self.assert_wled_on(result, True)
        self.assert_wled_bri_approx(result, 127, tolerance=15)

    def test_hue_brightness_range(self, result: TestResult):
        """Test several brightness levels across the full range."""
        test_levels = [1, 50, 127, 200, 254]
        for bri in test_levels:
            self.hue.set_light_state(self.cfg.light_id, {"on": True, "bri": bri})
            self.settle()
            self.assert_wled_bri_approx(result, bri, tolerance=15)
            result.add_detail(f"bri={bri} -- OK")

    def test_hue_brightness_min(self, result: TestResult):
        """Set brightness to minimum (1) via Hue, verify WLED is on and dim."""
        self.hue.set_light_state(self.cfg.light_id, {"on": True, "bri": 1})
        self.settle()
        self.assert_wled_on(result, True)
        self.assert_wled_bri_approx(result, 1, tolerance=5)

    def test_hue_color_red(self, result: TestResult):
        """Set red via Hue XY, verify WLED shows red."""
        # CIE xy for red: approximately (0.675, 0.322)
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 254, "xy": [0.675, 0.322]})
        self.settle(1.5)  # color commands sometimes arrive slightly delayed
        self.assert_wled_color_approx(result, 255, 0, 0, tolerance=60)

    def test_hue_color_green(self, result: TestResult):
        """Set green via Hue XY, verify WLED shows green."""
        # CIE xy for green: approximately (0.17, 0.70)
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 254, "xy": [0.17, 0.70]})
        self.settle(1.5)
        self.assert_wled_color_approx(result, 0, 255, 0, tolerance=60)

    def test_hue_color_blue(self, result: TestResult):
        """Set blue via Hue XY, verify WLED shows blue."""
        # CIE xy for blue: approximately (0.15, 0.06)
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 254, "xy": [0.15, 0.06]})
        self.settle(1.5)
        # Blue via XY may have some purple tint due to gamut mapping
        self.assert_wled_color_approx(result, 50, 0, 255, tolerance=80)

    def test_hue_color_white(self, result: TestResult):
        """Set white (D65) via Hue XY, verify WLED shows near-white."""
        # CIE xy for D65 white: (0.3127, 0.3290)
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 254, "xy": [0.3127, 0.3290]})
        self.settle(1.5)
        # White should have all channels high
        state = self.wled.get_state()
        segs = state.get("seg", [])
        if not segs:
            raise AssertionError("No segments")
        col = segs[0].get("col", [[0, 0, 0]])[0]
        r, g, b = col[0], col[1], col[2]
        result.add_detail(f"WLED RGB for white: ({r},{g},{b})")
        # All channels should be relatively high (> 180) for white
        assert r > 180, f"Red channel too low for white: {r}"
        assert g > 180, f"Green channel too low for white: {g}"
        assert b > 180, f"Blue channel too low for white: {b}"

    def test_hue_color_yellow(self, result: TestResult):
        """Set yellow via Hue XY, verify WLED shows yellow."""
        # CIE xy for yellow: approximately (0.42, 0.50)
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 254, "xy": [0.42, 0.50]})
        self.settle(1.5)
        # Yellow = high red + high green, low blue
        state = self.wled.get_state()
        segs = state.get("seg", [])
        if not segs:
            raise AssertionError("No segments")
        col = segs[0].get("col", [[0, 0, 0]])[0]
        r, g, b = col[0], col[1], col[2]
        result.add_detail(f"WLED RGB for yellow: ({r},{g},{b})")
        assert r > 180, f"Red too low for yellow: {r}"
        assert g > 150, f"Green too low for yellow: {g}"
        assert b < 80, f"Blue too high for yellow: {b}"

    def test_hue_color_cyan(self, result: TestResult):
        """Set cyan via Hue XY, verify WLED shows cyan."""
        # CIE xy for cyan: approximately (0.15, 0.34)
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 254, "xy": [0.15, 0.34]})
        self.settle(1.5)
        # Cyan = low red, high green + blue
        state = self.wled.get_state()
        segs = state.get("seg", [])
        if not segs:
            raise AssertionError("No segments")
        col = segs[0].get("col", [[0, 0, 0]])[0]
        r, g, b = col[0], col[1], col[2]
        result.add_detail(f"WLED RGB for cyan: ({r},{g},{b})")
        assert r < 100, f"Red too high for cyan: {r}"
        assert g > 150, f"Green too low for cyan: {g}"
        assert b > 150, f"Blue too low for cyan: {b}"

    def test_hue_color_magenta(self, result: TestResult):
        """Set magenta via Hue XY, verify WLED shows magenta."""
        # CIE xy for magenta: approximately (0.35, 0.15)
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 254, "xy": [0.35, 0.15]})
        self.settle(1.5)
        # Magenta = high red + blue, low green
        state = self.wled.get_state()
        segs = state.get("seg", [])
        if not segs:
            raise AssertionError("No segments")
        col = segs[0].get("col", [[0, 0, 0]])[0]
        r, g, b = col[0], col[1], col[2]
        result.add_detail(f"WLED RGB for magenta: ({r},{g},{b})")
        assert r > 150, f"Red too low for magenta: {r}"
        assert g < 100, f"Green too high for magenta: {g}"
        assert b > 150, f"Blue too low for magenta: {b}"

    def test_hue_combined_on_bri_color(self, result: TestResult):
        """Send combined on+bri+color in a single Hue API call."""
        # Turn off first
        self.hue.set_light_state(self.cfg.light_id, {"on": False})
        self.settle()

        # Combined command: turn on, set brightness to 180, set to red
        self.hue.set_light_state(self.cfg.light_id,
                                  {"on": True, "bri": 180, "xy": [0.675, 0.322]})
        self.settle(2.0)  # Hue batches these as separate ZCL commands
        self.assert_wled_on(result, True)
        self.assert_wled_bri_approx(result, 180, tolerance=20)

    def test_wled_http_after_zigbee_traffic(self, result: TestResult):
        """Verify WLED HTTP API still works after heavy Zigbee traffic.

        This is the key coexistence test: the 802.15.4 radio has been active
        for all the Hue->WLED tests above. If the coex arbiter is misconfigured
        or WiFi has been starved, HTTP requests will timeout here.
        """
        # Try multiple endpoints to be thorough
        endpoints = [
            ("/json/state", "JSON state"),
            ("/json/info", "JSON info"),
            ("/win&T=2", "win endpoint"),
        ]
        for path, desc in endpoints:
            try:
                t0 = time.time()
                r = requests.get(f"http://{self.cfg.wled_ip}{path}", timeout=5)
                elapsed = time.time() - t0
                r.raise_for_status()
                result.add_detail(f"{desc}: status={r.status_code}, "
                                  f"time={elapsed:.2f}s, len={len(r.content)}")
                if elapsed > 3.0:
                    result.add_detail(f"  WARNING: {desc} took {elapsed:.2f}s (>3s)")
            except Exception as e:
                raise AssertionError(
                    f"HTTP {desc} failed after Zigbee traffic: {e}. "
                    f"WiFi may be starved by 802.15.4 coexistence.")

        # Also verify we can POST state (write, not just read)
        try:
            t0 = time.time()
            r = requests.post(f"http://{self.cfg.wled_ip}/json/state",
                              json={"on": True, "bri": 128}, timeout=5)
            elapsed = time.time() - t0
            r.raise_for_status()
            result.add_detail(f"POST /json/state: status={r.status_code}, "
                              f"time={elapsed:.2f}s")
        except Exception as e:
            raise AssertionError(f"POST /json/state failed after Zigbee traffic: {e}")

    def test_rapid_color_changes(self, result: TestResult):
        """Send rapid color changes via Hue, verify WLED ends at final color.

        This tests reliability under rapid command bursts, which exercises
        the coexistence arbiter and End Device polling.
        """
        colors = [
            ([0.675, 0.322], "red"),
            ([0.17, 0.70], "green"),
            ([0.15, 0.06], "blue"),
            ([0.42, 0.50], "yellow"),
            ([0.3127, 0.3290], "white"),
        ]
        # Send all colors rapidly with minimal delay
        for xy, name in colors:
            self.hue.set_light_state(self.cfg.light_id,
                                      {"on": True, "bri": 254, "xy": xy})
            time.sleep(0.5)  # minimal gap between commands

        # Wait for everything to settle
        self.settle(2.0)

        # The final color was white — verify WLED shows white
        state = self.wled.get_state()
        segs = state.get("seg", [])
        if not segs:
            raise AssertionError("No segments")
        col = segs[0].get("col", [[0, 0, 0]])[0]
        r, g, b = col[0], col[1], col[2]
        result.add_detail(f"WLED RGB after rapid changes: ({r},{g},{b})")
        # After all rapid changes, should end at white (all channels high)
        assert r > 160, f"Red too low after rapid changes: {r}"
        assert g > 160, f"Green too low after rapid changes: {g}"
        assert b > 160, f"Blue too low after rapid changes: {b}"
        result.add_detail("Rapid color changes settled to white -- OK")

    def test_wled_http_sustained(self, result: TestResult):
        """Make repeated HTTP requests over 10 seconds to detect WiFi degradation.

        After all Zigbee tests have run, the 802.15.4 radio has been heavily
        used. This test verifies WiFi connectivity remains stable by making
        10 requests over 10 seconds and checking that all succeed with
        reasonable latency.
        """
        successes = 0
        failures = 0
        max_time = 0.0
        total_time = 0.0

        for i in range(10):
            try:
                t0 = time.time()
                r = requests.get(f"http://{self.cfg.wled_ip}/json/state",
                                 timeout=5)
                elapsed = time.time() - t0
                r.raise_for_status()
                successes += 1
                total_time += elapsed
                if elapsed > max_time:
                    max_time = elapsed
            except Exception:
                failures += 1
            time.sleep(1.0)

        avg_time = total_time / successes if successes > 0 else 0
        result.add_detail(
            f"10 requests: {successes} OK, {failures} failed, "
            f"avg={avg_time:.2f}s, max={max_time:.2f}s")

        assert failures == 0, \
            f"{failures}/10 HTTP requests failed — WiFi degraded under Zigbee coex"
        assert max_time < 4.0, \
            f"Slowest request took {max_time:.2f}s (>4s) — WiFi latency too high"


# ---------------------------------------------------------------------------
#  Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Integration tests for WLED Zigbee RGB Light usermod",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument("--bridge-ip", required=True,
                        help="Philips Hue Bridge IP address")
    parser.add_argument("--api-key", required=True,
                        help="Hue Bridge API key")
    parser.add_argument("--light-id", required=True,
                        help="Hue light ID for the WLED Zigbee device")
    parser.add_argument("--wled-ip", required=True,
                        help="WLED device IP address")
    parser.add_argument("--settle", type=float, default=3.0,
                        help="Seconds to wait after commands (default: 3.0)")
    parser.add_argument("-k", "--filter",
                        help="Only run tests whose name contains this string")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Show detailed info for each test")

    args = parser.parse_args()

    cfg = Config(
        bridge_ip=args.bridge_ip,
        api_key=args.api_key,
        light_id=args.light_id,
        wled_ip=args.wled_ip,
        settle_time=args.settle,
        verbose=args.verbose,
    )

    print(f"WLED Zigbee RGB Light -- Integration Tests")
    print(f"  Hue bridge: {cfg.bridge_ip} (light #{cfg.light_id})")
    print(f"  WLED:       {cfg.wled_ip}")
    print(f"  Settle:     {cfg.settle_time}s")
    print()

    runner = TestRunner(cfg)
    if args.filter:
        runner.set_filter(args.filter)
        print(f"  Filter: *{args.filter}*")
        print()

    success = runner.run_all()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
