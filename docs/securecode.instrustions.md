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

### FW5: Missing auth checks on state-changing endpoints
- **Severity**: CRITICAL
- Any endpoint that changes device state/config must enforce configured auth policy.

### FW6: Fail-open behavior after parse or allocation errors
- **Severity**: IMPORTANT
- On error, reject update and preserve safe previous state.

### FW7: Heap churn in hot paths
- **Severity**: IMPORTANT
- Avoid repeated dynamic allocation in render/effect loops; prefer pre-allocation and reuse.

### FW8: Unsafe use of `String` in performance-critical paths
- **Severity**: IMPORTANT
- In hot paths, avoid repeated `String` growth; reserve or use fixed buffers.

### FW9: Unsafe feature flag names
- **Severity**: IMPORTANT
- Verify all new `WLED_ENABLE_*`/`WLED_DISABLE_*` names are valid known flags; typos silently alter build behavior.

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

### SEC4: Overly detailed error responses
- **Severity**: IMPORTANT
- Avoid exposing stack traces or internal details to API/UI consumers.

## Supply Chain and CI/CD (OWASP A03/A08)

### SC1: New dependency risk
- **Severity**: IMPORTANT
- Review new npm/pip/PlatformIO dependencies for legitimacy, pinning, and known vulnerabilities.

### SC2: Workflow permission hardening regressions
- **Severity**: IMPORTANT
- Check for broad `permissions`, unpinned third-party actions, or unsafe secret exposure.

### SC3: Script injection in workflows
- **Severity**: IMPORTANT
- Avoid direct interpolation of untrusted `${{ github.event.* }}` values in `run` commands.

## Reviewer Checklist

- [ ] No new memory-safety hazards (bounds, overflow, unsafe copies/format strings)
- [ ] External input is validated and range-clamped before use
- [ ] State-changing API paths enforce auth policy
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
