# issue-a4eea8fe2246: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-observability)

### 2026-11-24T20:48:12.150Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-ad0bab437950, opencode fallback delivery) at 2026-11-24T20:2xZ. Read-only: no source edit, no build or test started by this agent, no fix or closure claimed.

ACCEPTANCE / PROOF-EXPECTATION CHECK (observability/friction record, not a behavior claim)
- wait_id game-dll-build:2026-11-24T20:00:45.963Z, kind game-dll-build, stage game_build, elapsed 60.828s, budget 60.0s, started_utc 2026-11-24T20:00:45.963Z, source_generation 9, request_ids ["2026-11-24T20:00:45.704Z-bb02a2dd:mode3:game"] are present.
- Issue record exists (issue-a4eea8fe2246) and exactly one reviewer wake was queued for this wait: wake-ad0bab437950, .dev/agent-wake/events.jsonl wake.queued 2026-11-24T20:01:47.676Z.
- PROOF GAP: the required dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake events are absent. No logs/**/dev-loop/events.jsonl contains this wait_id, the issue id, or any runtime event in the 20:00-20:03Z window; a repo-wide search for game-dll-build:2026-11-24T20:00:45.963Z returns only .dev/agent-wake/requests/wake-ad0bab437950.json. The "Dev-loop wait event + issue record + reviewer wake" proof expectation is therefore only partly met (2 of 3).
- build/run IDs are also mis-attributed: evidence.build_id 2026-11-24T19:27:02.742428Z is the latest EXE build id, generation is null, and the real DLL attempt id game-dll-20261124T200045986936Z-3f16b3 appears only inside the truncated reason string.

FIRST DIVERGENCE / CAUSE
- The wait is a genuine symptom of a compile still running at 60.8s, not a stalled or hung wait. Attempt game-dll-20261124T200045986936Z-3f16b3 (Mode 3 batch sealed 20:00:45.704Z by request -bb02a2dd, note: vehicle-contact-solver changed src/physics/physical-entity.cpp + config/vehicles/octane_rl.json) started 20:00:45.987Z and ended 20:03:00.380Z, total 134.394s.
- It FAILED at compile: worker_error "compile failed: 1 file(s) in 125.197s"; failed_phase=compile; compile phase 125.2s, object_cache_scan 5.8s, link 0s, publish null. 34 compiled / 595 skipped / jobs 10. Slowest TUs: network/server-npcs.cpp 57.64s, network/server-packets.cpp 48.41s, physics/movement/actor-triangle-solver.cpp 44.54s, procedural/procedural-world.cpp 42.68s, map/map-authored-geometry.cpp 41.18s.
- The compile error is src/entities/player-render.cpp:634:18 error: 'layer' was not declared in this scope (suggestion: Player), in Player::renderCurrentPose. This is a source defect, not a wait/dispatch defect; it is outside this issue's owner scope. Later Mode 3 attempts progressed and the daemon reports game generation 616 published, so it appears transient; no duplicate issue created here.
- MISSING-EVENT ROOT CAUSE: the root dev-loop daemon (pid 30304) restarted at 2026-11-24T19:55:25.909Z and never launched a game run. .dev/state.json has run_id=null and events_path=null, and dev-loop --status shows run=NOT RUNNING, events=(none). devscripts/dev-loop.py record_runtime_event() returns early when events_path is None (lines 2333-2335), so check_wait_budget()'s events at lines 3915 and 3921 were silently dropped. The wait used the game-dll branch while a build was in flight but no launch-run journal was open. This is a fresh occurrence of issue-6e2d7b91e7d9 (root dev-loop drops runtime journal events) in its events_path=None variant rather than the stale-path/ENOENT variant.

REVIEWER WAKE TIMING (recurrence of issue-84c584957e65)
- wake-ad0bab437950 queued exactly once at 20:01:47.676Z. codex wake.claimed 20:01:49.039Z and wake.launched 20:01:49.191Z (pid 24936), then exited; wake.retry 20:07:52.113Z "previous adapter process exited; trying fallback"; opencode wake.claimed 20:12:42.414Z and wake.launched 20:12:42.536Z (pid 7516, this run). requests/wake-ad0bab437950.json attempts=2.
- Queue-to-final-launch 654.86s (~10m54.9s), above the proposed <=5 min bound. Two wake.launched events for one wait_id (the exactly-once queue held; the one-launch target did not). Because the dev-loop wait events were dropped, I cannot check the usual launch_result mis-attribution for this occurrence.

RECURRENCE / DUPLICATION
- No other issue carries this wait_id; per-wait issue dedupe held. The recurring classes are already tracked: missing dev-loop wait event -> issue-6e2d7b91e7d9 (2 prior occurrences); late/duplicate reviewer dispatch -> issue-84c584957e65 (7 prior); reason tail -> issue-6e08d89b55e3 (4 prior); build_id/generation identity -> issue-a0fa88737830. Occurrences appended to each; no duplicate issue created.
- The 60s policy will keep opening one high-priority issue per over-budget compile until the umbrella latency/fanout work lands; this build was a header-triggered rebuild (changed_paths src/physics/physical-entity.h) that also failed.

EVIDENCE-QUALITY / RESOURCE
- reason is again a mid-JSON worker-row tail: ':51674.542,"build_attempt_id":"game-dll-20261124T200045986936Z-3f16b3","source_generation":9,"generation":611,"revision":"d624e3bd..."' (dev-loop.py detail=line.rstrip()[-240:]).
- .dev/build-timings/game-dll-20261124T200045986936Z-3f16b3.resource.json: 99 samples @1s over 134.387s, mean host CPU 99.87%, peak 100%, minimum available memory 160,858,112 B (~153.4 MiB), jobs=10, no sampler errors. runtime_alignment status mode_unverified: server authoritative mode bloodtest (run 20261124_195553, server pid 13680), client mode stale_or_unreported (client pid 35812), so live-workload attribution is unknown. gap_key build-resource-sampling:game-dll-20261124T200045986936Z-3f16b3. Appended to issue-2a5c5da53565.

CONFIDENCE
- High: exact wait/attempt identity, build failure phase and error, phase timings, exactly-once wake queue, two launches, reason/build_id/generation defects, and that no dev-loop journal event was written for this wait.
- Medium: events_path=None is inferred from current daemon state plus no active run; the exact dropped write was not captured in a retained log.
- No runtime/behavior proof applies; the active bloodtest pair was left untouched.

NEXT CHECK (owners)
1. issue-6e2d7b91e7d9: when events_path is None, write wait/over-budget events to a fallback journal (e.g. create_shared_events_path or a dedicated .dev wait journal), or initialize a journal at daemon start; acceptance already requires the wait event in the current run journal.
2. issue-84c584957e65: do not re-drain a failed primary adapter after a multi-minute gap; deliver via the first healthy adapter within the bound and avoid a second wake.launched for one wait_id.
3. issue-6e08d89b55e3: derive reason from the parsed stage/event, not the last 240 chars of a worker row.
4. issue-a0fa88737830: record the real attempt id and attempt/published generation in check_wait_budget().
