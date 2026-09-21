# Generation sync evidence and hot versioninfo

Time created: 2026-09-20T16:50:00Z

## Requested outcome

Investigate the recurring client/server live-generation mismatch and make the
running session identify its executable, generation, server view, and JSONL
file without adding a cold command-registration dependency.

## Changes

- Added `runtime.info`, a generic read-only kernel capability.
- Added hot-package command `versioninfo`; terminal hot commands already take
  precedence over cold `ConsoleCommand` entries.
- Added `generation_sync_state` records for server announcements and mismatch
  transitions, including hashes and protocol phase.
- Added `generation_converged` notification and JSONL record.
- Corrected the mismatch notification wording so candidate/status and pending
  switch states are not reported as an already-active server generation.
- Added regression record:
  `docs/regressions/2026-09-20/generation-mismatch-repeats-REG.md`.

## Evidence

- Build: `C:\mimita-priv-v8\mimita-20260920T124940.exe`
- Hot DLL: `C:\mimita-priv-v8\build\mimita-game.dll`
- Build result: success; 121 compiled, 550 skipped.
- `--live-code-selftest`: PASS; hot package registered 21 commands.
- Runtime two-process convergence and visible `versioninfo` output: not yet
  performed in this session.

## Live-development chain

`versioninfo-command.cpp` is hot package code. The cold kernel supplies facts
through one generic capability, and the terminal dispatches the command through
the active package. Future edits to the command's output can therefore build
and activate a new DLL while the EXE, world, session, and logger stay alive.
Only changes to the generic ABI/capability provider itself cross the one-time
cold boundary documented by the regression.
