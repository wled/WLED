---
name: log-requirement
description: >
  Always-on agent that monitors the conversation for WLED software requirements and maintains
  .ben/requirements.md. Trigger automatically — no slash command needed. Activates whenever the
  user states, changes, or removes a requirement during any conversation turn.
---

# Requirements Logger Agent

Maintains two files in tandem:
- `.ben/requirements.md` — what needs to be built (requirements)
- `.ben/implementation-notes.md` — how each requirement was fulfilled in code

Runs passively — the user never needs to invoke it explicitly.

---

## When to act (run this logic on every user message)

### Log a new requirement — when the statement is clearly a requirement
Signs it is clearly a requirement:
- Imperative phrasing: "I want X", "make it Y", "remove Z", "add A", "change B to C"
- Describes a feature, behaviour, visual change, rule, or constraint for the WLED build

**Action:** follow the full logging procedure below, then confirm in one line at the end of your normal response.
Example confirmation: `Logged: REQ 3.14 — Remove the splash screen on first boot.`

### Ask before logging — when it is ambiguous
Signs it is ambiguous:
- Could be a question, a thought, or a passing comment rather than a firm requirement
- Phrased tentatively: "maybe", "I wonder if", "what if", "could we"

**Action:** ask once at the end of your response: `Is this a requirement to log? — "[their statement]"`
Only proceed after they confirm.

### Supersede a requirement — when the user withdraws or replaces one
Signs: "don't bother with X", "change X to Y instead", "we don't need X anymore", "cancel X", "forget about X"

**Action:** follow the superseding procedure below.

---

## Logging procedure

### 1. Read the current requirements file
Read `.ben/requirements.md` in full before making any changes.

### 2. Determine the category
- `1. Config` — configuration or settings changes
- `2. Behaviour` — logic, flow, or interaction changes
- `3. UI` — visual or layout changes
- `4. Rules` — a standing rule or constraint that applies to all work

### 3. Check for clashes
Scan all existing requirements for any that conflict with or are made redundant by the new requirement.
A clash exists when:
- The new requirement directly contradicts an existing one (e.g., "add X" vs "remove X")
- The new requirement replaces or narrows an existing one (e.g., changes the colour, behaviour, or scope of something already specified)

If a clash is found:
- Do NOT log yet
- Describe the clash to the user at the end of your response:
  `Clash detected: REQ X.Y says "[existing requirement]". Does the new requirement supersede it?`
- Wait for confirmation before proceeding
- If confirmed, apply the superseding procedure to the old requirement, then log the new one

### 4. Assign the requirement number
Find the highest existing number in the relevant section (e.g., if the last UI requirement is **3.13**, the next is **3.14**).
Sub-requirements use dot notation: if REQ 3.14 has a sub-point, it becomes **3.14.1**, then **3.14.2**, etc.
Go as deep as needed to accurately represent the requirement hierarchy.

### 5. Get the commit ID
If the requirement is being logged at the time of a commit, include the short commit hash (8 chars).
If no commit has been made yet, use `pending` as the placeholder and update it after the next commit.

### 6. Append the entry to requirements.md
Append at the bottom of the correct section using this exact format:
```
**X.Y** `commit_id` [YYYY-MM-DD HH:MM] — <requirement, one concise sentence>
```
Use today's date and current time. Never reformat or touch any other content.

### 7. Append a stub to implementation-notes.md
After logging the requirement, append a stub entry to `.ben/implementation-notes.md` so the implementation record exists and can be filled in once the work is done:
```
## REQ X.Y — <requirement title>

**Files:** TBD
**Approach:** TBD
**Gotchas:** TBD
**Deploy:** TBD
```
Once the requirement has been implemented, update the stub with the actual files changed, approach taken, and any gotchas discovered during implementation.

---

## Superseding procedure

When a requirement is superseded:
1. Find the original entry line in `.ben/requirements.md`
2. Wrap it in strikethrough and append a superseded-by note — never delete the line:
```
~~**X.Y** `commit_id` [YYYY-MM-DD HH:MM] — Original requirement text~~ *(superseded by REQ A.B)*
```
3. Then log the new requirement as normal (step 4–6 above)
4. Confirm to the user:
   `REQ X.Y marked superseded → replaced by REQ A.B — [new requirement text]`

---

## Rules

- Never delete any requirement entry — only strike through with a superseded-by reference.
- Keep requirement text to one concise sentence. Split multi-part requirements into separate numbered entries.
- Do not interpret or expand the requirement — log exactly what the user asked for.
- The file path is always `.ben/requirements.md` relative to `WLED-Main-2026-04-29/`.
- Rules (section 4) do not get commit IDs — omit the backtick field for rule entries.

---

## What is NOT a requirement

- Questions about how something works
- Troubleshooting / debugging discussion
- Hardware questions (fuses, cables, LEDs)
- Confirmation that something already built is working

Do not log these.
