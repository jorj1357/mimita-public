# Over-budget game-DLL wait review (build-wait-reviewer)

Date: 2026-11-22 ~14:46Z
Agent: build-wait-reviewer
Context: wake-fdf648dcf5cf
Issue: issue-a236165fa6fc (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started, no files were leased, and no duplicate issue was created.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T14:21:40.449Z`, stage `game_build`,
  elapsed 61.027s, budget 60.0s, request_ids `auto-3`.
- Matching attempt `game-dll-20261122T142140783029Z-c00839`, run
  `20261122_141131`, generation 498, revision `3ed4ed87...`. Journal
  `logs/2026-11-22/20261122_141131/dev-loop/events.jsonl`.
- Timeline (UTC): worker start 14:21:40.449 -> over-budget flag 14:22:49.281
  -> `dev-loop.game-build.result` status=ok 14:22:52.093 -> generation 498
  published 14:22:57.757.
- Cost: `total_seconds` 71.217, compile 49.798, link 10.385, plus ~9s of
  manifest/source snapshot and codegen before `game-build.started` at
  14:21:46.610. Slowest units: `src/npc/npc-navigator.cpp` 27.272s and
  `src/network/server-npcs.cpp` 21.732s; compiled=3, skipped=640.
- At the 61.027s mark the build was ~3s from a successful terminal result. This
  was not a stall.

## Findings

1. Legitimate latency wait, not a queue/worker stall. The attempt was in flight
   and completed successfully 2.4s after the flag; generation 498 published 8.3s
   after the flag.
2. The flat `WAIT_ISSUE_SECONDS` default of 60 (devscripts/dev-loop.py:109-110,
   applied at 3486-3547 to total elapsed since worker start) is shorter than a
   normal fast incremental game-DLL build whenever it must compile a ~20-30s TU,
   so healthy builds produce guaranteed false-positive waits. Same systemic
   class as issue-f200660c97fe and issue-136d4553ff92.
3. Reporter evidence gaps (reported, not fixed):
   - `reason` is a raw truncated worker-output tail (`59755.693,"build_attempt_id":...`),
     not a readable reason. Owner: `_record_game_build_output` stores
     `line.rstrip()[-240:]` (dev-loop.py:2833/2863) and the wait dict reuses it as
     `reason` (dev-loop.py:3539).
   - `generation` is stale: evidence says 497 while the attempt built 498;
     `game_build_generation` is only set at result parsing.
   - `build_id` is `latest_build` (prior published EXE build
     2026-11-22T08:00:31.376619Z), not the in-progress attempt; the attempt id
     appears only inside the malformed reason fragment.
   - no `run_id` in the wait evidence (actual run 20261122_141131).
4. `build_friction.py:131-169` dedups by exact wait_id, so each new over-60s
   game-DLL build opens another identical-titled issue even though only the
   wait_id differs. The board already holds dozens of matching titles; the
   sibling issue-5870fafa670e is the immediately following gen-499 build.

## Claims

- Source evidence: review-only (read of devscripts/dev-loop.py,
  devscripts/build_friction.py, run journal).
- Build evidence: attempt game-dll-20261122T142140783029Z-c00839, generation 498,
  71.217s success (verifiable in the same journal).
- Runtime evidence: agent-wake event only; no gameplay runtime required.
- Human acceptance: not performed.

## Handoff

- Appended evidence and review verdict `reviewed-no-fix` to
  issue-a236165fa6fc; state `in_review`. Pinned `related_build` to the exact
  attempt and `related_run` to 20261122_141131.
- An accidental placeholder review comment was immediately rewritten to a
  correction note; both entries remain in the append-only issue log.
- Recommended fix left for a separate claim (no code edited): make the
  game-dll-build wait progress-aware (flag only after N seconds without a
  phase/file progress event, or reset the timer at first compile submission),
  or raise/scale the game-dll-build budget; emit a human-readable reason; and
  group recurring identical waits into one rolling issue with occurrences.
- No duplicate issue created; no code fix claimed.
