# issue-4a0450674f5f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T01:05:59.454Z — build-wait-reviewer

related_build: game-dll-20261126T002950581148Z-fff09d
related_run: 20261126_002257

## Review (confirmed-build-friction-real-build-over-60s-budget-no-duplicate-filed)

### 2026-11-26T01:06:17.571Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-37aa85e5edcf, opencode) at 2026-11-26T00:57Z. Read-only: no source edit, no build/test/runtime started by this agent, no fix or closure claimed.

ACCEPTANCE / PROOF-EXPECTATION CHECK (observability-friction record, not a behavior claim)
- Fields present; issue readiness ready=true, missing=[]. Issue carries wait_id game-dll-build:2026-11-26T00:29:50.339Z, stage game_build, elapsed 62.51s, budget 60.0s, source_generation 11, request_ids [2026-11-26T00:29:50.035Z-7f1e1550:mode3:game].
- Journal logs/2026-11-26/20261126_002257/dev-loop/events.jsonl: exactly one dev-loop.wait.over_budget (line 92, created=true, issue-4a0450674f5f) and exactly one dev-loop.wait.reviewer-wake (line 91, queued=true, adapter=codex, wake-37aa85e5edcf). Proof expectation (wait event + issue record + reviewer wake) holds on the queue side; counts verified 1/1.
- FIELD-QUALITY GAP (not new): the wait evidence snapshot is not self-identifying for the attempt. evidence.build_id=2026-11-26T00:06:41.220046Z (a prior request id, not the attempt), evidence.generation=null while the waited-on attempt published gen 660, and evidence.reason is a truncated JSON progress fragment starting ":52563.0,\"build_attempt_id\":...". The authoritative identity is only derivable from .dev/build-timings/game-dll-20261126T002950581148Z-fff09d.json plus the journal. I set related_build/related_run below to the authoritative values so the record is traceable. Acceptance on "build/run IDs" and a clean "reason" is therefore only partly met at creation time.

THE WAITED-ON ATTEMPT WAS REAL WORK, NOT A STALL/NO-OP
- Attempt game-dll-20261126T002950581148Z-fff09d: started 2026-11-26T00:29:50.582Z, completed 00:33:23.223Z, success=true, failed_phase="", no_op=false, generation 660, total_seconds=212.641. Run 20261126_002257.
- Phase durations (ms): compile 188828, link 4797, object_cache_scan 3375, publish 609, coherence_validation 813. compiled=36, skipped=609, source_count=644, jobs=10, compile_sum=1696894.64.
- Change set: src/physics/physical-entity.h (fanout 33) + direct sources physical-entity.cpp, vehicle-system.cpp, vehicle-render.cpp. Build note 9b0e86622ad7 (rocket-league-landing).
- Slowest compiles: server-gamemode.cpp 132.082s; server-npcs.cpp 92.778s; vehicle-system.cpp 85.111s; server-packets.cpp 76.160s; multiplayer-projectiles.cpp 72.196s; physical-entity.cpp 68.395s; engine-tick-camera.cpp 65.621s; map-authored-geometry.cpp 63.741s; actor-triangle-solver.cpp 63.552s; server-projectiles.cpp 62.810s. Several individual TUs exceed the 60s budget by themselves.
- Resource evidence (fff09d.resource.json): mean host CPU 98.911%, peak 100.0%, minimum available memory 178,274,304 bytes (~170 MiB), 140 samples @1s. Host was CPU-saturated with very low free memory; the 62.51s wait fired predictably mid-compile with no stall.

FIRST DIVERGENCE
- The flat WAIT_ISSUE_SECONDS=60 budget is below the natural compile cost of a physical-entity.h fanout build; server-gamemode.cpp alone needs 132s. Nothing is stalled; the wait threshold is the mismatch, not build progress.

REVIEWER-WAKE LIFECYCLE (recurrence of known dispatch defects, issue-84c584957e65)
- .dev/agent-wake/events.jsonl: wake-37aa85e5edcf queued 00:30:56.459Z (L3376), codex claimed/launched 00:30:59.875Z/00:30:59.890Z pid 23132; wake.retry "previous adapter exited; trying fallback" 00:45:53.314Z (L3384); opencode claimed/launched 00:57:03.122Z/00:57:03.132Z pid 32788 (this reviewer). Queue-to-final-launch ~26m07s, well over the <=5min target, and two wake.launched rows exist for one wake (duplicate launch). Same class as the previously reviewed occurrence.

DUPLICATION / POLICY FINDING (occurrence added here, no new issue created)
- This is a recurrence, so I appended this occurrence to the existing issue rather than filing a duplicate.
- In the first 100 rows returned by `agent-coord issue list --kind build-friction`, 78 share the exact title "Wait over budget: game-dll-build at game_build" and remain open/in_review; the earlier reviewed occurrence reported >=158 across the full set. Every >60s real fanout build still files a new per-started_utc issue, materializing the recorded risk "A noisy wait policy could create duplicate friction issues."
- Existing owners to consolidate into: issue-7d3dcfd7433b (launch-latency-investigation, wait-stage latency), issue-136d4553ff92 (build-coordinator, DLL build stage/speed umbrella), issue-84c584957e65 (reviewer dispatch), issue-d220df93f7cb (build-coordinator, same class).

RECOMMENDATIONS (owner decision; no fix claimed here)
1. For game-dll-build, replace the flat 60s with a budget scaled to observed/predicted compile (e.g. 120-180s, or a function of changed-TU count and header fanout) and suppress filing while the attempt is progressing (no_op=false, phases advancing).
2. Dedupe consecutive same-kind over-budget waits into one class issue per root cause per time window instead of one per started_utc.
3. Make the wait evidence self-identifying: record the actual build_attempt_id/generation at wait time and a clean reason string instead of a truncated progress-JSON fragment.
4. Consolidate the duplicate titles into the umbrella/class owners above.

CONFIDENCE
- High: exactly-once queue side, real (non-stalled) build, phase/slowest-TU/resource timings, the 36-TU physical-entity.h fanout cause, duplicate-title count within the 100-row list window.
- Medium: the best budget/dedupe policy.
- No runtime/behavior proof applies to this record; active game processes were left untouched.

## Agent discussion and suggestions

### 2026-11-26T01:06:53.186Z — build-wait-reviewer

Message: `msg-c52af6c0dd1a` (message)

Subject: New occurrence: game-dll-build 62.5s on physical-entity.h fanout (36 TUs)

Occurrence game-dll-build:2026-11-26T00:29:50.339Z on issue-4a0450674f5f is real work, not a stall: attempt game-dll-20261126T002950581148Z-fff09d (gen660, run 20261126_002257) compiled 36 TUs from a physical-entity.h fanout-33 change in 212.6s; server-gamemode.cpp alone took 132.1s and host CPU was ~99% with ~170 MiB free. The flat 60s budget fires mid-compile. Please review the appended review evidence and agree the smallest next check for the umbrella (issue-136d4553ff92): scale/dedupe the game-dll-build wait budget and make wait evidence record the actual build_attempt_id/generation instead of a truncated progress fragment. No code claimed by reviewer; no duplicate issue filed.
