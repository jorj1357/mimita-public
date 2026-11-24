# issue-a6017d163afb: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-24T18:49:19.095Z — build-wait-reviewer

related_build: game-dll-20261124T174700746286Z-2aff70
related_run: 20261124_174426

## Review (reviewed-no-fix_not_a_stall_heavy_header_fanout_build)

### 2026-11-24T18:50:14.092Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed against the exact run journal (run 20261124_174426, dev-loop/events.jsonl) and the pinned attempt summary .dev/build-timings/game-dll-20261124T174700746286Z-2aff70.json.

VERDICT: not a stall, but this occurrence is NOT the ~1s-over-budget noise case seen elsewhere. The build was genuinely heavy: total 2443.672s (~40m44s) for 92 compiled TUs.

TIMELINE (UTC):
- 17:46:58.539 request 2026-11-24T17:46:58.539Z-97b1dd08:mode3:game enqueued.
- 17:46:59.909 game_build wait clock starts (issue wait_id).
- 17:47:00.746 attempt worker started_utc; 17:47:18.961 first timeline build.started (18.2s pre-worker setup gap; same class as the 62.75s sampler-setup finding in issue-d82b02478dff / issue-136d4553ff92, but far smaller here).
- 17:47:21.807 manifest_snapshot started (this is the reason JSON embedded in the wait evidence).
- 17:48:00.531 wait fired at elapsed 60.622s (+0.622s over the 60s budget): 41.57s after build.started and 38.7s into a manifest_snapshot stage that completed at 17:48:41.150 (duration 78.16s). No stall, no hang.
- 18:27:12.919 build.completed; 18:27:44.415 attempt completed_utc. success=true, generation 598, revision ba05f7697ae60e67c8bedb927a78abe1930a945d5c2ba7f763fcaefdb84e3d9e, source_generation 53.

WHY IT WAS LONG: changed_paths includes 3 headers (src/gamemode/map-config.h, src/physics/physical-entity.h, src/vehicles/vehicle-types.h) from the vehicle-rl-physics build note, forcing a broad rebuild. Metrics: compiled=92, compile_seconds=2144.375, slowest TU 984.977s (~16m25s), link_seconds=56.172, changed_source_preflight_seconds=148.156, object_scan_seconds=8.781, total_seconds=2443.672 vs 60s budget.

ACCEPTANCE CHECK:
- wait_id/stage/reason/elapsed: present, but reason is a raw GAME_STAGE JSON, not a human-readable stage/path reason.
- build/run IDs: the wait evidence carried build_id 2026-11-24T17:05:20.935679Z (generation 595, last successful) instead of the in-flight attempt, and the issue had no attempt id or run id. Set related_build=game-dll-20261124T174700746286Z-2aff70 and related_run=20261124_174426 here. Same evidence-quality gap raised in issue-136d4553ff92: wait evidence should carry build_attempt_id.
- reviewer wake exactly once: exactly one wake request exists for this wait (wake-660366817125), so dedupe holds. Dispatch was very late: queued 17:48:03.005; codex claimed/launched 17:48:09.946/17:48:10.012; codex exited; wake.retry fell back to opencode 17:53:06.039; opencode claimed/launched 18:42:32.106/.148. Queue-to-final-launch 3269.143s (~54m29s); attempts=2 for one wait while the request file records fallbacks=[] despite the fallback. Same late-dispatch/adapter-fallback class as issue-d82b02478dff (~39m33s there, worse here); recorded as this issue's occurrence.

ROOT CAUSE: flat WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:114) applied to game-dll-build total elapsed in check_wait_budget's game branch (dev-loop.py:3885-3900), with wait.reason taken from the last GAME_STAGE line. A header-fanout rebuild cannot finish inside 60s by construction, so the flag is guaranteed even while the build is healthy and progressing. Not a hang, not a wait-policy crash.

SMALLEST USEFUL NEXT CHECK (no code edited here; needs human/owner approval): on the canonical flood issue (issue-a236165fa6fc), make the game-dll-build wait progress-aware (flag only after no stage/file-progress event for N seconds) or raise/scale its budget (>=180s, or base + predicted per-TU), emit a clean reason (stage/path + seconds), include build_attempt_id in wait evidence, and collapse recurring identical game-dll-build-at-game_build waits into one rolling issue with occurrences.

CONFIDENCE: high for timings and outcome (read directly from the run journal and the pinned attempt summary). UNKNOWN: whether the 16m25s single TU is a compiler/resource anomaly or simply a huge translation unit; the attempt succeeded and its resource sampling is partial, so no cause is inferred.

## Occurrence

### 2026-11-24T18:51:02.356Z — build-wait-reviewer

Reviewer wake for wait game-dll-build:2026-11-24T17:46:59.909Z (issue-a6017d163afb) was queued exactly once but dispatched very late: wake-660366817125 wake.queued 2026-11-24T17:48:03.005Z; codex claimed/launched 17:48:09.946/17:48:10.012Z then exited; wake.retry fell back to opencode 17:53:06.039Z; opencode claimed/launched 18:42:32.106/18:42:32.148Z. Queue-to-final-launch 3269.143s (~54m29s), fallback-retry-to-launch 2966.109s. attempts=2 and two wake.launched events for one wait_id, while the request file .dev/agent-wake/requests/wake-660366817125.json records fallbacks=[]. Same late-dispatch/adapter-fallback class as issue-d82b02478dff (there ~39m33s). The waited-on attempt game-dll-20261124T174700746286Z-2aff70 was not stalled at trigger time: wait fired 41.57s after build.started during manifest_snapshot; the attempt then succeeded after 2443.672s (92 TUs, header fanout). Reviewer: build-wait-reviewer.

Evidence: {"attempts": 2, "build_attempt_id": "game-dll-20261124T174700746286Z-2aff70", "dev_loop_journal": "logs/2026-11-24/20261124_174426/dev-loop/events.jsonl", "fallback_adapter": "opencode", "fallback_claimed_utc": "2026-11-24T18:42:32.106Z", "fallback_launched_utc": "2026-11-24T18:42:32.148Z", "journal": ".dev/agent-wake/events.jsonl", "launches_for_wait_id": 2, "primary_adapter": "codex", "primary_claimed_utc": "2026-11-24T17:48:09.946Z", "primary_launched_utc": "2026-11-24T17:48:10.012Z", "queue_to_final_launch_seconds": 3269.143, "related_issue": "issue-d82b02478dff", "request_fallbacks": [], "wait_id": "game-dll-build:2026-11-24T17:46:59.909Z", "wake": "wake-660366817125", "wake_queued_utc": "2026-11-24T17:48:03.005Z", "wake_retry_utc": "2026-11-24T17:53:06.039Z"}
