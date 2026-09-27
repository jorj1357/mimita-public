# Dev loop stopped at zero processes after a server startup crash

Time created: 2026-09-27T15:48:01Z
Time last updated: 2026-09-27T16:24:36Z

Status: FIXED (configuration startup blocker); dev-loop fallback behavior remains
covered by the orchestration fix below.

Related specification:
`docs/operations/task-completion/task-completion.md`

Related changelog:
`docs/changelog/2026-09-27/20260927_154801-dev-loop-retry-and-rainbow.md`

## Observed

Build 26 linked successfully and was published, but its dedicated server exited
before writing a room code with exit code `541541187` (`0x20474343`). The loop
then showed `RUNNING: (none)` and repeated the same failure when the user
pressed `[1]`.

Windows Application Error records confirm the faulting executable was
`C:\mimita-v9\.dev\builds\0026\mimita.exe`, before room registration.

## Expected behavior

The loop should make the server failure visible, automatically retry when
AUTO-RESTART is enabled, and avoid leaving the user with zero MiMITA processes.
Retries must be bounded so a broken binary does not create an endless crash
storm.

The development-loop text should be visually distinct from ordinary command
output and cycle through the rainbow over five seconds.

## Confirmed causes

Two causes were separated:

1. The server loaded `config/onlinemodes.json` and `config/weaponsets.json`,
   then crashed while scanning `config/gamemodes/`. The offending file was
   `config/gamemodes/retrograd.json`, which began with `//` comment lines even
   though the runtime treats these files as strict JSON. The failure occurred
   before map loading, ICE initialization, or room-code publication.
2. The dev loop treated an exited server as an idle state. It did not schedule
   a bounded retry or launch a fallback process, so the visible result was
   `RUNNING: (none)`.

## Attempted fix 1

`devscripts/dev-loop.py` now:

- monitors whether its tracked MiMITA children are still alive;
- automatically retries the latest server up to three times with a delay,
  regardless of the legacy AUTO-RESTART toggle;
- launches one fallback GUI MiMITA process after the retry budget is exhausted;
- preserves the server exit code and failure state; and
- launches the fallback client at most once until a new build is selected;
- keeps AUTO-RESTART permanently ON instead of exposing a toggle that can
  leave the session stopped; and
- colors dev-loop/build-child terminal output with a moving five-second HSV
  rainbow when attached to a real terminal. The live status block is
  continuously redrawn so its already-visible text changes color too, while
  normal output first clears that block to avoid cursor corruption.

The fallback client does not claim that the server succeeded; it only prevents
the development session from having no MiMITA process at all.

## Corrective configuration fix

The non-JSON comment header was removed from `config/gamemodes/retrograd.json`.
The file remains valid JSON and keeps the same runtime fields and values.

## Proof status

- `config/gamemodes/retrograd.json | ConvertFrom-Json`: passed.
- Build 32 executable with the corrected config survived five seconds, reached
  dedicated server transport, initialized ICE, registered room `8THPU7N`, and
  wrote the room-code file.
- The same build with the original commented header exited with
  `541541187` (`0x20474343`) before the gamemode loader reached room startup.
- The projectile trail and Counter-Strike changes were not the cause of this
  startup failure.

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Human review remains required for the automatic room-code client join and
  visual terminal behavior. The currently running old dev-loop process must be
  restarted to load the one-shot fallback guard.
