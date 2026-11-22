# issue-628187de3d6d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-friction)

### 2026-11-22T05:19:06.902Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

VERDICT: confirmed over-budget wait; not a stalled build. The 60s budget is below this workload's normal incremental critical path, and the recorded reason for this occurrence is corrupt. Both are already-tracked generic defects; this is a per-occurrence confirmation, not a new class of issue.

VERIFIED THIS OCCURRENCE (journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl):
- dev-loop.wait.over_budget event DEV_RUNTIME_e435350ee35e4135b84c461884c4a2d9 at 2026-11-22T04:30:11.273Z: wait_id game-dll-build:2026-11-22T04:29:08.935Z, kind game-dll-build, stage game_build, elapsed 60.959s, budget 60.0s, created=true, generation 454, source_generation 6.
- dev-loop.wait.reviewer-wake event DEV_RUNTIME_a0b0eb5b4819421e8326455132045b01 for the same wait_id, queued=true, wake-ba237b4585d3. Exactly one wake request exists for this issue (.dev/agent-wake/requests/wake-ba237b4585d3.json; attempts=2 is launcher retry, not a second queued wake). Wake dedupe acceptance is met.
- Actual attempt game-dll-20261122T042908986103Z-513026: success=true, no_op=false, generation 455, source_generation 6, started 2026-11-22T04:29:08.986Z, completed 2026-11-22T04:30:17.201Z, total_seconds 68.205, worker_total_seconds 50.132 (.dev/build-timings/game-dll-20261122T042908986103Z-513026.json; result event DEV_RUNTIME_199d0b2696ca4679b1622343552a0b40).
- Changed path: src/main-systems.cpp only. Phase cost: changed_source_preflight 28.176s, compile 29.217s (main-systems.cpp 27.976s), link 14.824s (gnu_linker 14.824s), object cache scan 4.079s, publish 0.691s. The wait fired at 60.959s while the build was still in compile/meta and published ~6.3s later. Compile+preflight+link alone consume ~72s before overhead; forward progress was continuous, so this is a throughput/budget signal, not a wedge.

FIRST DIVERGENCE - reason in the issue evidence is corrupt (specific to this occurrence):
- Recorded reason is a mid-JSON fragment beginning 51589.979 followed by comma and quoted build_attempt_id fields, not a readable stage/file.
- Cause: _record_game_build_output stores the last 240 chars of the raw worker line (devscripts/dev-loop.py:2830, detail = line.rstrip()[-240:]; assigned to game_build_detail at :2860, copied into the wait reason at :3528). The [GAME_FILE] JSON on journal line 751 is longer than 240 chars, so the head ([GAME_FILE] plus stage) is cut and the tail begins at elapsed_ms 51589.979. The same truncation produced a leading :57590.996 fragment on issue-ce4c245a8a70 and a :58330.375 fragment on issue-ee4dac126d29.
- Impact: the acceptance criterion that the issue carries a reason is satisfied only nominally; a reviewer must re-read the journal to learn the phase/file. The parsed stage in the same event (codegen / manifest / source_snapshot etc.) is available and would be a correct reason source.

SECOND FINDING - stale build identity in wait evidence:
- Issue evidence build_id 2026-11-22T02:37:06.972018Z and generation 454 are the last completed/previous generation, while the waiting attempt is game-dll-20261122T042908986103Z-513026 / generation 455. The attempt ID is absent from the issue evidence; the reviewer must join via build-timings. Matches the umbrella finding already reported on issue-6202f0e8d9f4.

THIRD FINDING - noisy recurrence, already tracked:
- Same kind/stage recurrence between 04:10:48 and 04:36:01: bc45be2f32a3, 266f4fcaf861, 6202f0e8d9f4, ce4c245a8a70, this 628187de3d6d, d2466e463a18. Distinct wait_ids produce a new issue + reviewer wake each time although the class cause is identical. This is the duplicate-class concern already recorded; not adding a new issue.

RECOMMENDATION (reported, not applied; shared build path, needs owner/human decision):
1. Have _record_game_build_output store a parsed compact reason (e.g. stage+state, or [FILE] path compiled) instead of the raw last-240 tail, so JSON lines cannot be cut mid-field.
2. Include active build_attempt_id and source_generation in the wait evidence rather than the last completed build_id/generation.
3. Use a stage-aware or measured budget for game-dll-build; 60s is below routine incremental cost (preflight ~28s + compile ~28-30s + link ~15s here).
4. Coalesce same kind+stage recurrence by appending occurrences to one tracker instead of spawning new issues.
Existing generic owners: issue-9a4503f6ed68 (automatic wait issues), issue-7d3dcfd7433b (wait-stage measurement), issue-136d4553ff92 (build lifecycle observability), issue-e044a931c54b (avoidable waits).

No source edited; no file claimed; no runtime behavior proof claimed. Confidence: high for wait timing, build identity, wake dedupe, and the reason-truncation cause (direct journal + timing JSON + code path); medium for policy intent (budget choice requires human decision).
