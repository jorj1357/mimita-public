# issue-dea21a9c722d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-27T01:58:39.792Z — build-wait-reviewer

related_build: game-dll-20261127T013303208483Z-a405dd
related_run: 20261127_011757

## Review (confirmed-long-build-not-stall; flat-budget-policy-noise; wait-identity-stale; wake-queued-once-dispatch-late; journal-proof-missing(2of3))

### 2026-11-27T02:00:53.854Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake wake-c6fb7ac32356), read-only. No source edit, no build request, no runtime/behavior claim, no issue close.

VERDICT: confirmed genuine long build (not a stall); the 60s wait budget is simply below this build's natural wall time under a live game pair. Wait-record identity is stale (known defect). Reviewer wake was queued exactly once but dispatched late. The dev-loop journal half of the proof is not retained (2 of 3 proof components present).

WAIT UNDER REVIEW (pinned)
- wait_id: game-dll-build:2026-11-27T01:33:03.139Z
- kind/stage: game-dll-build / game_build
- elapsed at record: 62.229s vs budget 60.0s
- started_utc 2026-11-27T01:33:03.139Z; source_generation 5; request_ids [2026-11-27T01:33:02.926Z-418dceaf:mode3:game]; reason "[GAME BUILD] revision=11e6d92fa5c8eeede4e94c60e28336f8fc75fe8c070adb2f1382ed33b9d96c1c sources=645 jobs=10 priority=high cache=build\\game\\objects\\b10e3a31b3ae8637 seeded=0"
- evidence build_id: 2026-11-27T00:44:04.497616Z ; generation: null
- TRUE in-flight attempt (recovered from the reason revision and .dev/build-timings): game-dll-20261127T013303208483Z-a405dd, generation 749, revision 11e6d92fa5c8eeede4e94c60e28336f8fc75fe8c070adb2f1382ed33b9d96c1c, source_generation 5, request_ids match. This review set related_build=game-dll-20261127T013303208483Z-a405dd and related_run=20261127_011757 on the issue.

WHAT HAPPENED (direct artifacts)
- .dev/build-timings/game-dll-20261127T013303208483Z-a405dd.json: success=true; started 01:33:03.208Z, completed 01:37:56.090Z, total_seconds 292.891 (worker_total 261.797), generation 749 published to build\game\generation-749\game-g749.dll.
- Phases: changed_source_preflight 148140ms, compile 82860ms, link 24031ms, object_cache_scan 3329ms, publish 1063ms; jobs 10; 5 compiled / 641 skipped of 645 sources; compile_sum 281601ms.
- The 60s crossing (~01:34:05) occurred inside changed_source_preflight (the 148.14s map-editor compile), so the build was still healthy progress with ~230s of work left; it was NOT wedged.
- Slowest TUs: src/mapeditor/map-editor.cpp 148.043s, src/network/server-gamemode.cpp 79.926s, src/map/map-authored-geometry.cpp 40.207s, src/map/authored-block-rebuild.cpp 10.612s, module-identity-provider 2.813s. Only map-editor.cpp was directly changed; the other TUs recompiled because their dependency files were missing/invalid: cache_invalidations lists authored-block-rebuild.cpp, map-authored-geometry.cpp, server-gamemode.cpp all "missing or invalid dependency file" (object-cache/depfile freshness class already tracked; see issue-70b27f66e00e / issue-51b425637ea9 / issue-a6890271d414).

RESOURCE SAMPLE (direct, partial; causality NOT claimed)
- .dev/build-timings/game-dll-20261127T013303208483Z-a405dd.resource.json: status partial; 214 samples @1s over 292.563s; mean_host_cpu_percent 99.969, peak 100.0; minimum_available_memory_bytes 644,521,984 (~615 MiB); sampler_thread_status did_not_stop_before_timeout (final_sample_status skipped_sampler_thread_timeout).
- Same run 20261127_011757 hosted a live pair: server pid 5900 mode team_deathmatch (9 NPCs / 1 player; npc server-phase-window ai_ms 568.04 over 30 ticks, total 594.4ms) and client pid 34092 at ~21.69 fps (22/22 frames over the 4.17ms budget, max frame 68.57ms). Client mode was stale_or_unreported (client_mode_unverified).
- unavailable_fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation. Host was near-saturated during the compile, but absence of a matched prebuild baseline means pressure is OBSERVED, not proven as the cause (a missing sample is unknown, not zero).

WAIT RECORD / WAKE (pinned)
- Exactly one reviewer wake: wake-c6fb7ac32356 queued 2026-11-27T01:34:14.395Z (.dev/agent-wake/events.jsonl line 3718). .dev/agent-wake/requests/wake-c6fb7ac32356.json: attempts=2 — codex claimed 01:34:36.479Z / launched pid 4940 01:34:36.634Z then exited; opencode claimed 01:46:15.829Z / launched 01:46:16.123Z. Queue-to-working-launch ~12m02s, reproducing late-dispatch issue-84c584957e65. No duplicate wake and no duplicate issue for this wait_id.
- IDENTITY DEFECT (recurrence of issue-7d3dcfd7433b): the evidence build_id 2026-11-27T00:44:04.497616Z is the pinned EXE artifact of the active run (dev-loop.parallel-run.started build_id prefix 2026-11-27T00:44:04.497616Z:edbf9b4a...), and generation is null; neither names the in-flight game-DLL attempt a405dd/gen 749. Positive contrast: the reason string is clean this time, so the reason-truncation defect (issue-6e08d89b55e3) does NOT apply to this occurrence.

PROOF / JOURNAL GAP (2 of 3)
- No retained dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event exists for this wait_id in any logs/2026-11-27 dev-loop journal. The run 20261127_011757 dev-loop/events.jsonl contains only 9 startup lines ending 01:20:24Z; the daemon restarted at 01:49:35Z (.dev/state.json). The issue record and one queued wake do exist, so record+queue worked; only the journal half of the proof expectation ("Dev-loop wait event plus the issue record and reviewer wake record") is missing. This is the known root-dev-loop runtime-journal loss (issue-6e2d7b91e7d9 / duplicate issue-0498cbef4ef3), not a detection failure.

ACCEPTANCE CHECK
- wait_id present: pass. stage game_build: pass. elapsed 62.229: pass. reason readable: pass (clean this time).
- build/run IDs: PARTIAL — request_ids + source_generation present; recorded build_id stale and generation null (issue-7d3dcfd7433b). True attempt/run now recorded in related_build/related_run by this review.
- reviewer wake queued exactly once: pass (delivery late, issue-84c584957e65).
- Dev-loop wait event retained: FAIL for this occurrence (issue-6e2d7b91e7d9). Overall proof expectation 2 of 3.

RECURRENCE (not duplicates)
- Distinct wait_id, so one issue per occurrence is by design. 267 same-titled "Wait over budget: game-dll-build at game_build" issues exist all-time (247 in_review, 19 new, 1 assigned); this is another instance of the flat-60s-budget-vs-natural-wall-time pattern, not a new defect.

OWNERS / NEXT STEP / UNKNOWNS
- Policy/dedupe (per-class or attempt-aware budget, or consolidation): umbrella issue-7d3dcfd7433b (owner launch-latency-investigation), issue-136d4553ff92, issue-e044a931c54b.
- Wait-record identity (record in-flight attempt id/generation/run): issue-7d3dcfd7433b.
- Wake dispatch latency: issue-84c584957e65.
- Journal retention under the root daemon: issue-6e2d7b91e7d9.
- Resource baseline/causality: issue-2a5c5da53565 (gap_key build-resource-sampling:game-dll-20261127T013303208483Z-a405dd recorded once there).
- Unknowns: exact split of the 148s map-editor.cpp compile between TU cost and host saturation (no matched prebuild baseline); whether the three "missing or invalid dependency file" recompiles could have been avoided (object-cache freshness bug, not investigated here).
- No code fix claimed or edited by this review.
