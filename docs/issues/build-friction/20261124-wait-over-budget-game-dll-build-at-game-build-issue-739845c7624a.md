# issue-739845c7624a: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-policy-noise)

### 2026-11-24T22:58:21.586Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: confirmed wait-policy false positive, not a build stall (recurring).

Observed occurrence
- wait_id game-dll-build:2026-11-24T22:06:22.495Z, stage game_build, elapsed 61.269s vs budget 60.0s.
- Journal C:\mimita-v9\logs\2026-11-24\20261124_215719_01\dev-loop\events.jsonl lines 72-73: dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake at 2026-11-24T22:07:31Z.
- Actual build (game-dll-20261124T220623155095Z-9c49ca, generation 626) completed 2026-11-24T22:11:43.208Z with duration_seconds=319.177, worker_total_seconds=283.786 (same journal, dev-loop.game-generation.ready). The 61.269s figure is only the first over-budget polling tick; the compile genuinely runs ~5 minutes.
- Sibling build generation 627: duration_seconds=300.759, worker_total_seconds=270.433 (logs/2026-11-24/20261124_221014/dev-loop/events.jsonl).

Root cause (owner: dev-loop wait policy)
- WAIT_ISSUE_SECONDS defaults to 60s (devscripts/dev-loop.py:114-115) and is applied unchanged to game-dll-build (devscripts/dev-loop.py:3884-3900).
- wait_id is keyed on game_build_started_utc (devscripts/dev-loop.py:3889), so every whole-game DLL build that exceeds 60s files a fresh issue.
- Recurrence already open on the same signature: issue-f995213a15b0 (wait started 2026-11-21T15:25:31.164Z, elapsed 60.676s) and issue-121f867b430a (wait started 2026-11-21T15:19:35.672Z, elapsed 60.236s), both state=new. This is the third instance; no further duplicate issue should be created.

Acceptance check
- Required fields present: wait_id, stage, reason, elapsed, budget_seconds, build_id, generation, source_generation, request_ids, started_utc. issue readiness: ready=true, missing=[].
- Reviewer wake queued exactly once for this occurrence: wake-1aabfb035087, event wake.queued at 2026-11-24T22:07:24.897Z (.dev/agent-wake/events.jsonl:2932). Acceptance met for this wait.

Recommended fix (not implemented; needs owner + human approval; report separately)
- Give game-dll-build a build-appropriate budget (>= observed p95, currently ~320s; e.g. >=600s) or switch to stall-progress detection (no progress for the budget window) instead of total elapsed. This keeps genuine-stall detection while ending one issue per normal build.
- Optionally coalesce duplicate open "game-dll-build at game_build" issues rather than opening one per wait_id.

Unknowns
- Whether a per-stage progress heartbeat already exists that can distinguish a real stall from a long compile; not verified here.

No source edited; dev-loop/runtime untouched. Review-only.
