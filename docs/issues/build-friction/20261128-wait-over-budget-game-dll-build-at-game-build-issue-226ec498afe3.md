# issue-226ec498afe3: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-fanout-build-scope-under-declared)

### 2026-11-28T17:16:03.421Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed this wait occurrence against the pinned game-DLL attempt, the dev-loop journal, the build-timings postmortem, and the reviewer-wake record. Conclusion: **confirmed real over-budget wait caused by a genuine heavy rebuild, not a hung build.** The wait and wake mechanics behaved correctly. This occurrence is materially worse than the recent marginal (~0.4s over) cases: the build actually ran 490.8s, not 61s.

WAIT OCCURRENCE (verified)
- wait_id `game-dll-build:2026-11-28T09:02:17.737Z`; started 09:02:17.737Z; elapsed 61.251s; budget 60.0s; stage `game_build`.
- Journal of record: `logs/2026-11-28/20261128_082433/dev-loop/events.jsonl`, event `dev-loop.wait.over_budget` (DEV_RUNTIME_f3b712feca3b4622bf3e6152df46bed5) and `dev-loop.wait.reviewer-wake` (queued:true, adapter codex) for `issue-226ec498afe3`.
- Acceptance "one reviewer wake per occurrence": met. Exactly one wake request targets this issue (`wake-51ed0c726de1`). The adapter was codex although the reviewer ran under opencode; that queue-to-review latency is the separate `issue-84c584957e65` family, not this occurrence's defect.
- Known attribution gaps (not new, already tracked): the issue. evidence `build_id` `2026-11-27T17:45:19.700118Z` is the active EXE artifact id, not this DLL attempt; evidence `generation` 817 while the attempt produced 819; `reason` is a truncated mid-JSON tail (unreadable) -> `issue-6e08d89b55e3`; mis-attribution family -> `issue-2235461bb31b`/REG.

PINNED BUILD ATTEMPT
- attempt `game-dll-20261128T090217845310Z-a0bac6`; request `build-request-e11cc2c76c15`; source_generation 56; revision `4e0a2ba59d1d...`; success, exit 0; generation 819.
- Timeline (jsonl): build.started 09:02:17.988Z; compile phase 09:02:41.255Z -> 09:10:20.659Z (459.406s); build.completed 09:10:28.585Z. total 490.843s; worker_total 481.547s.
- Work: 163 compiled / 484 skipped. Compile 471.469s dominates; link 6.031s; changed_source_preflight 12.078s; object_scan 2.485s.
- Slowest TUs: `src/network/server-gamemode.cpp` 91.098s, `server-npcs.cpp` 53.51s, `map-editor.cpp` 51.781s, `server-packets.cpp` 47.147s, `multiplayer-tick.cpp` 45.649s.
- Worker starvation: jobs=10; queue_wait_max 424.422s, queue_wait_mean 202.352s; per-TU queue waits ~260-424s (e.g. video-commands.cpp 424.422s, vehicle-system.cpp 424.406s).
- Resource (postmortem + resource.json): mean host CPU 98.37%, peak 100%, minimum available memory 90.4 MB (resource.json 94.8 MB). Postmortem `unavailable_fields` include matched prebuild baseline, host commit charge/limit, and journal-aligned live mode/workload.

ROOT CAUSE OF THE 163-TU COMPILE (why a one-include build took 490s)
- This build's own `change_report.changed` is only `src/vehicles/vehicle-hud.cpp`; `changed_headers` is empty.
- But the working tree still carries uncommitted edits to two high-fanout headers: `git status` M for `src/network/multiplayer-context.h` and `src/network/packets.h` (diff +7 and +7/-2). Their mtimes (03:51:10 / 03:50:35 local = ~08:51Z) sit between the 08:41 build 817 (3 TUs, 79s) and the failed 08:57 attempt.
- `python devscripts/dependency_graph.py --affected src/network/multiplayer-context.h` = **128 affected sources**. 30 of this attempt's 32 `cache_invalidations` name `dependency newer than object: src/network/multiplayer-context.h` (2 name `packets.h`; 2 are `missing or invalid dependency file` for vehicle-commands.cpp/vehicle-system.cpp). That fanout accounts for the 163 recompiled TUs.
- Sequence: 08:41 build 817 ok (3 TUs) -> 08:51Z header edits -> 08:57 attempt `game-dll-20261128T085721640685Z-c68ca8` failed compiling vehicle-hud.cpp (the "Generation 818 failed" note) -> 09:02:03 vehicle-hud.cpp re-edited with the missing `terminal/terminal-state.h` include -> 09:02:17 attempt 819 rebuilt the 163-TU header fanout.
- The only queued build note for attempt 819 (`note_id 63add70496d5`) declared `files: [src/vehicles/vehicle-hud.cpp]`, risk "Include only", `expected_seconds: 60.0`. That is materially under-scoped: the attempt also compiled two changed high-fanout headers. Gate 6's "each agent's change note must stay attached to the attempt" was not satisfied for the header work.

ASSESSMENT
- Observed (fact): 490.8s build, 471.5s compile, 424.4s max per-TU queue wait, 98-100% host CPU, ~90 MB free RAM, 163 TUs driven by `multiplayer-context.h`/`packets.h`. Direct observation, from build-timings json/jsonl and git status/diff.
- Inference (not yet proven): host memory/CPU pressure plus 10-job worker starvation lengthened an already-heavy fanout compile. Matched prebuild baseline/commit charge are unavailable, so this review does not claim pressure was the sole cause.
- The wait policy is behaving as designed; the recorded `elapsed_seconds` 61.251 is the first over-budget crossing, not build duration, so it understates a 490s build. That is a reporting nuance, not the defect.
- No source edited, no build requested, no fix claimed here.

OWNERSHIP / NEXT SMALLEST CHECK
- Slow build + stage accounting: `issue-136d4553ff92` (owner build-coordinator, in_review).
- Header fan-out (`multiplayer-context.h` affects 128 sources): `issue-196c3045b4cb` (owner npc-header-latency).
- Cross-agent overlap / build-note scope race (undeclared uncommitted header edits inflating one attempt): `issue-c4f2df8edeba`.
- Resource pressure: `issue-2a5c5da53565`, `issue-ca5eb16d9e21`, `issue-a13a383a3f3d`.
- Unreadable wait reason: `issue-6e08d89b55e3`. Late wake dispatch/adapter: `issue-84c584957e65`.
- Smallest useful next check (owner of the header edits): confirm whether the `multiplayer-context.h`/`packets.h` edits were intended to ride this attempt; if yes, attach their build notes and set an honest `expected_seconds` (minutes, not 60s) and record `dependency_graph --affected` fanout; if unintended, treat as cross-agent interference under `issue-c4f2df8edeba`.
- Duplicate-issue note: this is one of dozens of `game-dll-build at game_build` wait issues; this review appended the occurrence here instead of creating another.
- Confidence: high for the timings, fanout files, and note-scope mismatch (pinned jsonl/json, git status/diff, dependency_graph).
