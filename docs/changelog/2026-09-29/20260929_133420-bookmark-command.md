# Collision Bookmark Command

Time: `2026-09-29T13:34:20Z`

## Request

Pressing `\\` should immediately bookmark the current collision moment. The
bookmark needs client/server ticks, an ISO 8601 timestamp, JSONL logging, an
optional detail prompt, automatic terminal close, and a visible confirmation.

## Change

- Added the `bookmark` terminal command.
- Added the `\\` gameplay shortcut, routed through the same command owner.
- Captures a per-run bookmark number, player movement/client tick, latest
  received server tick, tick source, and UTC ISO 8601 time in
  `bookmark.created`.
- Opens the terminal with `Write more detail here...`.
- Empty Enter is accepted, closes the terminal, and leaves the original
  bookmark event intact. Non-empty detail appends `bookmark.annotated`.
- Shows the requested `Bookmark saved: ...` notification.

## Evidence

- Source diff passes `git diff --check` for the changed files.
- The previously successful `.dev/builds/0394/mimita.exe` contains the
  bookmark command, prompt, notification, and JSONL event strings.
- The collision self-test passed on `.dev/builds/0394/mimita.exe`.
- A later full build was attempted but is currently blocked by unrelated
  pre-existing `src/impact/destructible-geometry.*` header/source mismatches.
- Live keypress and JSONL acceptance remain pending.

## Files

- `src/devtools/terminal.cpp`
- `src/engine/engine-tick-state.cpp`
