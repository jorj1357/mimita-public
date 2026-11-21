# Server setup validation under resource pressure: method

## Purpose

Answer the user request to define a method for AI to test server setup while
builds, CPU load, or memory load are high, and record it as a cold-handoff
runbook on the owning issue.

## Changes

- Issue `issue-be39a4f5ee17` (AI-run server setup validation under resource
  pressure) was claimed and acknowledged for context `ctx-eda926650fa6`.
- Added a bounded load-concurrent validation runbook to the issue discussion:
  safety ceilings, preflight, pinned isolated-run matrix, required readiness
  events, stage timings, verdict rules, and known implementation gaps.
- Added proof task `server-load-baseline` and cancelled two duplicate tasks
  after `root-dll-status-review` created equivalent `rpsv-*` coverage.
- Sent a coordination message to `root-dll-status-review` linking the runbook
  to its implementation tasks and recording current pressure evidence.

## Evidence

- Source/ownership: `devscripts/dev_loop/parallel_test_runs.py` already exposes
  pinned isolated runs with per-process journals and readiness events;
  `devscripts/perf-watch.py` already samples host and process CPU/memory.
- Build: no build requested or performed.
- Runtime: no isolated run performed. Preflight at 2026-11-21T15:35Z measured
  0.27 GB free physical and 98.2 percent memory load against a 2.0 GB reserve,
  so the method correctly returns `NOT_TESTED`.
- Current build pressure: `game_build.status=building`, source generation 1,
  642 sources, jobs 12; published gameplay generation 420 ready at
  13:59:59Z.

## Not changed

- No gameplay, networking, dev-loop, or test-run code was edited.
- No commits were made.

## Next

- Run `server-load-baseline` on the pinned artifact folder
  `2026-11-21T02-26-50.306117Z` and game generation 420 once host reserve is
  restored, then run the CPU/memory/build pressure rows through the
  `rpsv-*` proof tasks.
