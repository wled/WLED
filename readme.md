<p align="center">
  <!-- Optional: replace with your fork's logo -->
  <img src="/images/wled_enterprise_logo.png" alt="WLED Enterprise Wi-Fi">
  <!-- Badges: update org/repo as needed -->
  <a href="https://github.com/<your-org>/<your-repo>/releases"><img src="https://img.shields.io/github/release/<your-org>/<your-repo>.svg?style=flat-square"></a>
  <a href="https://raw.githubusercontent.com/<your-org>/<your-repo>/main/LICENSE"><img src="https://img.shields.io/github/license/<your-org>/<your-repo>?color=blue&style=flat-square"></a>
  <a href="https://kno.wled.ge"><img src="https://img.shields.io/badge/quick_start-wiki-blue.svg?style=flat-square" alt="Quick Start"></a>
</p>

# WLED + WPA2-Enterprise (802.1X)

A focused fork of WLED that adds **WPA2-Enterprise (802.1X)** support and integrates enterprise authentication into the existing **Wi-Fi Settings** workflow. Configure **PEAP/MSCHAPv2**, **TTLS/PAP**, optional **server CA validation**, and related fields directly in the UI while preserving the familiar WLED experience.

Originally based on [WLED by Aircoookie](https://github.com/wled-dev/WLED).

---

## Features

- **Enterprise mode** in Wi-Fi Settings
- Fields for **SSID**, **Outer Identity** (optional), **Username**, **Password**
- **EAP methods:** PEAP, TTLS
- **Phase-2 methods:** MSCHAPV2, PAP
- Optional **server CA validation** with **Upload/Delete** certificate actions
- **SSID synchronization** with the primary network entry (works after **Scan** and “Other network…”)
- Preserves stock **Scan** behavior and all original Wi-Fi options (DNS, mDNS, AP mode, static IP, etc.)

---

## Screenshots

<p align="center">
  <img src="/images/enterprise-toggle.png" width="49%" alt="Enterprise Toggle">
  <img src="/images/enterprise-fields.png" width="49%" alt="Enterprise Fields">
</p>

> Replace screenshots with your actual UI captures.

---

## Quick Start

See [WLED docs](https://kno.wled.ge) for general device setup and flashing guidance. This fork introduces changes to the **Wi-Fi Settings** page and adds a small HTTP API for enterprise configuration.

### Building & Installing

This fork supports both UI pipelines used in WLED. Choose the path your build environment uses.

#### Option A — Filesystem UI (LittleFS/SPIFFS)

1. Place the modified page:
```

wled00/data/settings\_wifi.htm

````
2. Build & upload filesystem (PlatformIO):
```bash
pio run -e <your_env> -t buildfs
pio run -e <your_env> -t uploadfs
````

3. Flash firmware (if needed):

   ```bash
   pio run -e <your_env> -t upload
   ```
4. Hard reload the browser (Ctrl/Cmd+Shift+R).

> Note: OTA firmware updates do **not** update filesystem files. Re-run `uploadfs` after UI changes.

#### Option B — Compiled UI (PROGMEM)

1. Place the modified page:

   ```
   wled00/html/settings_wifi.htm
   ```
2. Generate headers:

   ```bash
   python3 tools/cdata.py
   ```
3. Build & flash:

   ```bash
   pio run -e <your_env> -t upload
   ```
4. Hard reload the browser.

---

## Using WPA2-Enterprise in the UI

1. Open **WLED → Wi-Fi Settings**.
2. Click **Scan** and select your enterprise SSID (or choose **Other network…** to enter it).
3. Enable **WPA2-Enterprise (802.1X)**.
4. Fill enterprise credentials:

   * **Outer Identity** (optional; often left blank unless required by your network)
   * **Username**, **Password**
   * **EAP Method** (commonly **PEAP**)
   * **Phase-2** (commonly **MSCHAPV2**)
   * **Validate server CA** if your organization requires it; **Upload** CA certificate when needed.
5. Click **Save & Connect**. The device restarts and attempts 802.1X association.

> Typical campus/corporate setups use **PEAP + MSCHAPV2**.

---

## HTTP API (optional)

Automate enterprise configuration via simple endpoints.

* **Get config**

  ```bash
  curl http://<device>/eap/get
  ```
* **Save config**

  ```bash
  curl -X POST http://<device>/eap/save \
    -H "Content-Type: application/json" \
    -d '{
      "enabled": true,
      "ssid": "CorpWiFi",
      "identity": "",
      "username": "user@org",
      "password": "secret",
      "method": "PEAP",
      "phase2": "MSCHAPV2",
      "validateCa": false
    }'
  ```
* **Upload CA**

  ```bash
  curl -X POST http://<device>/eap/upload_ca -F "file=@/path/to/ca.pem"
  ```
* **Delete CA**

  ```bash
  curl -X POST http://<device>/eap/delete_ca
  ```

---

## Compatibility

* **Target:** ESP32 is recommended. WPA2-Enterprise on ESP8266 is limited or unsupported depending on SDK/toolchain.
* **EAP methods:** Username/password profiles (PEAP, TTLS) are supported in the UI; certificate-based methods are not covered by this fork.
* **WLED versions:** Built against recent WLED code paths using `settings_wifi.htm` and `/json/net`. If your base differs, update selectors/IDs accordingly.

---

## Differences vs Upstream WLED

* Adds an **Enterprise mode** section to Wi-Fi Settings
* Adds endpoints: `/eap/get`, `/eap/save`, `/eap/upload_ca`, `/eap/delete_ca`
* Integrates **CA validation** handling (optional)
* Keeps upstream Wi-Fi configuration semantics (DNS, mDNS, AP, static IP) unchanged

Everything else strives to remain consistent with upstream behavior and UI.

---

## Troubleshooting

* **UI changes don’t appear**

  * For LittleFS/SPIFFS builds: run `buildfs` + `uploadfs`.
  * For compiled UI: run `tools/cdata.py` then rebuild/flash.
  * Hard reload; WLED serves strong cache headers.

* **Scan button no-ops**

  * Ensure `common.js` is loaded (use `<script src="/common.js" defer></script>`).
  * Check browser Console for missing globals (e.g., `getURL`, `scanLoops`).

* **Enterprise association fails**

  * Verify EAP/Phase-2 match your network policy.
  * Try without CA validation; if it connects, re-enable with the correct CA certificate.
  * Some networks require a specific **Outer Identity** (e.g., `anonymous@realm`).

---

## Acknowledgments

* Upstream project: **WLED** by [Aircoookie](https://github.com/wled-dev/WLED) and contributors.
* README structure inspired by the upstream WLED README.&#x20;

---

## License

This fork follows the same license as the upstream project (see [LICENSE](./LICENSE)).
