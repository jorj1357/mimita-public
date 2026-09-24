# Canonical client/server JSONL logging

## Scope

Consolidated live-code diagnostics into the authoritative structured
`events.jsonl` stream and added a shared-session path for `buildv4.py`.

## Changes

- `LiveEventJournal` remains source-compatible but now bridges records into
  `StructuredLogger`; new runs no longer create a separate
  `logs/features/live-code/live_events_*.jsonl` file.
- `StructuredLogger` accepts `MIMITA_EVENTS_FILE` so the client and server can
  append to the same session file while retaining process/PID provenance.
- `StructuredLogger::Entry` can carry structured fields from compatibility
  callers.
- `buildv4.py` creates one session `events.jsonl`, passes it to both processes,
  and opens that exact file in VS Code.
- Added the canonical logging contract at
  `docs/specs/debug-logging/canonical-jsonl.md`.
- Kept the debug category configuration focused on network, NPC combat,
  grenade-launcher, and executable diagnostics; performance, movement,
  collision, and weapon spam remain disabled.

## Validation

- `python -m py_compile buildv4.py` passed.
- `git diff --check` passed; only line-ending normalization warnings were
  reported.
- A full executable build and live client/server acceptance test remain to be
  run because this logger bridge is a cold executable change.
