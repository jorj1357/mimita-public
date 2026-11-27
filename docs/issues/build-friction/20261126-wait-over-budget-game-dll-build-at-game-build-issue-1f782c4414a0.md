# issue-1f782c4414a0: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-true-positive)

### 2026-11-26T17:49:12.636Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (activation wake-9bb21344abe0). Read-only review: no source edits, no build requested by this agent, no runtime/behavior claim.

VERDICT: Confirmed true positive. The 60s wait budget was exceeded by a real whole-game DLL compile; the in-flight attempt then completed successfully and published generation 707. The wait record and the single reviewer wake are correct. No duplicate issue was created.

WAIT UNDER REVIEW
- wait_id: game-dll-build:2026-11-26T17:34:13.747Z
- kind/stage: game-dll-build / game_build
- elapsed at record: 62.025s (budget 60.0s)
- request: 2026-11-26T17:34:13.375Z-6ad15143:mode3:game (Mode 3 batch, source_generation 7)
- dev-loop run: 20261126_172851
- dev-loop journal: logs/2026-11-26/20261126_172851/dev-loop/events.jsonl (dev-loop.wait.reviewer-wake, dev-loop.wait.over_budget, dev-loop.game-generation.ready)

TIMELINE (UTC)
- 17:34:13.375 batch request; 17:34:13.747 wait start
- 17:34:13.992 build attempt game-dll-20261126T173413992544Z-de2f00 starts
- 17:35:16.716 issue-1f782c4414a0 created (elapsed 62.025s)
- 17:35:18.878 reviewer wake wake-9bb21344abe0 queued (once)
- 17:35:27.05 codex claimed/launched, then exited
- 17:35:27.530 attempt completed success; 17:35:27.805 generation 707 published (build total 73.547s)
- 17:42:01.9 wake.retry to opencode; 17:43:44.995 claimed, 17:43:45.070 launched
This was a true over-budget build (~2s over at first record, ~13.5s over at completion), not a stalled worker.

ACTUAL BUILD IDENTITY vs RECORDED (identity gap)
- Actual in-flight attempt: game-dll-20261126T173413992544Z-de2f00, generation 707, source_generation 7, revision 71721d54298ca2dd8a96d5e958f67e5eede4efcec497d3954b5a2b7afb42897c.
- Recorded in wait evidence: build_id 2026-11-26T17:27:52.712242Z (previous build id, not the in-flight attempt) and generation 706 (pre-build/active generation; the live server's game.hotness.summary showed generation 706 during the build).
- Root: same known identity gap tracked by issue-7d3dcfd7433b (wait record uses latest build/generation while an attempt is in flight). Not a new defect.
- Reason field is a raw truncated compiler-progress JSON tail ending "...progress_write_ms:8.68,record_overhead_ms:9.38", not a stage summary; tracked by issue-6e08d89b55e3.

WHY IT WAS SLOW (direct evidence from .dev/build-timings/game-dll-20261126T173413992544Z-de2f00.json)
- Changed paths: src/network/vehicle-networking.cpp, src/vehicles/vehicle-profile.cpp, src/vehicles/vehicle-system.cpp, src/vehicles/vehicle-profile.h (header).
- 19 cache invalidations: 16 reason "dependency newer than object: src/vehicles/vehicle-profile.h" and 3 "missing or invalid dependency file" for the three direct sources. One shared-header edit forced the fanout wave.
- 20 compiled / 625 skipped of 644 sources, jobs=10.
- phase_durations: compile 58.860s (compile_sum 537.153s over those 20 TUs), link 2.078s, object_cache_scan 2.938s, publish 0.468s.
- Slowest TUs: map-editor.cpp 52.868s, vehicle-system.cpp 48.697s, main-systems.cpp 46.639s, community-match-client.cpp 41.357s, engine-tick-camera.cpp 40.749s, vehicle-networking.cpp 39.986s.
- Interpretation: a changed shared header produced a ~59s compile wave, so a flat 60s budget is exceeded by construction for this workload. This is expected behavior of the current budget, not a mechanism failure. Exact per-TU-vs-host-saturation split is not isolated (see resource gap below).

RESOURCE SAMPLE (direct observation, .resource.json)
- status captured; 58 samples @1.0s over 73.532s; mean host CPU 94.291%, peak 100.0%; minimum available memory 59,973,632 B (~57.2 MiB).
- runtime_alignment run 20261126_172851: same_run_server_client=true; server PID 12544 mode vehicle_arena, generation 706, 1 player, 0 NPCs; client PID 29500 mode stale_or_unreported, ~373 FPS (374 frames, 13 over the 4.17ms budget). A live vehicle_arena pair was running concurrently.
- unavailable_fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation. These are unmeasured, not zero. CPU saturation is expected from a 10-job compile; this sample does not prove host pressure caused the duration.

ACCEPTANCE CHECK (acceptance: wait_id, stage, reason, elapsed, build/run IDs, evidence; reviewer wake queued exactly once)
- wait_id: pass.
- stage: pass (game_build).
- elapsed: pass (62.025s).
- evidence: pass (issue evidence contains the wait record and request id).
- build/run IDs: partial. request_id and dev-loop run are present, but recorded build_id is stale and generation is the pre-build value (issue-7d3dcfd7433b).
- reason readable: fail. Raw truncated compiler-progress JSON tail (issue-6e08d89b55e3).
- reviewer wake queued exactly once: pass. One wake.queued for wake-9bb21344abe0 at 17:35:18.878Z; codex claimed/launched 17:35:27, exited; fallback opencode claimed/launched 17:43:44/45 after wake.retry. One queue entry, retry only. Dispatch latency (codex primary fail then fallback) is issue-84c584957e65.

RECURRENCE
Over-budget game-DLL wait issues recorded around this window: issue-404bd5b44b6e (17:20:43), issue-45733b82bbbc (17:28:23), this issue (17:35:16), issue-24863e1f595e (17:40:04), issue-450cfc80a0b9 (17:42:01), issue-b73ec4904f0c (17:43:33). Distinct wait_ids, so one issue each is intended, not duplicates. The rate (about six in 24 minutes) indicates the flat 60s budget is systematically below header-fanout build wall time. Umbrella: issue-136d4553ff92 (whole-DLL stage accounting) and issue-9a4503f6ed68 (wait policy).

NEXT STEP / OWNER
- No code fix performed (review scope; no human approval to edit). Smallest checks, ordered: (1) decide whether the 60s budget should differ for header-fanout builds or derive from changed-TU/dependent count; (2) reduce vehicle-profile.h fanout if header hygiene permits; (3) fix wait identity so the recorded build_id/generation name the in-flight attempt; (4) fix the reason text to a stage summary. Owners: issue-136d4553ff92 (build-coordinator) for budget/latency, issue-7d3dcfd7433b for wait identity, issue-6e08d89b55e3 for reason text, issue-84c584957e65 for wake dispatch.

UNKNOWNS
- Exact split between per-TU compile cost and host CPU/memory saturation; no matched prebuild baseline (gap recorded on issue-2a5c5da53565).
- Whether the concurrent live vehicle_arena client/server contributed to the 94.3% mean host CPU cannot be separated without that aligned baseline.
