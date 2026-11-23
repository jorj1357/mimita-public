# Independent review of the game-dll-build wait issue (issue-8a2e0ac7236b)

## Session-specific evidence

This activation completed the independent review of
`issue-8a2e0ac7236b` ("Wait over budget: game-dll-build at game_build",
wait `game-dll-build:2026-11-23T15:57:37.550Z`). I read the issue record and
readiness (8/8, no missing fields), the dev-loop journal
`logs/2026-11-23/20261123_144429/dev-loop/events.jsonl`, `build/current.json`,
and the wait-budget code in `devscripts/dev-loop.py`. I did not start a build,
a test run, or edit source.

Findings appended as review 281:

- The over-budget wait is legitimate, not an avoidable queue stall. Request
  `2026-11-23T15:57:37.033Z-f2552bcf` started attempt
  `game-dll-20261123T155737827366Z-ad165e` (generation 562, revision 6af8618d).
  `dev-loop.wait.over_budget` fired at 15:58:41.050 (60.923s >= 60.0s) and
  queued exactly one reviewer wake (`wake-ada3b5f1f2dc`).
- The build then failed at 15:59:25 with `phase=changed_source_preflight`,
  `changed-source preflight failed: src/terminal/debug-commands.cpp in 85.397s`,
  and a real compile error at `src/terminal/debug-commands.cpp:583` (no matching
  `Terminal::registerCommand(<brace-enclosed initializer list>)`) in the
  work-in-progress `adminadmin` command (notes from mode3-latency-goal). The
  wait crossed 60s before the failure surfaced.
- Budget calibration: the flat `WAIT_ISSUE_SECONDS=60`
  (`dev-loop.py:114-115`) is below the clean single-TU cost of
  `src/terminal/debug-commands.cpp`. The next good attempt
  (`game-dll-20261123T160037032706Z-485a05`, generation 563) took compile
  36.524s + link 21.293s, worker_total 60.977s, total 69.123s
  (`build/current.json`), so any correct edit to this TU exceeds the budget.
- Observability defect: the wait record reports the previous completed
  generation. `dev-loop.py:3165-3169` sets `game_build_started_utc` and
  `game_build_source_generation` but not `game_build_generation`; that field is
  assigned only on completion (`dev-loop.py:3259`) yet read for the wait record
  at `dev-loop.py:3879`. The wait reported generation 561 while the attempt was
  562 (and the 16:00 wait reported 561 while the real build was 563).
- Proof expectation met: one `dev-loop.wait.over_budget` event, one issue
  record, and one reviewer wake exist for this wait_id. No duplicate issue was
  created (occurrences are auto-filed separately).

## Exact source changes

- No C++ or configuration source changed.
- Appended review 281 to
  `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-8a2e0ac7236b.md`.
- Appended one cross-link suggestion to `issue-136d4553ff92` (owner
  build-coordinator) with the measured budget gap and the stale-generation code
  reference.
- Created this changelog; there was no prior text.

## Next check

Have the build-coordinator calibrate the game-DLL wait budget against measured
changed-path cost and set `game_build_generation` at worker start. Re-run a
single-file edit to `src/terminal/debug-commands.cpp` and confirm the
`dev-loop.wait.over_budget` event either does not fire on the clean path or
records the true in-flight generation.
