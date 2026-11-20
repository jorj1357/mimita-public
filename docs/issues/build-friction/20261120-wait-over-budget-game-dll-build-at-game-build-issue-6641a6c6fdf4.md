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
