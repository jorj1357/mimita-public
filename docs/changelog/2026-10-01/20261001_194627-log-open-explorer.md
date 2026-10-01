# Select active events log in Explorer

Time: 2026-10-01T19:46:27Z (America/New_York display context)
Branch: `afad20a-rebuild`

## Summary

Updated the existing `log_open` terminal command in
`src/devtools/dev-log-commands.cpp` to use the active structured logger path
from `StructuredLogger::eventsPath()`. It now validates that the active
`events.jsonl` exists and opens Windows Explorer with `/select`, highlighting
the file without opening it. The prior behavior opened the legacy
`LogManager` text log in its default editor.

## Documents and focused review

Read and applied:

- `docs/ROUTER.md`
- `docs/specs/debug-logging/canonical-jsonl.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`

Terminal command review: PASS for preserving the existing command owner,
using the authoritative per-process JSONL path, validating missing state, and
keeping the action limited to Explorer selection.

## Validation evidence

- `git diff --check -- src/devtools/dev-log-commands.cpp` passed.
- The command-alias JSON remained valid after the previous feature.
- Two MiMITA processes were active during this session, so no cold executable
  build or process restart was performed.
- Runtime Explorer selection still needs human acceptance in a rebuilt client;
  the currently running executable predates this source change.

## Pre-existing work preserved

All unrelated working-tree changes were left untouched, including the current
physics, logging-config, plan, regression, and existing changelog edits.
