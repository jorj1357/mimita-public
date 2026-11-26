# issue-af48c43ce6c5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-friction-real-build-over-60s-budget)

### 2026-11-26T00:14:06.987Z — build-wait-reviewer

Reviewer: build-wait-reviewer

(no body)

## Review (confirmed-build-friction-real-build-over-60s-budget)

### 2026-11-26T00:15:05.248Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-ec92bcb2152d) at 2026-11-26T00:14Z. Read-only: no source edit, no build/test started by this agent, no fix/closure claimed. (This re-appends the intended body; the first review row for this wake was recorded with an empty body because the CLI needs --body rather than stdin.)

ACCEPTANCE / PROOF-EXPECTATION CHECK (observability-friction record, not a behavior claim)
- All fields present; issue readiness ready=true, missing=[]. Issue carries wait_id game-dll-build:2026-11-26T00:03:44.421Z, stage game_build, elapsed 60.869s, budget 60.0s, build_id 2026-11-25T23:04:22.955793Z, generation 657, source_generation 23, request_ids [2026-11-26T00:03:43.880Z-566cf4c2:mode3:game].
- Dev-loop journal logs/2026-11-25/20261125_234424/dev-loop/events.jsonl line 184: dev-loop.wait.over_budget created issue-af48c43ce6c5 (created=true). Line 183: exactly one dev-loop.wait.reviewer-wake for this wait_id (queued=true, adapter=codex, wake-ec92bcb2152d). Proof expectation (wait event + issue record + reviewer wake) is satisfied on the queue side.

THE WAITED-ON ATTEMPT WAS REAL WORK, NOT A STALL/NO-OP
- Attempt game-dll-20261126T000344661706Z-d96bf3: started 2026-11-26T00:03:44.662Z, completed 00:05:25.219Z, success=true, failed_phase="", no_op=false, generation 658, total_seconds=100.563.
- Phase durations: compile 75.235s, link 7.953s, publish 0.531s, object_cache_scan 2.875s, coherence 0.266s.
- Slowest compiles: src/vehicles/vehicle-system.cpp 73.453s; src/network/vehicle-networking.cpp 51.885s; vehicle-config.cpp 49.648s; vehicle-commands.cpp 44.038s; vehicle-profile.cpp 39.730s; vehicle-hud.cpp 29.098s; module-identity-provider.cpp 1.701s. compiled=7, skipped=638, source_count=644, jobs=10, compile_sum=289.554s.
- Build note bbe0390215ee (rocket-league-landing) changed src/vehicles/vehicle-profile.h, invalidating 6 vehicle TUs plus vehicle-networking. A single TU (vehicle-system.cpp) needs 73.45s, above the 60s budget by itself, so the wait fired predictably mid-compile. First divergence: the flat 60s WAIT_ISSUE_SECONDS is below the natural compile cost of the changed vehicle TUs; nothing is stalled.

EVIDENCE IDENTITY NOTE (not a defect; reviewers must key on the right record)
- The issue's evidence snapshot records build_id 2026-11-25T23:04:22.955793Z and generation 657 (the previous success) because the attempt was still in progress when the wait fired. The authoritative record for this occurrence is .dev/build-timings/game-dll-20261126T000344661706Z-d96bf3.json (generation 658) and .dev/build-timings/game-dll-20261126T000344661706Z-d96bf3.resource.json (captured, 75 samples, mean host CPU 99.982%, min available mem ~1.08GB).

REVIEWER WAKE LIFECYCLE (recurrence of known dispatch defects, issue-84c584957e65)
- wake-ec92bcb2152d queued once 00:04:47.254Z (.dev/agent-wake/events.jsonl L3357); codex claimed/launched 00:04:48.885Z/00:04:49.015Z pid 31080; wake.retry "stale launched request recovered by scheduler" 00:06:21.150Z; opencode claimed/launched 00:06:22.288Z/00:06:22.335Z pid 36248 (this reviewer). Queue-to-final-launch 95.081s, within the <=5min target; the exactly-once queue held. However two wake.launched rows exist for one wait_id (duplicate-launch target miss), and the dev-loop.wait.reviewer-wake launch_result tail embeds the previous wait's wake (issue-4ab3b491ad6c / game-dll-build:2026-11-25T23:04:23.764Z), not wake-ec92bcb2152d - the known build_friction.py launch_result misattribution.

DUPLICATION / POLICY FINDING
- At least 158 open build-friction issues share the exact title "Wait over budget: game-dll-build at game_build". The wait_id is per attempt (game-dll-build:<started_utc>), so every >60s real vehicle-fanout build files a new issue. This materializes the recorded risk "A noisy wait policy could create duplicate friction issues."
- Existing owners: issue-7d3dcfd7433b (owner launch-latency-investigation, wait-stage latency), issue-136d4553ff92 (owner build-coordinator, DLL build stage/speed umbrella), issue-84c584957e65 (reviewer dispatch), issue-d220df93f7cb (same class, build-coordinator). No duplicate issue created.

RECOMMENDATIONS (owner decision; no fix claimed here)
1. For game-dll-build, do not use a flat 60s: scale the threshold to observed p95 real compile (e.g. 120-180s) or to a predicted time from changed-TU count, and suppress a new issue while the attempt is progressing (no_op=false and phases advancing).
2. Dedupe consecutive same-kind over-budget waits into one class issue per root cause per time window instead of one per started_utc.
3. Consolidate the >=158 duplicate titles into the umbrella/class owners above.

CONFIDENCE
- High: wait/wake exactly-once on the queue side, real (non-stalled) build, phase/slowest-TU timings, duplicate-title count.
- Medium: the best budget/dedupe policy.
- No runtime/behavior proof applies to this record; active game processes were left untouched.
