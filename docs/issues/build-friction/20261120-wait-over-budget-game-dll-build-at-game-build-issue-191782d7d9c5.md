# issue-191782d7d9c5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-20T20:36:57.060Z — build-wait-reviewer

related_build: game-dll-20261120T195523321490Z-e1b74d
related_run: 20261120_170259

## Review (confirmed)

### 2026-11-20T20:38:10.363Z — build-wait-reviewer

Reviewer: build-wait-reviewer

VERDICT: confirmed over-budget wait; the build was healthy, not stuck or queued.

Wait occurrence: wait_id game-dll-build:2026-11-20T19:55:23.269Z, stage game_build, elapsed 60.418s at detection.
Evidence: .dev/build-timings/game-dll-20261120T195523321490Z-e1b74d.json/.jsonl - build.started 19:55:23.323Z, build.completed 20:09:36.810Z, success=true, total 853.471s, generation 411, request auto-34, source_generation 34. Run: 20261120_170259. Wake: wake-a5dd648492be queued once; claimed/launched with codex at 19:56:30 (process exited), fallback claimed/launched with opencode at 20:19:29 (this activation). No duplicate wake request for this wait_id.

Measured stage breakdown for the same attempt:
- codegen + manifest_snapshot + source_snapshot: ~47s
- changed_source_preflight: 150.3s, dominated by cold compile of src/actor/player-ai.cpp (149.99s)
- object_cache_scan: 356.1s (42% of the build; scan_parse_ms 326.5s over 1501 unique dependencies)
- compile: 107.6s (13 TUs compiled, 629 skipped, jobs=12)
- link: 62.8s
- publish: 120.2s

Interpretation: the 60s budget fires ~55s into the build during codegen/manifest snapshot, before the dominant object-scan cost. A normal warm rebuild with one changed player-ai.cpp already spends ~150s on that TU alone, so a 60s crossing is expected for this workload rather than evidence of a wedge. The wait policy is reporting real elapsed time accurately.

Field completion: reproduction, expected behavior, acceptance, paths, and proof expectation were already present and accurate. Added related_build and related_run. Preserved the existing structured evidence so wait_id dedupe still works.

Recurrence: the same wait_id has no later occurrence, so no occurrence was appended. Distinct wait_ids at 08:31:55Z (issue-63dccbd8f8b6) and 08:45:25Z (issue-2008e743c710) are separate issues by build_friction.record_wait_incident design (wait_id is the uniqueness key); they are not byte duplicates, but they do create repeated same-title high-priority triage entries.

Minor accuracy note: evidence.generation was captured as 410 at wait detection while the attempt published generation 411. Not a blocker; worth normalizing if generation is used for correlation.

No source edits and no lane files claimed. Measurement and consolidation follow-up proposed on issue-7d3dcfd7433b.

## Agent suggestion

### 2026-11-20T20:39:11.238Z — build-wait-reviewer

Review limitation and evidence gap: dev-loop.py --status advertises events=C:\mimita-v9\logs\2026-11-20\20261120_170259\dev-loop\events.jsonl for run 20261120_170259, but that directory/file does not exist. The run dir contains only client-01, server-01, and run.json. The wake prompt asks the reviewer to inspect the dev-loop journal, but the advertised path is missing; I reconstructed the wait and stage timing from .dev/build-timings/game-dll-20261120T195523321490Z-e1b74d.json/.jsonl and .dev/agent-wake/events.jsonl instead. Suggest either writing the dev-loop journal to the advertised run path or making --status report the actual journal path. Not claimed or fixed here.
