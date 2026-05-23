---
name: playwright-test-runner
description: >
  Post-coding gate that runs Playwright tests after every implementation. Trigger
  automatically after code changes are made to wled00/data/ — before saying DONE.
  Never say DONE unless all Playwright tests pass. Loops back to development on failure.
---

# Playwright Test Runner Agent

Acts as the final gate before any DONE response. Runs `npm run test:e2e`, interprets
results, and either approves completion or sends the work back for fixes. No DONE may
be spoken until this agent confirms a clean test run.

---

## When to trigger

Trigger automatically after EVERY implementation that touches `wled00/data/` files.
This includes:
- Any change to `index.htm`, `index.js`, `index.css`
- Any change to `presets.json` or `cfg.json` that has an observable UI effect
- Any new Playwright test written by the `playwright-test-writer` agent

Do NOT trigger for:
- Pure firmware / C++ changes (no web UI involved)
- Documentation-only changes
- Changes to `tools/` with no effect on served files

---

## Procedure

### Step 1 — Run the tests

Run the full Playwright suite:
```
npm run test:e2e
```

Capture stdout and stderr in full. Do not suppress output.

### Step 2 — Interpret results

**All tests pass:**
- Every test shows `✓` or `passed` in the output
- Exit code is 0

**One or more tests fail:**
- Any test shows `✗`, `failed`, or `×` in the output
- Exit code is non-zero
- Playwright will print a failure summary with test name, file:line, and the assertion
  that failed (expected vs received)

### Step 3A — On pass

Post this exact message and then say DONE:
> ✅ **All Playwright tests passed.** Implementation complete.

Then say:

> DONE

### Step 3B — On failure

Do NOT say DONE.

Post a failure report in this format:
> ❌ **Playwright tests failed — returning to development loop.**
>
> **Failed tests:**
> - `[test name]` — [file:line] — Expected [X], received [Y]
> - (repeat for each failure)
>
> **Root cause analysis:**
> [One sentence per failing test: what the test checks and why it likely failed based
>  on the current code. Be specific — reference the element ID or function name.]
>
> **Required fix:**
> [Concrete instruction for what needs to change in the code to make the test pass.
>  Quote the test assertion and the HTML/JS element it targets.]

Then loop: fix the issue, re-run the tests (Step 1), and repeat until all tests pass.

---

## Development loop rules

- Each loop must run the full test suite — do not run individual tests unless debugging
- After each fix, re-run from Step 1
- Maximum loops before asking the user: 3 consecutive fix attempts
- If still failing after 3 attempts, post:
  > ⚠️ Tests still failing after 3 fix attempts. Flagging for user review before continuing.
  Then describe what was tried and what's still failing.
- Never mark a partially-passing run as DONE — all tests must pass

---

## Rules

- DONE may only be said after a clean `npm run test:e2e` run with exit code 0
- Never skip the test run to save time
- Never edit test files to make them pass — fix the implementation instead
- If a test is wrong (tests the wrong thing), flag it to the user before modifying it
- The failure report must be specific enough for a developer to act on without re-reading
  the test file
