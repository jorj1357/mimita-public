# issue-7e086ec27372: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-benign-wait; dev-loop-wait-event-missing)

### 2026-11-22T06:21:16.324Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review of the over-budget wait (wake-60194d0dea26).

WAIT
- wait_id game-dll-build:2026-11-22T05:44:19.012Z, stage game_build, elapsed 60.633s, budget 60s.
- Build attempt game-dll-20261122T054419086188Z-4f8139, request 2026-11-22T05:44:18.920Z-e846d72c.
- Changed source src/main-systems.cpp (note: "Release agent GUI mouse buttons even when the target widget disappears on press").

BUILD WAS NOT WEDGED
- .dev/build-timings/game-dll-20261122T054419086188Z-4f8139.jsonl:
  build.started 2026-11-22T05:44:19.086Z; build.completed 2026-11-22T05:47:54.559Z,
  elapsed_ms 215464, exit_code 0, generation 459, dll build\game\generation-459\game-g459.dll.
- build/current.json (same attempt): status ok, total_seconds 214.979, compile_seconds 124.155,
  slowest compile src/main-systems.cpp 122.168s, link_seconds 26.003, compiled 2, skipped 641.
- Phase breakdown: changed_source_preflight 122.627s, manifest_snapshot 35.24s, link 26.326s,
  source_snapshot 10.79s, object_cache_scan 5.378s, compile 1.543s, publish 0.623s.
  The over-budget wait is expected build latency, not a hang.

THRESHOLD IS MISCALIBRATED (systemic, not this occurrence)
- In .dev/build-timings, 100/100 retained game-DLL attempts succeeded and 94/100 exceeded 60s;
  median build time 279.3s. The 60s budget therefore fires on nearly every healthy build.
- There are 75 distinct "Wait over budget: game-dll-build at game_build" issues (75 distinct
  wait_ids; 56 in_review, 19 new). This is one-issue-per-occurrence noise, not independent defects.
- Route the latency root cause to issue-136d4553ff92 and the wait-policy/noise decision to
  issue-9a4503f6ed68. Do not open another duplicate issue for the recurring condition.

ACCEPTANCE CHECK
- wait_id, stage, reason, elapsed, evidence: present.
- build/run IDs: partially wrong. evidence.build_id 2026-11-22T02:37:06.972018Z is the latest EXE
  build, not this DLL attempt; evidence.generation is null although the attempt produced g459;
  no run id. The DLL attempt id is only recoverable from started_utc + build-timings.
- Reviewer wake queued exactly once: PASS. One request .dev/agent-wake/requests/wake-60194d0dea26.json
  and one wake.queued event at 2026-11-22T05:45:21.258Z for issue-7e086ec27372.

PROOF GAP (blocks the proof_expectation "Dev-loop wait event plus the issue record and reviewer wake record")
- The dev-loop wait event is missing. Root dev-loop events_path is
  C:\mimita-v9\logs\2026-11-22\20261122_011031\dev-loop\events.jsonl, and that directory does not exist.
- .dev/dev-loop-watch.log shows repeated failures, including every ~5s:
  "[DEV RUNTIME] unable to record launch evidence: [Errno 2] No such file or directory:
   'C:\mimita-v9\logs\2026-11-22\20261122_011031\dev-loop\events.jsonl'"
  and "unable to append journal event dev-loop.watcher.source-change-deferred: [Errno 2] ...".
- No dev-loop.wait.over_budget / reviewer-wake event exists in any dev-loop journal for this run.
- Filed separately as issue-6e2d7b91e7d9 (root dev-loop drops runtime journal events). That is the
  required code fix; it is not claimed by this bounded review.

CONCLUSION
- Confirmed benign/expected build latency; wake queued exactly once; the dev-loop wait-event proof is
  absent because the root dev-loop journal path is missing. No duplicate issue created for the
  recurring condition. Related build/run attached.

## Context

### 2026-11-22T06:21:16.957Z — build-wait-reviewer

related_build: game-dll-20261122T054419086188Z-4f8139
related_run: 20261122_011031
