# Over-budget game-DLL wait review (build-wait-reviewer)

Date: 2026-11-22 ~13:54Z
Agent: build-wait-reviewer
Context: wake-637b83a47c6a
Issue: issue-f200660c97fe (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started, no files were leased, and no duplicate issue was created.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T13:00:15.598Z`, stage `game_build`,
  elapsed 60.735s, budget 60.0s, request_ids `auto-1` and
  `2026-11-22T13:00:31.139Z-6b8ed743`.
- Matching attempt `.dev/build-timings/game-dll-20261122T130015674891Z-687e4d.json`,
  started 13:00:15.674Z, completed 13:01:17.636Z, success, generation 492,
  total 61.943s, changed only `src/effects/zombie-tower-effects.cpp`.
- Cost: single-TU compile 24.540s inside `changed_source_preflight` 24.891s,
  link 9.025s (480,395,963 bytes / 641 objects), object-cache scan 2.856s,
  compile phase 1.666s, coherence validation 1.476s, publish 0.607s.
  compiled=2, skipped=641.
- At the 60.735s mark (~13:01:16.333Z) the build was in final
  coherence-validation/publish; it completed ~1.3s later. This was not a stall.
- Prior attempt `game-dll-20261122T125133309168Z-46b1e5` took 68.864s, so a
  normal heavy one-TU incremental DLL build is routinely above the fixed 60s
  `WAIT_ISSUE_SECONDS` budget (devscripts/dev-loop.py:109-110).
- Wake `.dev/agent-wake/events.jsonl`: exactly one wake.queued for
  wake-637b83a47c6a at 13:01:19.515Z; codex claimed/launched, then opencode
  fallbacks at 13:08:02 and 13:35:18 reused the same wake id.

## Findings

1. Legitimate latency wait, not a queue/worker stall. The build was in flight at
   the 60s mark and succeeded at 61.943s.
2. Reporter evidence gaps (reported, not fixed):
   - `reason` is a raw truncated worker-output tail (`438.765,"build_attempt_id":...`),
     not a readable reason. Owner: `_record_game_build_output` stores
     `line.rstrip()[-240:]` (devscripts/dev-loop.py:2833/2863) and the wait dict
     reuses it as `reason` (dev-loop.py:3539).
   - `generation` is stale: evidence says 491 while the attempt built 492.
     `game_build_generation` is only set at result parsing (dev-loop.py:2976).
   - `build_id` is `latest_build` (prior published build
     2026-11-22T08:00:31.376619Z), not the in-progress attempt; the attempt id is
     only inside the malformed reason fragment (dev-loop.py:3543).
   - no `run_id` in the wait evidence (actual run 20261122_011031).
   - the expected dev-loop wait event is absent: `record_runtime_event`
     (dev-loop.py:2275-2300) targets
     `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl`, but that directory
     does not exist and the OSError is swallowed.
3. The fixed 60s budget is shorter than a normal heavy-TU edit build. Same
   systemic class as issue-136d4553ff92.
4. `build_friction.py:131-169` dedups by exact wait_id, and no second issue
   exists for this wait; the wake was queued once.

## Claims

- Source evidence: review-only (read of devscripts/dev-loop.py,
  devscripts/build_friction.py, build timings).
- Build evidence: attempt game-dll-20261122T130015674891Z-687e4d, generation 492,
  61.943s success.
- Runtime evidence: agent-wake events only; no gameplay runtime required.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-with-evidence-gaps` to
  issue-f200660c97fe; state `in_review`. Pinned `related_build` to the exact
  attempt and `related_run` to 20261122_011031.
- An accidental placeholder review comment was immediately superseded by the
  substantive review; both remain in the append-only issue log.
- No duplicate issue created; no code fix claimed.
