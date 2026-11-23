# issue-3d02ab0de320: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-23T17:28:01.017Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Build-wait review — issue-3d02ab0de320 (game-dll-build at game_build). Agent: build-wait-reviewer. Evidence gathered 2026-11-23 ~17:35Z. No source edited.

WHAT HAPPENED (direct observation)
- `dev-loop.wait.over_budget` wait_id `game-dll-build:2026-11-23T16:54:31.578Z`, stage game_build, elapsed 60.722s vs 60.0s budget, fired ~16:55:39Z. Journal: `logs/2026-11-23/20261123_144429/dev-loop/events.jsonl`.
- The waited-on build did NOT hang: attempt `game-dll-20261123T165431661826Z-6c221c` reached `dev-loop.game-build.result status=ok` at 16:56:00.757Z and `dev-loop.game-generation.ready generation=565 success=true duration_seconds=90.92` at 16:56:03.002Z. Compiled 29, skipped 614; compile 76.122s; link 6.27s. Summary `.dev/build-timings/game-dll-20261123T165431661826Z-6c221c.json`.
- 60s is below a normal partial DLL rebuild here: slowest TUs server-gamemode 48.345s, map-editor 41.998s; queue_wait_ms reached ~55.6s with 10 jobs. Same compile-time/header-fanout surface as issue-136d4553ff92, not a stalled worker.
- Reviewer wake was queued exactly once for this wait: `wake.queued wake-7c08ac9af12c` issue issue-3d02ab0de320 at 16:55:33.005Z. Acceptance "exactly once" holds.

DEFECTS IN THE WAIT RECORD (follow-up needed; not fixed here)
1. `reason` is a truncated raw worker JSON tail: `9849.915,"build_attempt_id":"game-dll-...6c221c",...,"record_overhead_ms":4.976}`. Root: dev-loop.py:3112 sets `detail = line.rstrip()[-240:]` for every worker line and dev-loop.py:3141-3143 overwrites `game_build_detail`; the wait copies it at dev-loop.py:3874. Sibling issue-8fdc520a07c5 got a clean stage line, so quality is nondeterministic.
2. `build_id`/`generation` are stale for a game-DLL wait: evidence records build_id `2026-11-23T13:26:39.822193Z` and generation 564 while the active attempt is `game-dll-...6c221c` generation 565. Root: dev-loop.py:3878-3879 uses `self.latest_build` and `self.game_build_generation` (last published). The true attempt id appears only inside the contaminated reason.
3. Reviewer activation latency: wake created 16:55:32.987Z, claimed by codex 16:55:39.720Z, then `wake.retry` "stale launched request recovered by scheduler" 17:00:35.582Z, and the opencode launch only at 17:12:03.009Z (~16.5 min after queue). Journal `.dev/agent-wake/events.jsonl`. Not a duplicate wake, but the reviewer acted late, and the 16:55:39 `dev-loop.wait.reviewer-wake` launch_result shows a different queued agent (mode3-latency-goal wake-9724b287e9d8), so the recorded launch result is not matched to the requested wake. (Inference: single-runner backlog / launch-result matching; owner confirmation needed.)

RECURRENCE
Three game-dll-build wait issues were opened today for the same threshold: issue-991c632f1ca0 (16:28Z), this one (16:54Z), issue-8fdc520a07c5 (17:10Z). Dedup keys on wait_id (`build_friction.py:131-169`) and each attempt has a unique started_utc, so every >60s rebuild creates a new high-priority issue. With a 60s budget against ~90s typical builds this is guaranteed noise.

RECOMMENDATION (not applied)
- Build a curated `reason` (stage + attempt id + compiled/total + current TU) instead of the raw 240-char line tail.
- Record the active game-build attempt id/generation on the wait, not the last published one.
- Raise WAIT_ISSUE_SECONDS above observed normal build time, or dedup by (kind, stage) within a window so one slow-build episode is one issue.
- Match wake.reviewer-wake launch_result to the requested wake id.
Cross-link: issue-136d4553ff92 (build stage accounting/speed), sub-task `dll-build-stage-accounting` (owner build-coordinator, open).

CONFIDENCE
High on build result and reason/ID contamination (direct journal + summary JSON). Medium on wake-delay root cause (event sequence direct; mechanism inferred).

No proof claim: this is a tooling/observability review. The wait evidence is journal-tier, not a behavior proof.

## Agent discussion and suggestions

### 2026-11-23T17:29:01.191Z — build-wait-reviewer

Message: `msg-2159aa791f55` (note)

Subject: Wait-record defects found reviewing issue-3d02ab0de320

Reviewed issue-3d02ab0de320 (game-dll-build wait 60.722s/60s, g565). Build succeeded in 90.92s (compile 76.1s); not a hang. Two record defects for the build owner: (1) wait reason is a truncated raw worker JSON tail because game_build_detail=line[-240:] at dev-loop.py:3112/3142 and is copied at 3874; (2) wait build_id/generation are the last published values (564 / 2026-11-23T13:26:39.822193Z) not the active attempt game-dll-...6c221c generation 565 (dev-loop.py:3878-3879). Also three identical 60s game-dll wait issues were opened today (991c/3d02/8fdc) because dedup keys on unique wait_id. Suggest curated reason+active attempt id and a budget/dedup window. Cross-linked to issue-136d4553ff92. Details in the issue review; no code changed.
