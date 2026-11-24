# issue-6641a6c6fdf4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-20T21:22:15.621Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed exact wait occurrence in a bounded wake activation (wake-460522b68c2f). Read-only; no source edits, no file claims, no build started, no fix claimed.

Wait identity
- wait_id game-dll-build:2026-11-20T20:32:51.415Z; kind game-dll-build; outer stage game_build; elapsed 60.949s vs 60.0s budget; request auto-39; source_generation 39; structured evidence build_id 2026-11-20T20:20:51.788061Z, generation 412.
- Exactly one issue record exists for this wait_id (brain.db issues.evidence query).
- Exactly one wake was queued for it: wake-460522b68c2f at 2026-11-20T20:33:53.956Z. Request shows attempts=2, launched 2026-11-20T20:59:54.485Z (about 26 min after creation).
- Issue opened 2026-11-20T20:33:52.402Z (coord seq 2301). Acceptance (one issue plus one reviewer wake per occurrence) is met.

What the wait was
- In-flight attempt: game-dll-20261120T203251717649Z-0ac652, generation 413, source_generation 39, changed path src/physics/movement/physics-collision-glb-sweep-slide.cpp, request auto-39.
- Started 2026-11-20T20:32:51.721Z; build.completed 20:43:05.855Z success exit 0; total_seconds 614.121. It was progressing, not hung.
- Timeline (.dev/build-timings/game-dll-20261120T203251717649Z-0ac652.jsonl): resident_state codegen 1.47s; packet_registry codegen 49.13s (20:32:53.366-20:33:42.495); manifest_snapshot started 20:33:42.531; the 60.949s wait fired 20:33:52.364 during manifest_snapshot. The reason string captured the latest stage event (manifest_snapshot started, elapsed_ms 50810.647), not a stall.
- Dominant measured cost: object_scan 313.458s (scan_parse 300568.279ms, unique_dependencies 1501); changed_source_preflight 68.067s; compile 119.654s over 12 units (slowest physics-collision-glb-sweep-slide 67.844s); link 54.68s; publish 28.102s. The 60s budget is below this host measured non-trivial game-DLL baseline; this healthy full rebuild was about 10x the budget.

Attribution defect (previously reported on issue-f66e6e82656b; reproduced here)
- Structured evidence names build_id 2026-11-20T20:20:51.788061Z and generation 412, the previously published artifact, not the in-flight attempt 0ac652 / generation 413. The correct attempt id appears only inside the free-text reason.
- Consequence: only wait_id is trustworthy for correlation; build_id/generation in over-budget records can point at the wrong build. No duplicate fix filed here; the owner issue for this class is unchanged.

Issue completeness
- brain.db readiness: ready=true, missing=[], score 8/8 (reproduction, expected behavior, acceptance, paths, risk, proof expectation, and evidence all present). The markdown document renders only the human quote; this review carries the narrative evidence. No clarification needed.

Recommendation (no fix claimed)
- Calibrate per-stage game_build budgets from measured data (object scan alone is about 5 min) or check compile/link progress instead of raw wall time; emit sub-stage attribution (codegen/manifest/source_snapshot/scan/compile/link/publish) in the evidence/reason so a reviewer sees the dominant phase immediately.
- Distinguish an actively progressing long build from a no-progress stall.
- Consolidate the same-signal family (issue-6641a6c6fdf4, issue-0322fa45b0e8, issue-76f19e7598c4, issue-f66e6e82656b, issue-191782d7d9c5, issue-84ec2bf27598, issue-9a9807fa0128, issue-bb273ac1386c, issue-b0ba8698acd7; identical quote hash 1331248..., distinct wait_ids) under the wait-policy owner issue-e044a931c54b / issue-9a4503f6ed68 rather than leaving many separate open records.

Wake-delivery note
- The full prompt was written to .dev/agent-wake/logs/2026-11-20T20-33-53.949Z-build-wait-reviewer.prompt.txt and correctly names wake-460522b68c2f / issue-6641a6c6fdf4, but the opencode session received only the first line ("You were woken by MiMITA agent-wake. This is a bounded test activation."). The assignment was recovered from MIMITA_WAKE_ID / MIMITA_WAKE_ISSUE and the request file. Known defect issue-62ab3daa615d; it does not invalidate this wait occurrence.

Verdict: valid, complete, recurring build-friction caused by a miscalibrated wall-clock budget, not a stalled dev-loop. No duplicate created. Evidence review only; no behavior proof claimed.

## Scheduled independent review

### 2026-11-24T04:35:15.833Z — vehicle-rl-planning

A bounded independent review was queued as wake wake-4bd6530907ad after 506 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed)

### 2026-11-24T05:01:56.313Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (wake-4bd6530907ad, agent issue-review-sweeper). Read-only: no source edits, no file claims, no build/test started, no fix claimed; issue not closed or reassigned. Prior review text preserved (issue_reviews id 24 from build-wait-reviewer remains the narrative evidence).

Scope: verified the single wait occurrence for wait_id game-dll-build:2026-11-20T20:32:51.415Z directly against .dev/build-timings/game-dll-20261120T203251717649Z-0ac652.jsonl (748 lines), the issue record, and the adjacent build summaries.

Observed facts (verified against the raw journal)
- build.started 2026-11-20T20:32:51.721Z; attempt game-dll-20261120T203251717649Z-0ac652; source_generation 39; generation 413; request auto-39; build note acd7721a2ecd changed src/physics/movement/physics-collision-glb-sweep-slide.cpp.
- Segment times: codegen resident_state 1.472s; packet_registry_codegen 49.130s; manifest_snapshot 19.676s; source_snapshot 16.099s (808 inputs); changed_source_preflight 68.081s (physics-collision-glb-sweep-slide.cpp compile 67.844s); object_cache_scan 313.494s; vendor_compile 0.023s; compile 51.604s over 11 TUs (slowest npc-behavior-module.cpp 51.476s); link 54.700s; coherence_validation 0.978s; publish 30.112s.
- The wait fired at 20:33:52.364Z (elapsed 60.949s) during stage manifest_snapshot (started 20:33:42.531Z, elapsed_ms 50810.647, completed 20:34:02.238Z) — a normal progressing stage, not a stall.
- build.completed 20:43:05.855Z, exit_code 0, elapsed_ms 614121.0 (~614.1s), generation 413 published (game-g413.dll).
- object_cache_scan scan.snapshot: scan_seconds 313.458, parse_ms 300568.279, stat_ms 196.047, unique_dependencies 1501; scanned 641, skipped 630, stale 11. This one phase is 51.0% of total build wall time; scan parse (300.6s) dominates it.

Inference / uncertainty
- The 60s flat budget is far below this host's full-rebuild cost in this window; the build was healthy and actively progressing. This occurrence is a budget-calibration false-positive, not a stalled dev-loop. Confidence: high (direct timestamps).
- When the wait fired (~60.9s in), roughly 553s of work remained and the largest phase (object_cache_scan) had not even started; so the wait could not have distinguished progress from stall here. Confidence: high (timestamps).
- Whether the ~300s parse is a cold-cache full parse that can be cached/parallelized, versus intrinsically required, is UNKNOWN from this journal: parse_ms is reported as a single aggregate with no cold/warm split.

Contrary evidence / nuance
- object_scan_seconds 313.458 was normal for this window (adjacent generations 410-416: 215.0, 356.1, 259.9, 313.5, 318.3, 340.3, 382.9s), so this attempt is not an outlier; the defect is structural budget calibration, not anomalous build behavior.
- Recent summaries (2026-11-21 onward) often finish well under 60s (object_scan ~1-90s, totals frequently <150s), so a flat 60s budget is not universally too low; it misfires mainly on true full rebuilds and heavy single-TU compiles. The 2026-11-20 occurrence sits in a historical high-cost window.
- This individual record's acceptance IS met: exactly one issue + one reviewer wake per wait (prior review id 24). So this is a policy/calibration signal; it is not a functional dedup break in this specific record, and it is not evidence the build was broken.

Next discriminating check (concrete)
- Select the most recent non-no_op full game-DLL rebuild summary and compare its object_cache_scan parse_ms, unique_dependencies, and compiled-vs-skipped against this 2026-11-20 baseline. If a current full rebuild still shows ~300s parse for ~1500 deps, add a cold/warm parse split and measure parse_ms before/after any scan-cache change — that is the highest-value lever for full-rebuild wall time. If current full rebuilds are already far below 300s, the discriminating check becomes whether the flat 60s budget still fires on any current full rebuild (stage/workload-aware, stall-based budget), which is tracked on issue-9a4503f6ed68.

Process note
- brain.db count at 2026-11-24: 148 issues titled 'Wait over budget: game-dll-build at game_build' (all 148 open states). Escalation 29 (11-21) -> ~50 (11-22) -> 148 (11-24) is a documented issue flood. The signature-dedup + stall-based/per-stage-budget mitigation is already proposed and owned on issue-9a4503f6ed68 (build-wait-reviewer) and issue-e044a931c54b (build-runtime-subagents). This sweep adds no new direction (to avoid duplication) but records the current 148 baseline as a measurable target on issue-9a4503f6ed68.

Verdict: reviewed — evidence review only; no behavior proof claimed, no fix claimed, no duplicate issue created.
