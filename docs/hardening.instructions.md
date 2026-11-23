---
applyTo: "**/*.{cpp,h,hpp,ino,js,htm,html,css,yml,yaml}"
description: "WLED strict-mode security review: low-noise checklist."
---

# WLED Security Review — Low Noise Mode

Use these code hardening rules for automated reviews with minimal false positives.

## WLED Constraints (apply to all rules)

- Assume firewall/DMZ/VPN deployment; focus on realistic LAN-local and supply-chain risks.
- Do **not** require TLS/HTTPS as a baseline control.
- Do **not** require authentication for standards-based UDP multicast/broadcast paths where authentication is not defined in the protocol specification.

> **Trust boundary model**: Apply input-validation rules **only at the first untrusted ingress point**
> (HTTP/JSON API body or query string, WebSocket payload, UDP datagram, TCP read, serial command, ESP-NOW raw messages).
> Values that have been validated and range-clamped at ingress are **trusted** for internal WLED
> processing. Do not flag subsequent uses or internal copies of already-sanitized data.

## CRITICAL Rules

1. **No unchecked buffer copies** (`memcpy`, `memmove`, `strcpy`) in firmware paths when source buffer or size comes from an untrusted origin; prefer bounded alternatives (`strncpy`, `strlcpy`); require length validation before copying.
2. **No user-controlled format strings** in `DEBUG_PRINTF*` and similar logging APIs.
3. **Validate all untrusted external input** (HTTP/JSON/UDP/serial) before index/length/pin usage.
4. **Auth required for state-changing control endpoints where feasible** (for example HTTP/JSON); do not flag protocol-defined unauthenticated UDP multicast/broadcast channels solely for missing auth.
5. **No fail-open on parse/allocation errors** for config/state updates.
6. **No DOM XSS sinks with untrusted data** (`innerHTML`, unsafe HTML insertion). Server-side generation of JavaScript property-assignment statements (as used in WLED's printSetForm* helpers) is exempt.
7. **No dynamic code execution** (`eval`, `new Function`, string timers).
8. **No hardcoded secrets/credentials/tokens/keys** in committed files.
9. **No sensitive data in logs** (passwords, tokens, Wi-Fi secrets, auth headers).
10. **No secret exposure in workflows/log output, or in LittleFS files other than `wsec.json`**.
11. **No unsafe third-party GitHub Action pinning** (`@main`/`@master` disallowed).
12. **No untrusted expression interpolation in workflow shell commands**.

## IMPORTANT Rules

13. Avoid potentially unbounded string/memory operations (`strcmp`, `strchr`, `strlen`, `sprintf`) in firmware paths; prefer bounded alternatives (`strnlen`, `strncmp`, `snprintf`).
14. Check integer overflow risks in size/index arithmetic, but consider that unsigned wrap-around on small types might be intentional. 
15. Reject repeated heap allocation churn in hot render/effect loops.
16. Avoid repeated `String` growth in hot paths; prefer bounded/pre-allocated buffers.
17. Ensure UI validation is mirrored by firmware-side validation.
18. Require strict origin checks for `postMessage` listeners.
19. Disallow untrusted redirect/navigation targets.
20. Prevent verbose error responses that leak internals.
21. Review new dependencies for typosquatting and known vulnerability risk.
22. Keep workflow `permissions` least-privilege.
23. Verify new `WLED_ENABLE_*` / `WLED_DISABLE_*` names are valid known flags.
24. New privileged behavior must not be enabled by insecure defaults; first-use default-credential change required where applicable.
25. OTA paths (Update.begin(), Update.write()) must verify firmware integrity (checksum/hash); TLS not required.
26. Flag xTaskCreate/xTaskCreatePinnedToCore tasks with insufficient stack for String/JSON use; flag MDNS.begin() / ArduinoOTA.setHostname() with unsanitized hostnames.
27. Flag API/config serialization that exposes Wi-Fi/AP/MQTT password fields to unauthenticated clients.
28. Treat fetched and config-derived strings as untrusted when inserting into the DOM; explicit sanitization required for HTML contexts.

## Reviewer Output Format

- Include severity, exact file and line, and one concrete fix direction.
- Prioritize CRITICAL findings before IMPORTANT findings.
