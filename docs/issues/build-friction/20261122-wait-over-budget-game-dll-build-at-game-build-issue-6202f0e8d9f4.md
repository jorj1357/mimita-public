# issue-6202f0e8d9f4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-friction)

### 2026-11-22T04:49:05.251Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

VERDICT: confirmed over-budget wait; not a stalled build. The 60s budget is below the normal critical path for this workload, and this issue repeats an already-tracked policy/observability defect.

VERIFIED THIS OCCURRENCE (journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl):
- dev-loop.wait.over_budget event DEV_RUNTIME_012fbdc9e2b5415cb7ea2a823ff94564 at 2026-11-22T04:24:23.083Z: wait_id game-dll-build:2026-11-22T04:23:20.820Z, kind game-dll-build, stage game_build, elapsed 61.082s, budget 60.0s, created=true.
- dev-loop.wait.reviewer-wake event DEV_RUNTIME_16e8e565a1df4b668a27e564e55ea951 for the same wait_id, queued=true, wake-9643fee0fd33. The .dev/agent-wake record has one wake for this issue; attempts=2 is adapter fallback/retry, not a second queued wake.
- Actual attempt game-dll-20261122T042320937721Z-1fc51a: success=true, no_op=false, generation 453, source_generation 3, started 2026-11-22T04:23:20.937Z, completed 04:24:50.972Z, total_seconds 90.022, worker_total_seconds 41.645 (.dev/build-timings/game-dll-20261122T042320937721Z-1fc51a.json).
- Changed path: src/npc/npc.cpp only. Phase cost: changed_source_preflight 25.193s, source_snapshot stage 25.183s, compile 26.042s (npc.cpp 25.099s), link 12.045s (gnu_linker 11.791s), object cache scan 2.401s. The wait fired at 61.082s, during the npc.cpp compile, and the build published about 27.9s later. There was forward progress throughout; no wedge.

FIRST DIVERGENCE - acceptance says the issue carries build/run IDs, but the recorded build identity is stale:
- Issue evidence has build_id 2026-11-22T02:37:06.972018Z and generation 452, which was the last completed build, not the waiting attempt. The waiting attempt is game-dll-20261122T042320937721Z-1fc51a / generation 453. The attempt ID appears only in the build-timings path, not in the wait issue evidence. This matches the earlier umbrella finding that build_id lags an in-progress wait and makes the reviewer trace indirect.

SECOND FINDING - noisy recurrence:
- This is one of at least five same-title waits between 04:10:48 and 04:29:08 (bc45be2f32a3, 266f4fcaf861, 6202f0e8d9f4, ce4c245a8a70, 628187de3d6d). Each is a distinct wait_id, but the same kind/stage/cause, and each creates a new issue plus reviewer wake. issue-266f4fcaf861 already recorded 24 over_budget events and 24 reviewer wakes in this run's journal. This contradicts the reviewer instruction to add occurrences rather than create duplicate class issues for a recurring wait.

THIRD FINDING - reason is only a build header:
- This occurrence's reason is '[GAME BUILD] revision=d31c7dc75c5f3c1160463effa2ecf114eb0b0d5bc4b60f3fccd68077d3cbdfb3 sources=642 jobs=10 ...'. It identifies the build but not the live phase; the phase at trip time (compile; npc.cpp in flight) is only recoverable from the timing JSONL.

RECOMMENDATION (reported, not applied; shared build path, needs owner/human decision):
1. Use a stage-aware or measured budget; for game-dll-build, 60s is below routine incremental cost (this attempt 90s, with preflight+snapshot alone ~50s).
2. Put the active build_attempt_id/source_generation in the wait evidence instead of relying on latest completed build_id.
3. Derive reason from the parsed current stage/state (and file) rather than the raw header.
4. Coalesce same kind+stage recurrence into an existing tracker or append occurrences; do not spawn another duplicate analysis.
Existing generic owners: issue-9a4503f6ed68 (automatic wait issues), issue-7d3dcfd7433b (wait-stage measurement), issue-136d4553ff92 (build lifecycle observability), issue-e044a931c54b (avoidable waits).

No source edited; no file claimed; no runtime behavior proof claimed. Confidence: high for wait timing, build identity, progress, and wake dedupe (direct journal + timing JSON). Medium for policy intent (budget choice requires human decision).
