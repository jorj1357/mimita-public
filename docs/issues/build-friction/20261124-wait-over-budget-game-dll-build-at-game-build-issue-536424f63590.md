# issue-536424f63590: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-observability)

### 2026-11-24T20:08:00.140Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review (wake-76763ae5a22d, delivered via opencode fallback) at 2026-11-24T19:56Z. Read-only: no source edit, no build or test started by this agent, no fix or closure claimed.

ACCEPTANCE CHECK (observability/friction record, not behavior proof)
- wait_id game-dll-build:2026-11-24T19:41:25.941Z, kind game-dll-build, stage game_build, elapsed 61.335s, budget 60.0s, started_utc 2026-11-24T19:41:25.941Z, source_generation 48, request_ids ["2026-11-24T19:41:25.702Z-c6671473:mode3:game"] are present.
- Journal logs/2026-11-24/20261124_192708/dev-loop/events.jsonl: dev-loop.wait.over_budget for this exact wait_id, issue issue-536424f63590, created=true, wall_time 19:42:45.433Z; exactly one dev-loop.wait.reviewer-wake for this wait_id at 19:42:45.432Z.
- .dev/agent-wake/events.jsonl: exactly one wake.queued for this issue/wait (wake-76763ae5a22d, 19:42:28.524Z); codex claimed/launched 19:42:45.151Z/19:42:45.252Z then exited; wake.retry 'stale launched request recovered by scheduler' 19:55:42.336Z; opencode claimed/launched 19:55:43.646Z/19:55:43.762Z; requests/wake-76763ae5a22d.json records attempts=2. Queue-to-final-launch 795.24s (~13m15s). One queue entry per wait occurrence held; two launches for one wait_id did not.
- Proof expectation (wait event + issue record + reviewer wake) is satisfied only by cross-referencing the wake journal, because the dev-loop reviewer-wake event does not pin the queued wake id (see defect 3).

FIRST DIVERGENCE / CAUSE OF THE WAIT (attempt game-dll-20261124T194125987951Z-4d7bb0)
- Mode 3 batch sealed at 19:41:25.702Z (-c6671473) included changed sources src/entities/player-loader.cpp, src/physics/physical-entity.cpp and changed header src/entities/player.h. player.h fans out to 233 dependent sources; 231 were prioritized and 234 TUs compiled (411 skipped).
- The wait fired 61.335s in while compile was still running: changed_source_preflight 34.046s, compile 781.06s, link 8.29s, object_cache_scan 5.622s, total 801.055s. This is a correct symptom of a genuinely slow header-fanout rebuild, not a stalled or hung wait.
- Build succeeded: generation 609, build\game\generation-609\game-g609.dll, no_op=false, completed 2026-11-24T19:54:47.042Z. Slowest TUs: server-gamemode.cpp 102.633s, map-editor.cpp 70.966s, avatar.cpp 61.844s, server-npcs.cpp 61.788s.

EVIDENCE-QUALITY DEFECTS (already tracked; no new issues)
1. evidence.reason is not a readable stage summary but the last 240 chars of a worker row: '5213.615,"build_attempt_id":"game-dll-20261124T194125987951Z-4d7bb0",...}'. Source dev-loop.py:3892 reason=self.game_build_detail, set from line.rstrip()[-240:] at dev-loop.py:3112/3142. Appended as an occurrence on issue-6e08d89b55e3.
2. build_id 2026-11-24T19:27:02.742428Z is the latest EXE build id and generation 608 is the pre-build generation, not the DLL attempt (attempt published generation 609). check_wait_budget() reads self.latest_build/self.game_build_generation. Same defect as issue-a0fa88737830; the 'build/run IDs' acceptance wording is not cleanly met.
3. dev-loop.wait.reviewer-wake embeds launch_result for wake-d6cb2a6d7a8e (issue-a0fa88737830), not wake-76763ae5a22d, because build_friction.wake_reviewer returns the tail of the 'agent-wake.py run --once' drain, which can capture another pending wake. Appended as an occurrence on issue-84c584957e65.

RECURRENCE / DUPLICATION
- No other issue carries this wait_id; per-wait issue dedupe held.
- This is one instance of the recurring over-budget game-dll-build class driven by header fanout, tracked by umbrella issue-136d4553ff92 (<=10s edit-to-live goal). docs/issues/build-friction already holds >200 wait docs; the current 60s policy will keep opening one high-priority issue per occurrence until fanout rebuilds finish under budget. No duplicate issue created.

RESOURCE EVIDENCE (appended to issue-2a5c5da53565)
- .dev/build-timings/game-dll-20261124T194125987951Z-4d7bb0.resource.json: 685 samples @1s over 801.045s, mean host CPU 99.926%, peak 100%, minimum available memory 65,712,128 B (~62.7 MiB) of ~15.65 GiB, jobs=10, no sampler errors. runtime_alignment unavailable/mode_unverified (server authoritative mode bloodtest; client mode stale), so live-workload attribution is unknown. gap_key build-resource-sampling:game-dll-20261124T194125987951Z-4d7bb0.

CONFIDENCE
- High: exact wait/attempt identity, header-fanout cause (233 fanout, 234 compiled), phase timings, successful publish, exactly-once queue, and the reason/build_id/generation/launch_result defects.
- Medium: whether a scoped/raised budget or umbrella consolidation is the preferred response (owner decision).
- No runtime/behavior proof applies; the active bloodtest pair was left untouched.

NEXT CHECK (owners)
1. Fix reason readability under issue-6e08d89b55e3.
2. Record the real DLL attempt id and published generation in check_wait_budget() under issue-136d4553ff92 / issue-a0fa88737830.
3. Capture the wake id returned by agent-wake.py add and put the run --once launch result in a separate field under issue-84c584957e65.
4. Keep reducing src/entities/player.h fanout under issue-136d4553ff92.
