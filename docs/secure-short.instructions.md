---
applyTo: "**/*.{cpp,h,hpp,ino,js,htm,html,css,yml,yaml}"
description: "WLED strict-mode security review: low-noise 26-rule checklist."
---

# WLED Security Review — Strict Mode (Low Noise)

Use these compact rules for automated reviews with minimal false positives.

## WLED Constraints (apply to all rules)

- Assume firewall/DMZ/VPN deployment; focus on realistic LAN-local and supply-chain risks.
- Do **not** require TLS/HTTPS as a baseline control.
- Do **not** require authentication for standards-based UDP multicast/broadcast paths where authentication is not defined in the protocol specification.

## CRITICAL Rules

1. **No unchecked buffer copies** (`memcpy`, `strcpy`, `sprintf`) in firmware paths.
2. **No user-controlled format strings** in `DEBUG_PRINTF*` and similar logging APIs.
3. **Validate all external input** (HTTP/JSON/UDP/serial) before index/length/pin usage.
4. **Auth required for state-changing control endpoints where feasible** (for example HTTP/JSON); do not flag protocol-defined unauthenticated UDP multicast/broadcast channels solely for missing auth.
5. **No fail-open on parse/allocation errors** for config/state updates.
6. **No DOM XSS sinks with untrusted data** (`innerHTML`, unsafe HTML insertion).
7. **No dynamic code execution** (`eval`, `new Function`, string timers).
8. **No hardcoded secrets/credentials/tokens/keys** in committed files.
9. **No sensitive data in logs** (passwords, tokens, Wi-Fi secrets, auth headers).
10. **No secret exposure in workflows/log output, or in LittleFS files other than ``wsec.json``**.
11. **No unsafe third-party GitHub Action pinning** (`@main`/`@master` disallowed).
12. **No untrusted expression interpolation in workflow shell commands**.

## IMPORTANT Rules

13. Check integer overflow risks in size/index arithmetic.
14. Reject repeated heap allocation churn in hot render/effect loops.
15. Avoid repeated `String` growth in hot paths; prefer bounded/pre-allocated buffers.
16. Ensure UI validation is mirrored by firmware-side validation.
17. Require strict origin checks for `postMessage` listeners.
18. Disallow untrusted redirect/navigation targets.
19. Prevent verbose error responses that leak internals.
20. Review new dependencies for typosquatting and known vulnerability risk.
21. Keep workflow `permissions` least-privilege.
22. Verify new `WLED_ENABLE_*` / `WLED_DISABLE_*` names are valid known flags.
23. New privileged behavior must not be enabled by insecure defaults; first-use default-credential change required where applicable.
24. OTA paths (Update.begin(), Update.write()) must verify firmware integrity (checksum/hash); TLS not required.
25. Flag xTaskCreate/xTaskCreatePinnedToCore tasks with insufficient stack for String/JSON use; flag MDNS.begin() / ArduinoOTA.setHostname() with unsanitized hostnames.
26. Flag API/config serialization that exposes Wi-Fi/AP/MQTT password fields to unauthenticated clients.

## Reviewer Output Format

- Report only findings mapped to rules 1–26.
- Include severity, exact file and line, and one concrete fix direction.
- Prioritize CRITICAL findings before IMPORTANT findings.
