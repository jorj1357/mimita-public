# Dev loop looked frozen after the server crashed before room-code startup

Time created: 2026-09-27T01:45:00Z
Time last updated: 2026-09-27T01:45:00Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/operations/task-completion/task-completion.md`

Related changelog:
`docs/changelog/2026-09-27/20260927_014500-dev-loop-server-crash-status.md`

## Observed

The build linked successfully and published build 11, then the server exited
before writing a room code:

```text
[DEV] server exited before room-code handshake: 3221225477
```

Windows code `3221225477` is `0xC0000005`, an access violation. The loop then
returned to idle without a visible failure status, making it appear frozen.

## Expected behavior

The loop must clearly report that the launched server crashed, persist the
failure state, and remain available for a deliberate `[1]` retry after the
other agent has finished changing or rebuilding the code.

## Confirmed cause

The server child exited before room-code publication. The dev loop's failure
branch printed one line, stopped its tracked children, and returned without
updating `last_message`, saving `server_failed`, or redrawing the control menu.
The underlying access violation in the game binary is not yet diagnosed and
must not be guessed from this orchestration symptom.

## Attempted fix 1

`devscripts/dev-loop.py` now records `server_failed`, prints the exit code, and
prints the status menu after both early server exit and room-code timeout.

The loop does not automatically retry an access-violation crash, because that
would create a crash/relaunch storm while another agent may still be editing.

## Proof status

- Python syntax validation is required after this edit.
- Human review must confirm that a crashed server produces a visible failure
  message and that pressing `[1]` retries once the build is ready.
- The game-side `0xC0000005` remains unresolved until the server's crash output
  or dump identifies its first failing owner.
