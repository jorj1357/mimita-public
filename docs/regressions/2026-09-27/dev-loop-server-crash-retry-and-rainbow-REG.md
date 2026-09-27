# Dev loop stopped at zero processes after a server startup crash

Time created: 2026-09-27T15:48:01Z
Time last updated: 2026-09-27T16:20:00Z

Status: ATTEMPTED FIX (1)

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

## Confirmed cause

Two causes were separated:

1. The build-26 game binary itself exits during dedicated-server startup. Its
   console output reaches community configuration loading, then Windows
   reports exception `0x20474343`. The game-side fault owner is not yet
   identified.
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

## Proof status

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- The server crash remains unresolved at the game-binary level.
- Human review is required to observe the bounded retries, one-shot fallback,
  always-on restart behavior, and terminal colors during a real failed launch.
