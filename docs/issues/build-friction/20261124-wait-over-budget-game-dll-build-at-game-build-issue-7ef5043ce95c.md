# issue-7ef5043ce95c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-observability)

### 2026-11-24T21:27:28.906Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-c29456cabf35). Read-only: no source edit, no build or test started by this agent, no fix or closure claimed.

OBSERVED FACTS (direct, pinned)
1. Wait record: wait_id game-dll-build:2026-11-24T20:19:59.371Z, kind game-dll-build, stage game_build, elapsed 60.211s, budget 60.0s, started_utc 2026-11-24T20:19:59.371Z, source_generation 16, request_ids ["2026-11-24T20:19:58.861Z-b4c97b8e:mode3:game"]. Issue issue-7ef5043ce95c opened 2026-11-24T20:20:59.678Z by dev-loop-wait.
2. The wait was a genuine, progressing build, not a stall. Attempt game-dll-20261124T201959398000Z-aa0b47 started 2026-11-24T20:19:59.398Z, completed 20:21:11.411Z, success=true, total_seconds=72.013, generation=616, revision=fcb867d5172199f09ff5fa05f74c36acc73c5f0a5df3ae85dbf209eb3548de33, no_op=false. Source: .dev/build-timings/game-dll-20261124T201959398000Z-aa0b47.json. The 60s budget was crossed at 60.211s while compile was still running; the build finished ~11.8s later.
3. Phase split (same JSON, phase_durations_ms): compile 47.667s, link 16.721s, object_cache_scan 1.494s, publish 0.406s, coherence_validation 0.356s, toolchain_setup 0.095s. 4 TUs compiled / 641 skipped. Three network TUs were rebuilt because their dependency files were missing/invalid (cache_invalidations): multiplayer-tick.cpp 39.651s, multiplayer-packets.cpp 37.539s, multiplayer-shots.cpp 30.796s; identity-provider 7.966s.
4. Resource sample (game-dll-20261124T201959398000Z-aa0b47.resource.json; 66 samples @1s over 72.003s): host CPU mean 99.934%, peak 100.0%, minimum available memory 178,499,584 B (~170.2 MiB), jobs=10, no sampler errors. runtime_alignment: same_run_server_client=true, server authoritative mode bloodtest (run 20261124_195553, server pid 13680), client pid 35812, client mode stale_or_unreported -> status mode_unverified. A live 1-NPC/1-player pair was running during the build (correlation only; causation unproven).
5. Reviewer wake: wake-c29456cabf35 queued exactly once at 2026-11-24T20:21:00.308Z; codex claimed/launched 20:21:11.629/11.698Z then exited; wake.retry 20:25:24.985Z ("previous adapter process exited; trying fallback"); opencode claimed/launched 20:36:52.814/.889Z. .dev/agent-wake/requests/wake-c29456cabf35.json attempts=2. Queue-to-final-launch 952.6s (~15m52.6s); two wake.launched rows for one wait_id.
6. Proof expectation ("dev-loop wait event plus issue record and reviewer wake") is only partly met (2 of 3). The issue record and the wake request are durable, but no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event for this wait_id/attempt id exists in any logs/**/dev-loop/events.jsonl: a repo-wide search for aa0b47 returns nothing under logs/, and game-dll-build:2026-11-24T20:19:59.371Z appears only in .dev/agent-wake/requests/wake-c29456cabf35.json. By contrast the next over-budget occurrence (20:55:04.680Z, issue-98a885a0bb81) WAS journaled in logs/2026-11-24/20261124_205541_01/dev-loop/events.jsonl with a readable reason, so the omission is per-instance (events_path None/ENOENT variant), not universal.

EVIDENCE-QUALITY DEFECTS (already tracked; no new issues created)
- reason is a mid-JSON worker tail: '4158.436,"build_attempt_id":"game-dll-20261124T201959398000Z-aa0b47","source_generation":16,"generation":616,...,"progress_write_ms":3.913,"record_overhead_ms":4.454}' -> recurrence of issue-6e08d89b55e3 (dev-loop.py sets game_build_detail to the last 240 chars of a worker line).
- build/run identity: evidence.build_id 2026-11-24T19:27:02.742428Z is the previously published EXE build id and evidence.generation is null, while the actual DLL attempt is game-dll-20261124T201959398000Z-aa0b47 / generation 616 (recoverable only from the reason tail). Same class as issue-a0fa88737830 / umbrella issue-9a4503f6ed68. No run ID is recorded (N/A for a compile wait).
- missing dev-loop wait event -> issue-6e2d7b91e7d9 (duplicate issue-0498cbef4ef3).
- late + double reviewer dispatch (15m52.6s, two wake.launched for one wait_id) -> issue-84c584957e65.

RECURRENCE / DUPLICATION
- No other issue carries this wait_id; per-wait dedupe held. The recurring cost is the flat 60s policy: every incremental whole-game DLL rebuild that legitimately exceeds 60s mints a new high-priority build-friction issue plus a reviewer wake. Current board: 220 build-friction issues, ~167 titled "Wait over budget: game-dll-build at game_build". This occurrence is a normal ~72s build (47.7s compile + 16.7s link), so the policy fires on healthy progress, not a hang. Policy/consolidation is owned by issue-9a4503f6ed68; end-to-end build latency by issue-136d4553ff92; host CPU/resource pressure by issue-ed3ac775283d (which already records this exact attempt's CPU/RAM data). Gap key build-resource-sampling:game-dll-20261124T201959398000Z-aa0b47 (missing matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation) is substantively covered there; appended once for discoverability.

INFERENCE / UNCERTAINTY
- The wait was real and expected: a one-source change rebuilt three network TUs whose dependency files were missing/invalid, at ~100% host CPU with a live pair running. No defect found in the wait detector, issue creation, or the exactly-once wake queue for this occurrence.
- Causality between host pressure and the 72s duration is not proven (client mode unverified; no matched prebuild baseline). Treat as correlation only.

NEXT DISCRIMINATING CHECK (not performed here)
- After the missing-depfile invalidation is understood or fixed (issue-136d4553ff92), repeat a same-scope incremental build and confirm the network TUs are not needlessly recompiled. Then confirm the next over-budget wait's evidence.build_id/generation equal the in-flight attempt AND that a dev-loop.wait.over_budget event is present in the owning run journal. Identity/journal are fixed only when both hold for a real attempt.

CONFIDENCE
- High: exact wait/attempt identity, phase timings, success/gen616, wake queue-once + two launches, reason/identity defects, and that no dev-loop wait event was journaled for this occurrence.
- Medium: events_path None/ENOENT is inferred from missing events plus current daemon state; the exact dropped write was not captured.

No source edited; no code fix claimed; the live bloodtest pair was left untouched.
