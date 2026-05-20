---
name: session-handoff
description: >
  Generates a structured session handoff document so the next Claude session (or developer) can
  pick up exactly where this one left off. Use this skill whenever the user says /session-handoff,
  "wrap up", "end of session", "handoff doc", "summarize what we did", "prep for next session",
  or anything suggesting they want to capture the current state of work before closing out.
  Also trigger proactively if the conversation is long and the user seems to be wrapping up.
---

# Session Handoff Skill

Produces a concise, copy-paste-ready handoff document capturing the full state of the current
working session. The goal: someone (or Claude) reading this cold should be able to resume work
in under 2 minutes.

---

## Output Format

Generate a markdown document with exactly these six sections. Keep each section tight — ruthlessly
cut anything that isn't actionable or load-bearing for the next session.

```markdown
# Session Handoff — [Project / Feature Name] — [Date]

## Decisions locked & what shipped
- [Decision or completed item]. Reason: [one-line rationale if non-obvious]
- ...

## Key files for next session
- `path/to/file.ext` — [why it matters / what's in it]
- ...

## Running state
- **Running**: [services, containers, processes currently up]
- **Broken / degraded**: [what's currently failing and known cause if any]
- **Half-done**: [work-in-progress — what's been started but not completed]

## Verification
- [Command or step to confirm X still works]
- [How to check Y is healthy]
- ...

## Deferred + open questions
- [ ] [Deferred task or unresolved question]
- [ ] ...

## Pick up from here
Next concrete action: [single, specific, unambiguous first step]
Context needed: [any env vars, credentials location, branch name, config quirk the next session needs to know]
```

---

## Instructions

1. **Mine the conversation** — scan the full conversation history for decisions made, commands run,
   files mentioned, errors hit, and work completed. Don't ask the user to repeat what's already
   in context.

2. **Ask targeted fill-in questions** — only ask about things genuinely not inferable from context:
   - Current running state (you can't observe the terminal)
   - Any deferred items the user wants to explicitly flag
   - The single "pick up from here" action if ambiguous

3. **Be ruthless about brevity** — each bullet should be one line. If you need two lines, the item
   probably belongs in a file, not the handoff doc. No waffle, no padding.

4. **Verification steps must be runnable** — write actual commands or URLs, not vague descriptions
   like "check that it works". E.g. `docker ps | grep n8n` not "verify n8n is running".

5. **"Pick up from here" must be atomic** — one action, not a list. If the next step is genuinely
   unclear, say so explicitly and list the two candidates so the next session can decide fast.

6. **Output as a code block or artifact** — wrap the final document in a markdown code block or
   create it as a `.md` file so it's easy to copy. Offer to save it as
   `handoff-YYYY-MM-DD.md` if a filesystem is available.

7. **Save output** — create a session file with the handoff document content, so it can be retrieved later if needed.
   - Run `date +"%Hh-%Mm"` in Bash to get the current hour-minute before writing the file.
   - Filename format: `session-handoff-YYYY-MM-DD-HHh-MMm.md` (e.g. `session-handoff-2026-05-04-14h-32m.md`). The `HHh-MMm` part is **mandatory** — a date-only filename is invalid.
   - Save location: `.ben/session-hand-offs/`

## Examples

Bad: "Continue working on the n8n flow"
Good: "Open the Invoice Approval workflow in n8n, fix the Switch node routing bug on the
      Telegram callback branch — the condition currently matches `filename_approval` but needs
      to also check `chat_id` to avoid collision with file-download callbacks."

Bad: "Check the docker stuff"
Good: "Run `docker compose logs n8n --tail=50` on PrimaryNUC (192.168.1.140) — the MCP
      authorization failure was last seen here, fix should be a missing `X-N8N-API-KEY` header."

---

## Tone

Write for your future self at 9am on a Monday. Clear, dry, no fluff.