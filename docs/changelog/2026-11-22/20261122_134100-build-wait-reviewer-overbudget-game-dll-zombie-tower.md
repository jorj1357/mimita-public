# Over-budget game-DLL wait review (build-wait-reviewer)

Date: 2026-11-22 ~13:36Z
Agent: build-wait-reviewer
Context: wake-70d850300ee2
Issue: issue-bbaee7b0d2ee (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started, no files were leased, and no duplicate issue was created.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T12:51:33.244Z`, stage `game_build`,
  elapsed 60.121s, budget 60.0s, request `2026-11-22T12:51:32.959Z-148766b1`.
- Matching attempt `.dev/build-timings/game-dll-20261122T125133309168Z-46b1e5.json`,
  generation 491, changed only `src/effects/zombie-tower-effects.cpp`, success,
  total 68.864s, completed `2026-11-22T12:52:42.192Z`.
- Cost: single-TU compile 30.657s (`changed_source_preflight` 31.017s), link
  10.371s (480,395,963 bytes / 641 objects), object-cache scan 2.604s,
  source_snapshot 14.306s, publish 0.906s. compiled=2, skipped=641.
- Build note b9da2fd00705 ("test async source-snapshot pruning on a
  one-TU edit").
- Follow-on attempt `game-dll-20261122T130015674891Z-687e4d` (gen 492) was also
  over budget at 61.407s (preflight/compile 26.516s + link 8.073s); it produced
  the separate issue f200660c97fe.
- Wake `.dev/agent-wake/events.jsonl`: exactly one wake.queued for
  wake-70d850300ee2 at 12:52:36.299Z; the codex adapter process exited and the
  opencode fallback relaunched at 13:08:02.907Z. Queued once; retried launch.

## Findings

1. Legitimate latency wait, not a stall. The build was in flight at the 60s mark
   (mid-link after a 30.657s compile) and completed successfully at 68.864s.
2. Reporter evidence gaps (reported, not fixed):
   - `reason` is a raw truncated worker-output tail (`55770.586,...`), not a
     readable reason. Owner: `_record_game_build_output` stores
     `line.rstrip()[-240:]` (devscripts/dev-loop.py:2833/2863) and the wait dict
     reuses it as `reason` (dev-loop.py:3539).
   - `generation` is stale: evidence says 490 while the attempt built 491.
     `game_build_generation` is only set at result parsing (dev-loop.py:2976).
   - `build_id` is `latest_build` (prior published build), not the in-progress
     attempt; the attempt id is only inside the malformed reason fragment
     (dev-loop.py:3543).
   - no `run_id` in the wait evidence.
   - the expected dev-loop wait event is absent: `record_runtime_event`
     (dev-loop.py:2275-2300) targets
     `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl`, but that directory
     does not exist and the OSError is swallowed.
3. The fixed 60s budget is shorter than a normal heavy-TU edit build
   (68.864s and 61.407s here). Same systemic class as issue-136d4553ff92.
4. issue-f200660c97fe is a different wait occurrence, not a duplicate.

## Claims

- Source evidence: review-only (read of devscripts/dev-loop.py,
  devscripts/build_friction.py, build timings).
- Build evidence: attempt game-dll-20261122T125133309168Z-46b1e5, generation 491,
  68.864s success.
- Runtime evidence: agent-wake events only; no gameplay runtime required.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-with-evidence-gaps` to
  issue-bbaee7b0d2ee; state `in_review`. Pinned `related_build` to the exact
  attempt and `related_run` to 20261122_011031.
- Added occurrence evidence to build owner issue-136d4553ff92.
- No duplicate issue created; no code fix claimed.
