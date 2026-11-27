# issue-0a27ce892a00: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T23:11:02.049Z — build-wait-reviewer

related_build: game-dll-20261126T223808304994Z-2d57e6
related_run: 20261126_201711

## Review (confirmed-real-build-over-budget-one-wake-no-duplicate)

### 2026-11-26T23:11:45.893Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent build-wait review (read-only; no source edit, no build/test/runtime started by this agent).

VERDICT: real, non-stalled game-DLL build that legitimately exceeded the flat 60s wait budget; the automatic mechanism fired exactly once (one issue, one reviewer wake); no duplicate issue filed. Two evidence gaps noted below; resolution left to human/triage.

WAIT OCCURRENCE
- wait_id game-dll-build:2026-11-26T22:38:08.169Z; stage game_build; elapsed 60.336s (budget 60.0s); request 2026-11-26T22:38:07.862Z-c5f2ff1d:mode3:game; source_generation 7.
- logs/2026-11-26/20261126_201711/dev-loop/events.jsonl: exactly one dev-loop.wait.over_budget (created=true, 22:39:16.762Z) and exactly one dev-loop.wait.reviewer-wake (wake-bb643fb35c84, 22:39:11.145Z) for this wait. Dedup by wait_id held; no duplicate issue exists for this wait_id.

BUILD ACCOUNTING (corrects the wait snapshot identity)
- Authoritative attempt game-dll-20261126T223808304994Z-2d57e6, revision 4633798b848893e08ce846163164f8e9cec430985f00c36430fd5e0bbfc55ab3, generation 731, run 20261126_201711.
- dev-loop.game-build.started 22:38:19.296Z; result 22:42:42.209Z status ok generation 731: total 273.594s, compile 245.672s, link 8.859s, compiled 65 / skipped 580.
- .dev/build-timings/game-dll-20261126T223808304994Z-2d57e6.json: success true, total 277.797s, compile 245.703s, link 8.953s, object_cache_scan 4.609s, publish 0.594s; compile_sum 2,265,717ms over 65 TUs (max single TU 93.4s), jobs 10, source_count 644.
- Trigger: changed_paths include headers src/gamemode/map-config.h and src/mapeditor/map-editor.h; header fanout recompiled 65 TUs (map-editor.cpp 85.8s, map-config.cpp 89.1s, ...). The wait fired at ~60s while ~213s of legitimate compile remained, so the first divergence is budget-vs-natural-compile-cost, not a stalled build.

RESOURCE EVIDENCE (observed; explains the long compile)
- .dev/build-timings/game-dll-20261126T223808304994Z-2d57e6.resource.json: provider psutil_host_and_process, 186 samples @1.0s, duration 277.766s. mean_host_cpu_percent 98.909, peak 100.0 (whole-machine scale across 12 logical CPUs) => effectively all 12 logical CPUs saturated for the whole build.
- Host memory: total 16,802,918,400 B (~15.65 GiB); used 97.7% at capture start (available 380,485,632 B ~363 MiB); minimum available 5,275,648 B (~5.03 MiB) during the build => severe memory pressure/paging, consistent with the 40-148s per-TU queue waits recorded in journal file events.
- Inference (labeled): CPU saturation plus near-zero available memory is the plausible dominant cause of the 245s compile and the over-budget wait. A matched lower-pressure baseline was not captured (resource.json unavailable_fields includes matched_prebuild_resource_baseline), so this is not proven as the sole cause.
- Runtime during build (observed in sample): run 20261126_201711; runtime_context reported_mode vehicle_freeplay, map arena1, npcs 0; server/client journals under logs/2026-11-26/20261126_201711/{server-01,client-01}/events.jsonl.

EVIDENCE GAPS (not blocking acceptance; owned elsewhere)
1. The wait snapshot is not self-identifying: issue evidence build_id = 2026-11-26T20:12:20.599827Z (the previously published build; dev-loop.py:3896 uses self.latest_build) and generation = null (dev-loop.py:3897 self.game_build_generation). The true attempt/gen are recoverable only because this occurrence's truncated reason fragment happens to embed build_attempt_id / generation 731 / revision. Tracked by issue-6e08d89b55e3 (reason truncation).
2. Structural: ~200 open game-dll-build build-friction issues exist, most elapsed 60.0-62.8s, nearly all new/in_review. The flat 60s budget is systematically below typical legitimate rebuild latency (this build was 277s), so one-issue + one-wake per occurrence produces heavy, largely duplicate noise. Changing the budget is a shared-authority/spec decision (docs/specs/dev-loop/change-to-evidence-latency.md:122-127; human decision), so this review recommends a scaled/deduped budget rather than editing it. Linked owners: issue-136d4553ff92, issue-7d3dcfd7433b, issue-6e08d89b55e3.

ACCEPTANCE
- wait_id/stage/reason/elapsed/evidence present, and one reviewer wake queued exactly once for this occurrence => holds.
- build/run IDs were only partially self-contained in the record (gap 1); now pinned authoritatively via related_build/related_run on this issue.

Resolution: leave open for human/triage. No code, spec, or budget change was made by this review.
