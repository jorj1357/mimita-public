# issue-04ffbd3f90ed: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-wait-not-stalled; fields-complete; reviewer-wake-queued-once; journal-recorded-issue-failed-instead-of-over_budget; cause-wake-once-30s-timeout; build-cost-header-fanout+memory-pressure)

### 2026-11-28T08:27:37.704Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded reviewer review (build-wait-reviewer, wake-5406ee6c4908). Read-only: no source edit, no build, no runtime command, no close, no fix claim.

WAIT / ISSUE IDENTITY
- wait_id game-dll-build:2026-11-28T07:25:59.635Z; stage game_build; kind game-dll-build
- budget 60.0s; elapsed 60.624s; issue created 2026-11-28T07:27:00.762Z
- attempted build game-dll-20261128T072559701228Z-93ccd0; request build-request-aaf96c379975; generation 812; source_generation 9
- changed paths: src/network/multiplayer-physical-entities.cpp, src/network/vehicle-networking.cpp, src/network/schema/packet-vehicle.h (header fanout 41)

ACCEPTANCE CHECK (fields all present)
- wait_id, stage, reason, elapsed, build/run IDs, evidence: present in the issue record.
- Reviewer wake queued exactly once: YES. .dev/agent-wake/requests/wake-5406ee6c4908.json, one wake.queued at 2026-11-28T07:27:04.977Z; later claimed/launched by the adapter fallback chain (codex 07:41:06, opencode 08:20:38). No duplicate wake for this wait_id.

BUILD OUTCOME (not a stalled/failed build)
- .dev/build-timings/game-dll-20261128T072559701228Z-93ccd0.json: success=true, total 400.078s, generation 812 published.
- compile 384.109s, link 3.063s; 163 TUs compiled, 484 skipped.
- slowest TUs: server-gamemode.cpp 77.42s, map-editor.cpp 51.755s, server-npcs.cpp 47.126s.
- Cost driver: the packet-vehicle.h edit invalidated 32 dependents listed in cache_invalidations; with the 41-source fanout it forced a large recompile.

FIRST DIVERGENCE: journal recorded a false failure
- The only journal event carrying this wait_id is logs/2026-11-28/20261128_061051/dev-loop/events.jsonl:409: event=dev-loop.wait.issue-failed, wall_time 2026-11-28T07:27:35.365Z, error="Command '[python, devscripts/agent-wake.py, run, --once]' timed out after 30 seconds", reason "could not record over-budget wait".
- No dev-loop.wait.over_budget and no dev-loop.wait.reviewer-wake exist for this wait_id, even though the issue and wake request both exist.
- Root cause located in devscripts/build_friction.py:244-246 (wake_reviewer): after the wake `add` succeeds (line 237), it runs `agent-wake.py run --once` synchronously with timeout=30. On this host that launch blocks >30s and raises TimeoutExpired; dev-loop.py:3946 catches it and writes wait.issue-failed instead of the expected wait.over_budget/reviewer-wake pair. The wait_id was already added to reported_wait_ids (dev-loop.py:3929), so there is no retry/duplicate; the failure is a false-negative bookkeeping gap, not a lost wait.
- This is a distinct variant of issue-6e2d7b91e7d9 (dropped wait events). Evidence appended there.

RESOURCE PRESSURE (dominant measured cause of exceeding 60s)
- resource.json: mean host CPU 97.653%, peak 100.0%, minimum available memory 16.2 MB (17,010,688 bytes).
- postmortem seconds: compile 384.109s, queue_wait_max 361.078s, queue_wait_mean 176.629s. The worker pool was starved; a 60s budget is below any header-fanout rebuild under this pressure. Relevant owner issue: issue-2a5c5da53565 (dev-loop reliability under high CPU/memory).

MINOR EVIDENCE GAPS (do not block acceptance)
- The issue evidence build_id is "2026-11-27T17:45:19.700118Z" (a prior build) while generation is 812 (the new attempt game-dll-20261128T072559701228Z-93ccd0); the attempt id is not the build_id. Traceable but not one-to-one.
- reason is raw truncated worker JSON (":60109.0,\"build_attempt_id\":..."), matching issue-6e08d89b55e3.

DISPOSITION
- Acceptance met: one issue, one reviewer wake, evidence present.
- Not a stalled build; no build fix required for this occurrence.
- Code fix needed under separate ownership: make build_friction.wake_reviewer treat the immediate `run --once` launch as best-effort (catch TimeoutExpired, keep queued=True), and/or record wait.over_budget before waking, so a slow adapter launch cannot mask a successful wait record. Reported only; not claimed by this review.
