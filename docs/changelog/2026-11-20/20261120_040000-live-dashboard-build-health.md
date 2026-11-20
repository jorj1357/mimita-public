// 2026-11-20T04:00:00Z
/* purpose
* record live dashboard build-health visibility and dependency-prepass heartbeats
* separate dashboard/tooling proof from game runtime proof
* this file DOES NOT claim the active build or game features are complete
*/

# Live dashboard build health

## Task

- Task ID: session-20261120-live-dashboard-build-health
- Owner: runtime-60hz
- Focus: make the human dashboard show the latest true build/repo state
- Status: tooling implemented; dashboard restarted; build still active

## What changed

- `game_build_worker.py` now reports dependency-prepass progress:
  - `scan_phase=dependency_prepass`
  - parsed/total depfiles
  - current depfile
  - prepass elapsed time
  - `last_progress_utc`
- The prepass heartbeat updates at most twice per second. This removes the
  false `OBJECT CACHE SCAN 1/642 0%` appearance during the long parse/stat
  prepass.
- `dev_loop/gui.py` now reads live progress/current/state on every one-second
  refresh and shows:
  - `LIVE`, `SLOW`, `NO PROGRESS`, `FAILED`, or `PUBLISHED`;
  - last progress timestamp and age;
  - dependency-prepass parsed/total files;
  - dependency/object scan timing and current file;
  - source generation and published generation;
  - build attempt, notes, and errors.
- The build card is red for `NO PROGRESS`/`FAILED` and yellow for `SLOW`.
- The dashboard was restarted with the new GUI code. It remains open as PID
  `12936`, title `MiMITA Coordinator Dashboard — mimita-v9`.

## Observed state

At the first live read after the change, the dashboard helper reported:

```text
Game DLL: COMPILE 629/642 (98%) · SLOW 35s
source gen 59 · published gen 393
worker elapsed 1069.143s
CXX 1 · SKIP 628
```

The previous screenshot's `OBJECT CACHE SCAN 1/642 0%` was a prepass reporting
gap, not proof that the build was dead. The active attempt continues.

## Verification

- `python -m py_compile devscripts/game_build_worker.py devscripts/dev_loop/gui.py`
  passed.
- The dashboard helper was called directly against `C:\mimita-v9` and returned
  the live compile/health/source-generation data above.
- `python -m unittest devscripts.quality.test_agent_coord` and
  `devscripts.quality.test_manual_build_guard` passed: 34 tests.
- `git diff --check` passed for the changed tooling/docs.
- The dashboard process is alive after restart and the policy reports
  `required=true`.
- No game runtime, 60 Hz, 240 FPS, or build-completion claim is made.

## Remaining

- The active build still needs to publish.
- The build-owner health/watchdog brain integration from the build-queue policy
  remains to be implemented; the dashboard now exposes the truth needed for it.
- After publication, the pinned 67-NPC and 10-minute connection tests continue.
