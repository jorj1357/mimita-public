# DLL attempt-wide failure deadline

## User request

> can u make it so u fail before 19 min ? like how to make it fail super quick dont wait to fial

## Change

`devscripts/dev_loop/game_build.py` now caps each whole-game DLL attempt at 18 minutes from `_run_game_build` entry. `MIMITA_GAME_ATTEMPT_TIMEOUT` can lower the cap but cannot raise it above 18 minutes. Remaining time bounds code-generation subprocesses, module-slot waiting, and the game-build worker; manifest progress callbacks and source-snapshot loops check the same deadline. An interrupted source snapshot removes its partial attempt directory. A timeout is written as `failed_phase=attempt_timeout`, with the timed-out phase and configured limit in the attempt record. Existing GCC hard-error detection and sibling cancellation are unchanged.

## Verification

- `python -m py_compile devscripts/dev_loop/game_build.py` passed.
- `git diff --check -- devscripts/dev_loop/game_build.py` passed.
- No build was requested. Current dev-loop status showed no active build or pending batch, but status was `starting`; daemon readiness and loading of this Python change are not yet proven.

## Risk and follow-up

The 18-minute limit is a hard maximum, so it can terminate a healthy build that continues past the deadline. Historical successful attempts include one at 21 minutes 25 seconds. The user explicitly requested failure before 19 minutes; this implementation sets one minute of margin. Before claiming the new behavior is active, coordinate a safe dev-loop daemon reload and verify one controlled root-loop attempt's timeout configuration/terminal reporting. Do not launch an artificial 18-minute timeout trial unless needed; a focused injected-clock test can verify the failure branch.

This is failure-latency handling, not an edit-to-live speed improvement. It does not meet the active <=10-second multi-file live-reload goal.
