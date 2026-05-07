---
applyTo: "**/*.{cpp,h,hpp,ino,js,htm,html,css,yml,yaml}"
description: "WLED-focused security review guide based on OWASP Top 10 for embedded firmware and web UI."
---

# WLED Security Review Standards (Embedded + Web UI)

Use this guide for AI-assisted code reviews in:
- `/home/runner/work/WLED/WLED/wled00/`
- `/home/runner/work/WLED/WLED/usermods/`
- `/home/runner/work/WLED/WLED/.github/workflows/`

Ignore sections wrapped in `<!-- HUMAN_ONLY_START --> ... <!-- HUMAN_ONLY_END -->` in repo docs when applying review criteria.

## WLED Constraints and Threat Model Assumptions

- Assume typical deployment behind a firewall/DMZ/VPN; prioritize LAN-local and supply-chain risks.
- Do **not** require TLS/HTTPS as a baseline control for findings in this repo.
- Do **not** require authentication for standards-based UDP multicast/broadcast protocols where auth is not part of the spec.
- Do not propose mitigations that break protocol compliance just to add authentication.

## Severity

- **CRITICAL** — exploitable vulnerability; block merge.
- **IMPORTANT** — meaningful risk; fix before or with merge when practical.
- **SUGGESTION** — defense-in-depth; track for follow-up.

## Scope (WLED-relevant)

Prioritize:
- C++ memory safety and input validation
- Auth and access checks for state-changing HTTP/JSON APIs
- XSS and DOM safety in `wled00/data/*`
- Secrets handling and secure logging
- Dependency and GitHub Actions supply-chain hygiene
- Fail-safe behavior on constrained devices

De-prioritize unless explicitly introduced by a PR:
- SQL/NoSQL checks, JWT/OAuth flows, GraphQL-specific checks, generic backend framework checks not used by WLED.

## Firmware Security (C++, OWASP A01/A04/A05/A10)

### FW1: Unsafe buffer operations
- **Severity**: CRITICAL
- Flag `strcpy`, `sprintf`, unchecked `memcpy`, unchecked pointer arithmetic.
- Require explicit bounds checks and length validation.

### FW2: Format-string injection
- **Severity**: CRITICAL
- Do not pass untrusted input as a format string to `DEBUG_PRINTF*` or similar APIs.

### FW3: Integer overflow in length and offset math
- **Severity**: IMPORTANT
- Review `count * size`, index math, narrowing casts before allocations or copies.

### FW4: Unvalidated external input
- **Severity**: CRITICAL
- Validate and clamp external values from HTTP/JSON/UDP/serial before use as lengths, indices, IDs, or pin references.
- In UDP handlers (`parsePacket()`, `recvfrom()`), validate `packetSize` before buffer writes and clamp protocol-specific universe/channel ranges to valid limits.

### FW5: Missing auth checks on state-changing endpoints (where auth is feasible)
- **Severity**: CRITICAL
- HTTP/JSON and other control paths that support auth must enforce configured auth policy.
- Do not flag standards-based UDP multicast/broadcast paths solely for lacking authentication when authentication is not defined in the protocol specification.

### FW6: Fail-open behavior after parse or allocation errors
- **Severity**: IMPORTANT
- On error, reject update and preserve safe previous state.
- Explicitly check parse status (`deserializeJson()`/equivalent) and avoid silently applying unsafe zero/default values to safety-relevant fields (for example LED count and pin assignment).

### FW7: Heap churn in hot paths
- **Severity**: IMPORTANT
- Avoid repeated dynamic allocation in render/effect loops; prefer pre-allocation and reuse.
- Flag allocation patterns in loop and ISR-adjacent paths that can trigger fragmentation or timing instability.

### FW8: Unsafe use of `String` in performance-critical paths
- **Severity**: IMPORTANT
- In hot paths, avoid repeated `String` growth; reserve or use fixed buffers.
- Flag repeated `String` concatenation inside loop-heavy or ISR-adjacent code.

### FW9: Unsafe feature flag names
- **Severity**: IMPORTANT
- Verify all new `WLED_ENABLE_*`/`WLED_DISABLE_*` names are valid known flags; typos silently alter build behavior.

### FW10: OTA integrity verification (without TLS requirement)
- **Severity**: IMPORTANT
- OTA update flows should validate firmware integrity using the checksum/hash/signature mechanism available in the firmware/platform implementation.
- Do not require TLS/certificate pinning as a mandatory review criterion.
- In OTA paths (`Update.begin()`, `Update.write()`, and related flows), flag flashing without integrity verification.

### FW11: FreeRTOS task stack and recursion safety
- **Severity**: IMPORTANT
- In `xTaskCreate`/`xTaskCreatePinnedToCore` tasks that process `String`/JSON-heavy data, verify stack-size sufficiency and avoid unbounded recursion.

### FW12: mDNS and hostname sanitization
- **Severity**: IMPORTANT
- For `MDNS.begin()`, `MDNS.addService()`, and `ArduinoOTA.setHostname()`, ensure user-provided hostnames are restricted to RFC-compliant characters (`[a-zA-Z0-9-]`) and clamped to 63 characters.

### FW13: Outbound URL validation (no HTTPS requirement)
- **Severity**: SUGGESTION
- When using user-provided URL strings with `HTTPClient.begin()`/equivalent, validate scheme/format and constrain host targets (allowlist or equivalent policy).
- Do not require HTTPS/TLS as a baseline review rule.

### FW14: Optional unicast UDP source filtering
- **Severity**: SUGGESTION
- For unicast UDP receive paths, prefer optional user-configurable source filtering.
- Do not require this for multicast/broadcast protocol flows.

## Web UI Security (`wled00/data/*`, OWASP A01/A02/A05)

### WEB1: DOM XSS through `innerHTML`
- **Severity**: CRITICAL
- Prefer `textContent`; if HTML is required, sanitize trusted content path explicitly.

### WEB2: Dynamic code execution
- **Severity**: CRITICAL
- Reject `eval`, `new Function`, and string-based timer execution.

### WEB3: `postMessage` without origin validation
- **Severity**: IMPORTANT
- Require strict origin allowlist checks before processing payloads.

### WEB4: Unsafe redirects/navigation
- **Severity**: IMPORTANT
- Do not navigate directly from untrusted query/input without relative-path or allowlist checks.

### WEB5: Client-only validation
- **Severity**: IMPORTANT
- UI validation is not sufficient; equivalent firmware-side validation is required.

### WEB6: Direct DOM insertion from fetched/config data
- **Severity**: IMPORTANT
- Treat fetched and config-derived strings as untrusted unless proven otherwise.

### WEB7: CSRF checks for state-changing HTTP routes (advisory)
- **Severity**: SUGGESTION
- For state-changing HTTP routes (for example `/json/state`, `/win`), prefer `Origin`/`Referer` validation as low-cost defense-in-depth in firewall-isolated deployments.

## Secrets and Logging (OWASP A04/A09/A10)

### SEC1: Hardcoded secrets and credentials
- **Severity**: CRITICAL
- Reject committed API keys, passwords, tokens, private keys, or test backdoors.

### SEC2: Sensitive values in logs
- **Severity**: CRITICAL
- Do not log passwords, tokens, Wi-Fi keys, auth headers, or full sensitive payloads.

### SEC3: Insecure defaults
- **Severity**: IMPORTANT
- Reject new default credentials or insecure auto-enable behavior for privileged functions.
- For setup/onboarding flows, require first-change behavior for default credentials where applicable.

### SEC4: Overly detailed error responses
- **Severity**: IMPORTANT
- Avoid exposing stack traces or internal details to API/UI consumers.

### SEC5: Credential exposure in API/config responses
- **Severity**: IMPORTANT
- Flag API/config serialization that exposes password-like fields (for example Wi-Fi/AP/MQTT passwords) to unauthenticated clients.

### SEC6: Security-relevant event logging coverage
- **Severity**: SUGGESTION
- Prefer explicit logging for auth failures, OTA attempts, config resets, and AP activation events, without logging secret values.

## Supply Chain and CI/CD (OWASP A03/A08)

### SC1: New dependency risk
- **Severity**: IMPORTANT
- Review new npm/pip/PlatformIO dependencies for legitimacy, pinning, and known vulnerabilities.

### SC2: Workflow permission hardening regressions
- **Severity**: IMPORTANT
- Check for broad `permissions`, unpinned third-party actions, or unsafe secret exposure.
- Flag mutable third-party action refs (`@main`, `@master`, broad tags) where SHA pinning is expected by project policy.
- Flag overly broad permissions such as `write-all` without clear need.

### SC3: Script injection in workflows
- **Severity**: IMPORTANT
- Avoid direct interpolation of untrusted `${{ github.event.* }}` values in `run` commands.

## Reviewer Checklist

- [ ] No new memory-safety hazards (bounds, overflow, unsafe copies/format strings)
- [ ] External input is validated and range-clamped before use
- [ ] State-changing API paths enforce auth policy
- [ ] OTA paths enforce integrity verification (without requiring TLS baseline)
- [ ] Suggested rule patterns are checked where relevant (UDP bounds, hostname sanitization, workflow pinning/permissions)
- [ ] Web UI changes avoid unsafe DOM execution/injection patterns
- [ ] No secrets added; no sensitive logging introduced
- [ ] Error handling remains fail-safe and non-leaky
- [ ] Dependency/workflow changes are supply-chain safe
- [ ] Feature-flag names are valid and not typoed

## AI Review Behavior

- Prefer concrete, file/line-specific findings over generic guidance.
- Prioritize **CRITICAL** and **IMPORTANT** findings.
- Skip irrelevant framework checks not used by WLED.
- If control-flow trust is unclear, ask for clarification instead of guessing.
