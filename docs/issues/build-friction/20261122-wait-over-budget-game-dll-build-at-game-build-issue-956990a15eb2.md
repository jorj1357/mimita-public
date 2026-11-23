# issue-956990a15eb2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (false-positive-flat-wait-budget)

### 2026-11-22T23:06:16.804Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-7f7253b08ab1). Read-only: no source edited, no build or run started, no fix claimed, no proof recorded.

VERDICT: false positive from a flat, non-progress-aware wait budget -- not a build stall.

FACT (this occurrence)
- wait_id game-dll-build:2026-11-22T22:25:57.280Z, stage game_build, elapsed 60.625s vs budget 60.0s, started_utc 2026-11-22T22:25:57.280Z.
- Evidence fields point at build_id 2026-11-22T21:32:48.195707Z, generation 537, request_ids [auto-14, 2026-11-22T22:25:08.019Z-70043085].
- The actual in-flight attempt was game-dll-20261122T222557307822Z-3cc471: build.started 22:25:57.307Z, build.completed 22:27:25.843Z, elapsed_ms 88520 (88.52s), exit_code 0, generation 538, no_op=false, dll build\game\generation-538\game-g538.dll (.dev/build-timings/game-dll-20261122T222557307822Z-3cc471.jsonl, build.completed line).
- At the 60s crossing (~22:26:57.9Z) the build was actively compiling/linking, not stalled: last TU before the crossing src/network/server-npcs.cpp compiled 22:26:56.360Z; link.response_file_prepare 22:27:02.626Z at build elapsed 65.32s; link.lld_linker ran 22.2s and finished 22:27:24.861Z. It succeeded ~28s after the flag.
- Dev-loop journal logs exactly one issue record (created=true) and exactly one reviewer wake for this wait_id (logs/2026-11-22/20261122_220630/dev-loop/events.jsonl lines 95-96, wall_time 22:26:59.953Z/22:26:59.954Z). The autowire acceptance (carries wait_id, stage, reason, elapsed, build/run IDs, evidence; one reviewer wake queued exactly once) is therefore met mechanically.

FACT (root cause, code)
- dev-loop.py:109-110 sets one global WAIT_ISSUE_SECONDS (default 60.0) used for all wait kinds.
- check_wait_budget() at dev-loop.py:3531-3547 applies that single budget to game_build on total elapsed since game_build_started_utc, with no per-stage budget and no progress/liveness test. Any normal whole-game DLL build >60s fires even while advancing.
- Calibration proof: the current published build generation 541 took total_seconds 212.132s / compile_seconds 197.389s (.dev/build/current.json). A 60s game_build budget is below the normal floor, so the flag cannot distinguish a regression from routine work.

FACT (secondary evidence defects, same wait)
1. Structured attribution lags: evidence generation=537 and build_id=2026-11-22T21:32:48.195707Z (a prior EXE build) while the in-flight attempt was generation 538. The wait payload has no build_attempt_id; reviewers must recover it from free text.
2. reason is not human-readable: evidence.reason is the trailing 240 chars of a worker progress JSON line (fragment ee-gimplify/raw/tree-gimplify, truncated). Cause: dev-loop.py:2833 detail = line.rstrip()[-240:] feeds game_build_detail, which check_wait_budget uses as reason.

INFERENCE
- Same flat-budget signature already recorded on parent tracker issue-e044a931c54b (e.g. 2026-11-22T00:17Z suggestion) and issue-7d3dcfd7433b. Siblings in the same run confirm it is systemic, not per-build: issue-6943764e0d8e (22:14:00, 60.541s), issue-681e9fae416f (22:24:13, 60.484s), issue-be83fb5843ba (22:42:14, 60.994s).

UNKNOWN
- Whether the dominant pre-compile prelude (manifest/source snapshot) or compile/link needs the largest calibrated budget; that requires the per-stage baseline work on issue-7d3dcfd7433b.

RECOMMENDED NEXT CHECK (no code claimed; needs owner/human approval)
- Make the budget kind- and stage-aware and progress-aware (calibrate game_build from .dev/build-timings; only fire on a stop in the active stage with no progress past budget).
- Populate structured build_attempt_id/generation/stage from active build state while building, and stop using the last-240-chars detail as the reason.
- Consolidate repeated same-stage waits under issue-e044a931c54b instead of one high-priority issue per wait_id.

This issue is retained for the wait-policy root cause rather than closed; no duplicate fix issue created.
