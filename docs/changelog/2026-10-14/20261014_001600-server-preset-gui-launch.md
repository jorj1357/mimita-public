# Server preset GUI-equivalent launch

- Timestamp: 2026-10-14T00:16:00Z (display timezone: America/New_York)
- Branch: `20261011zt`
- Result: PASS_WITH_HUMAN_REVIEW

## Root cause

The live journal showed the client rejecting a server because the client
expected run `20261013_234636`, build `2238`, while the server answered as run
`20261014_000007`, build `2240`. The rejection is intentionally owned by
`src/network/multiplayer-tick.cpp:1023-1050`; it was not disabled.

## Fix

- Added `launchServerPresetAndJoin()` at
  `src/gui/gui-main.cpp:352-386` and declared it in
  `src/gui/gui-main.h:119-123`.
- Changed `server_start_preset` at
  `src/terminal/network-commands.cpp:380-402` to use that shared GUI external
  server-launch path and immediately queue a localhost join.
- The external child inherits the client runtime/build environment, preserving
  the identity guard while making terminal startup follow the GUI path.
- The existing in-process listen-server fallback remains available if child
  creation fails.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/regressions/2026-09-20/generation-mismatch-repeats-REG.md`

## Validation

- Canonical dev-loop build published build `2244` successfully after the
  initial asynchronous build status transition.
- Final dev-loop status: `latest_build_stale=false`, `server_alive=true`,
  `client_ready=true`, `builds_match=true`, run `20261014_001506`.
- Live journal: `logs/2026-10-14/20261014_001506/events.jsonl`.
- The bounded `ss` command was queued through the live client. The client
  continued receiving server snapshots with no new identity-mismatch event.
- `git diff --check` passed for the three source files changed here.

## Human review still needed

Run `ss` in a normal standalone client with no pre-existing server on port
1357, and confirm it launches the sandbox server, joins automatically, and
loads the sandbox map. The dev-loop check used an already-running server, so
it proves identity-safe live operation but not replacement of that server's
map/mode.
