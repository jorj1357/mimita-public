# Terminal command aliases from JSON

Time: 2026-10-01T18:42:39Z (America/New_York display context)
Branch: `afad20a-rebuild`

## Summary

Added live JSON-backed terminal command aliases at
`config/command-aliases.json`. The terminal now reloads the file when its
timestamp changes, expands aliases before normal command dispatch, appends
typed arguments, supports semicolon chains, and stops cyclic expansion after
16 levels. Invalid edits preserve the last valid alias map. Real registered
commands take precedence over config aliases.

## Files changed by this session

- `config/command-aliases.json:1-8`: replaced the TODO placeholder with the
  versioned `aliases` object and sample `cs`, `cc`, and `tpv1` mappings.
- `src/devtools/terminal.h:3,112-134`: added alias state and the private
  loading/dispatch helpers.
- `src/devtools/terminal.cpp:31-43,670-826`: added JSONC-compatible parsing,
  timestamp polling, safe replacement, recursive expansion, cycle protection,
  and centralized semicolon dispatch for both UI and direct callers.
- `src/devtools/terminal-builtins.cpp:45-61`: added the `aliases` inspection
  command.

## Documents and focused review

Read and applied:

- `docs/ROUTER.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/live-development/live-development.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md`

Terminal command review: PASS for registration, ownership, validation,
collision handling, chaining, and bounded expansion. JSON configuration review:
PASS for one owner, safe defaults, atomic last-valid replacement, and live
timestamp detection.

## Validation evidence

- `config/command-aliases.json` parsed successfully and its expected sample
  mappings were verified.
- `git diff --check` was clean for the four session-owned files. Existing
  unrelated changes in `docs/specs/20261001plan.md` retain their prior
  whitespace warnings.
- A full cold build was not run because active MiMITA processes were detected
  (`mimita.exe` PIDs 1676 and 28384). The running executable was not closed,
  restarted, replaced, or relinked.
- A direct compiler syntax-check attempt returned exit code 1 without compiler
  diagnostics in this environment; therefore no compilation claim is made.

## Pre-existing work preserved

All other modified and untracked files shown by `git status`—including the
physical-object changes, terminal rendering/input edits, existing plan,
regression, changelog, account/config edits, and image artifacts—were left
untouched.

## Human/runtime review still needed

After the normal executable build/activation path is available, open the
terminal and verify `aliases`, `cs`, `cc`, `tpv1`, a custom semicolon alias, a
live file edit, and an invalid JSON edit. The current running executable does
not contain this new cold terminal code yet.
