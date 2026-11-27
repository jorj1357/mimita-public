# issue-b783370b40b2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-27T06:52:49.437Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-744a51317826, coordination session session-b9307a093673). Read-only: inspected the issue record, the build timing/resource summaries, and dev-loop/wake events; no source edits, no builds, no leases.

VERDICT: confirmed-build-wait. True over-budget wait, not a false positive: the waited-on build was an incremental header-fanout rebuild progressing normally the whole time; the flat 60s wall-clock budget cannot contain a 229-dependent header change.

WHAT THE WAIT WAS
- wait_id game-dll-build:2026-11-27T06:22:39.059Z, kind game-dll-build, stage game_build, elapsed 60.975s vs budget 60.0s.
- In-flight attempt game-dll-20261127T062239069032Z-64213c, source_generation 33, generation 782, revision e3a18537915e5dfcde326e486ee33bccf2a736ff7bb42040302170298d9de49b.
- Trigger request 2026-11-27T06:22:38.829Z-2416ec35:mode3:game (Mode 3 batch). Batch consumed two build notes: 46a9ee8b582e (rocket-league-full, src/input/gamepad-input.cpp) and 27e104c6f0ff (body-rig-ads-cont, src/entities/player.h + player-loader.cpp + aim-commands.cpp + debug-visuals-lines.cpp + debug-visuals-scene.cpp).

TIMELINE (UTC, from .dev/build-timings/game-dll-20261127T062239069032Z-64213c.json/.jsonl)
- 06:22:39.059 wait registered; 06:22:39.069 build_attempt started; 06:22:47.751 first TU submitted (module-identity-provider.cpp); 06:22:50.234 first game TU (debug-visuals-lines.cpp).
- ~06:23:40 wait fires at 60.975s; 12/237 TUs completed (not stalled).
- 06:32:36 last TU completed; 06:32:42.435 dev-loop.game-generation.ready: generation 782 published, success true.
- Progress was healthy across the whole window: 12 TUs by +61s, 36 by +120s, 107 by +300s.

CAUSE / DOMINANT COST (observed)
- Incremental rebuild driven by a shared-header change: current.json changed_header_fanout src/entities/player.h = 229 and src/input/gamepad-input.h = 3. Compiled 237 / skipped 410 of 646; 236 cache invalidations, nearly all "dependency newer than object: src/entities/player.h".
- compile_seconds 588.708 (compile_sum 5,777,408 ms over 237 TUs, 10 jobs), link 4.951s, total 603.156s. Slowest TUs: src/network/server-gamemode.cpp 71.7s, src/vehicles/vehicle-system.cpp 63.5s, src/entities/player-loader.cpp 58.2s.
- The wait fired because the build's natural wall time (603s) far exceeds the 60s budget, not because of a stall.

ACCEPTANCE CHECK (verbatim: "The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence; a reviewer wake is queued exactly once for this wait occurrence.")
- wait_id: PASS (game-dll-build:2026-11-27T06:22:39.059Z).
- stage: PASS (game_build).
- elapsed: PASS (60.975s vs 60.0 budget).
- evidence: PASS (present on the issue).
- reviewer wake exactly once: PASS. One wake.queued for wake-744a51317826 (issue-b783370b40b2) in .dev/agent-wake/events.jsonl (row 3856), then claimed/launched under adapter codex and retried under opencode (this session). No duplicate wake for this wait_id.
- reason: FAIL (readable). evidence.reason is a raw JSON slice starting "60149.03,\"build_attempt_id\":..." derived from dev-loop.py _record_game_build_output detail = line.rstrip()[-240:]. Same defect tracked by issue-6e08d89b55e3; new occurrence recorded there.
- build/run IDs: FAIL. evidence.build_id is "2026-11-27T04:28:12.568764Z" (a previously published build) and generation 781, while the in-flight attempt was game-dll-20261127T062239069032Z-64213c / generation 782; no run_id is recorded (no game run was active between 06:21:35 and 06:33:42, so the dev-loop wait event also has no run-dir journal of its own). Same stale-identity defect as issue-3878b275e5d9 and issue-f36679f4ba95.
- Net: wait_id/stage/elapsed/evidence and exactly-one reviewer wake pass; readable reason and correct in-flight build/run identity still fail. Acceptance therefore NOT fully met yet, but the wait occurrence itself is valid.

PROCESS FINDING
- The dominant, avoidable cost is the src/entities/player.h change for per-avatar body sockets (note 27e104c6f0ff). That note lists player.h but records no header-fanout count, while the header has 229 dependents. AGENTS.md gate 6 requires recording the affected-source count and, over 100 dependents, human/root review with a short fanout plan. Recompiling ~237 TUs for a rig/socket change was the reason this wait existed at all.

RECOMMENDATIONS (report-only; nothing changed or claimed here)
1. Make the wait budget scale-aware instead of a flat 60s wall clock for known large rebuilds (header fanout and full/cold rebuilds); key the decision on changed_header_fanout and/or steady progress. Tracked by issue-136d4553ff92; new occurrence recorded there.
2. Fix the wait snapshot to populate a readable stage/file/elapsed reason and the true in-flight attempt id/generation/run_id (issue-6e08d89b55e3 + stale-identity siblings issue-3878b275e5d9, issue-f36679f4ba95). This occurrence is additional evidence for both.
3. For body-socket/rig work, avoid editing player.h (229 dependents) or split the change so a rig edit does not force a 237-TU rebuild; record header_fanout in the build note and obtain the >100-dependent review per AGENTS.md.
4. Consolidate repeated game-dll-build waits under the existing trackers instead of one high-priority issue per wait_id.

STATUS: confirmed-build-wait; occurrence valid and reviewer wake unique; substantive causes (flat wall-clock budget vs 229-source header fanout + stale identity fields + raw reason slice) remain and are tracked. No duplicate issue created. No runtime/behavior proof claimed (build-friction review only).
