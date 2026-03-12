# Category 10 title matching (pre-alert vs end)

**Important for future development.** This document explains how we tell apart **pre-alert** and **end** when the OREF API sends category 10 for both, and why the implementation is done this way.

## The problem

The OREF API uses **category 10** for two different things:

- **Pre-alert**: title `"בדקות הקרובות צפויות להתקבל התרעות באזורך"` (alerts expected in your area soon).
- **End / clear**: title `"האירוע הסתיים"` (the event has ended).

We must distinguish them by the `title` field. That sounds trivial, but in WLED it is not.

## Why parsed `title` is unreliable

1. **WLED disables Unicode decoding in ArduinoJson**  
   In `wled00/wled.h` you have:
   ```c
   #define ARDUINOJSON_DECODE_UNICODE 0
   ```
   So when the API sends `"title":"\u05d1\u05d3\u05e7\u05d5\u05ea ..."`, the parser does **not** convert `\uXXXX` into UTF-8. The string value stored in the document is whatever the library does with that (e.g. literal backslash + `u` + hex, or partial/copy behavior).

2. **The pointer you get from `alert["title"].as<const char*>()`** may not match what you expect: storage, deduplication, or PROGMEM/ESP32 behavior can make comparisons fail even when you think you’re comparing “the same” text.

3. **Using PROGMEM + `strcpy_P` for the expected titles** and then `strstr(payload, buf)` failed in practice on ESP32: the copied buffers did not match the payload, so pre-alert was never detected.

So we **do not rely on the parsed `title`** for category 10 when we have the raw payload.

## The solution: search the raw JSON payload

The HTTP response body is the raw JSON string. It literally contains:

```json
"title":"\u05d1\u05d3\u05e7\u05d5\u05ea \u05d4\u05e7\u05e8\u05d5\u05d1\u05d5\u05ea ..."
```

So we **search that raw string** for unique substrings that identify each title:

- **Pre-alert**: the title starts with “בדקות ” (minutes). In the wire format that is the literal characters `\`, `u`, `0`, `5`, `d`, `1`, … i.e. the ASCII sequence `\u05d1\u05d3\u05e7\u05d5\u05ea ` (with a space).
- **End**: “האירוע ” (the event) → `\u05d4\u05d0\u05d9\u05e8\u05d5\u05e2 ` in the JSON.

We pass the raw payload into `stateFromAlert(alert, payload.c_str())` and, when `rawPayload != nullptr` and category is 10, we **only** use `strstr(rawPayload, needle)` with these needles. No parsed title, no PROGMEM buffers for this path.

## Use inline string literals for the needles (no PROGMEM)

We tried storing the expected titles in PROGMEM and copying them with `strcpy_P` into stack buffers, then passing those buffers to `strstr(rawPayload, buf)`. On ESP32 that did **not** work: the match failed every time (likely PROGMEM/`strcpy_P` or alignment behavior).

The fix that works is to pass **inline string literals** directly to `strstr`:

```c
if (strstr(rawPayload, "\\u05d1\\u05d3\\u05e7\\u05d5\\u05ea ") != nullptr) return STATE_PRE_ALERT;
if (strstr(rawPayload, "\\u05d4\\u05d0\\u05d9\\u05e8\\u05d5\\u05e2 ") != nullptr) return STATE_END;
```

In C, `"\\u05d1"` is a single backslash plus `u05d1`, i.e. the same sequence as in the JSON. So we are searching for the exact bytes that appear on the wire. The compiler places these literals in flash, but using them as the second argument to `strstr` works correctly on ESP32; we avoid any PROGMEM copy for this critical path.

**Do not** replace these with PROGMEM + `strcpy_P` for the raw-payload branch unless you re-verify on real hardware that the match succeeds.

## Where this lives in code

- **`stateFromAlert(JsonObject& alert, const char* rawPayload = nullptr)`** in `redalert.cpp`.
- When `category == 10` and `rawPayload != nullptr`, only the two `strstr(rawPayload, "\\u05...")` checks above are used; the parsed `title` and PROGMEM buffers are not used for that branch.
- Call sites (array and object parsing) pass `payload.c_str()` so the same buffer that was logged and parsed is searched.

## If the API or titles change

1. Get a sample JSON response that contains the new pre-alert or end title.
2. In that JSON, find the exact `"title":"\uXXXX\uXXXX ..."` sequence for the new text.
3. In C, that corresponds to a literal `"\\uXXXX\\uXXXX ..."` (double backslash for one backslash).
4. Pick a **unique** substring (e.g. first word + space) so you don’t false-positive on other fields.
5. Update the corresponding `strstr(rawPayload, "\\u05...")` line in `stateFromAlert` with the new needle.
6. Keep using **inline literals** for the raw-payload path; do not move these needles to PROGMEM without re-testing.

## Fallback when raw payload is not available

When `stateFromAlert` is called with `rawPayload == nullptr`, we fall back to the parsed `title`: we copy PROGMEM expected titles (escaped and UTF-8 forms) into stack buffers and use `strstr(title, buf)`. That path may still be flaky on some builds (e.g. due to `ARDUINOJSON_DECODE_UNICODE 0` or storage layout). In normal operation we **always** pass the payload, so the raw-payload branch is the one that must be correct.

---

**Summary:** For category 10, we distinguish pre-alert vs end by searching the **raw JSON payload** for literal `\u05XX` substrings, using **inline string literals** in `strstr()` so we don’t depend on PROGMEM/strcpy_P on ESP32. Do not rely on the parsed `title` for this when WLED has Unicode decoding disabled.
