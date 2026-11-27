# issue-4fb7a24c54b0: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-needs-followup)

### 2026-11-26T17:25:38.579Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: reviewed-needs-followup. Read-only review; no source edit, no build requested, no fix claimed, no runtime proof claimed.

WAIT CONFIRMED: journal logs/2026-11-26/20261126_165201/dev-loop/events.jsonl line 128 `dev-loop.wait.over_budget` records wait_id game-dll-build:2026-11-26T16:56:24.029Z, kind game-dll-build, stage game_build, elapsed 60.924s vs budget 60.0s, issue issue-4fb7a24c54b0, created=true. Line 127 `dev-loop.wait.reviewer-wake` queued=true (wake-6f86e0fbb409, adapter codex). Exactly one over_budget event and one wake for this wait_id across the 2026-11-26 journals. Acceptance (wait_id, stage, elapsed, build/run IDs present; one reviewer wake) is met.

ROOT CAUSE - REAL LONG BUILD, NOT A HANG: attempt game-dll-20261126T165624137995Z-32c560 (gen 698, source_generation 20, revision 471a974f...) started 16:56:24.137Z and SUCCEEDED at 16:58:55.298Z. total_seconds 151.156, worker_total 145.766; phase compile 134.375s (worker compile_seconds 140.329), 64 TUs compiled, compile_max 57.319s; link 2.422s; object_scan 1.672s. The wait crossed 60s inside compile and the build finished ~90s later. Source .dev/build-timings/game-dll-20261126T165624137995Z-32c560.json.

RESOURCE CONTEXT: host logical CPUs 12; 119 samples @1.0s over 151.11s; mean host CPU 91.693% of whole-machine (~11.0/12 logical CPUs), peak 100%; minimum available memory 51,388,416 B (~49.0 MiB); client PID 11544 ~440 FPS arena1, 0 NPC in view (.dev/build-timings/game-dll-20261126T165624137995Z-32c560.resource.json). Inferred: the host was CPU-saturated during the compile. Unknown: per-process attribution and client share.

RECURRENCE (not a duplicate issue): three distinct same-titled waits in ~19 min, all successful: issue-404bd5b44b6e (16:49:17.979Z, gen 696 attempt ...164918058647Z-26c738, 141.047s), this issue (16:56:24.029Z, gen 698 attempt ...165624137995Z-32c560, 151.156s), issue-45733b82bbbc (17:07:53.638Z, gen 702 attempt ...170753867026Z-7428d5, 103.313s). A flat 60s budget is below normal full game-DLL compile time (100-150s). Attribution: .dev/build-timings/<attempt>.json and the sibling issue records. This is the policy noise already named in this issue's risk field; consolidation/cooldown or a per-kind budget is a policy decision (owner issue-e044a931c54b; auto-issue pipeline issue-9a4503f6ed68).

PRE-EXISTING RECORD DEFECTS (cross-linked, not re-filed): (1) reason is a truncated last-line tail (dev-loop.py:3112 [-240:], stored at 3892) -> issue-6e08d89b55e3; (2) build_id is the previous latest_build with generation=null and stage hardcoded 'game_build' (3891), not the in-flight attempt; sibling 404 carries the identical build_id -> issue-7d3dcfd7433b; compile-stage accounting -> issue-136d4553ff92. Wake delivery: queued 16:57:27.591Z, codex launched 16:57:38.674Z then exited, fallback opencode effective launch 17:10:10.197Z (~12m43s) -> recurrence of issue-84c584957e65. Host pressure -> issue-2a5c5da53565.

NEXT STEP: wait-policy owner decides per-kind budget and/or consolidation; wait-issue writer owner fixes reason/identity fields. No code fix implemented or claimed here. Sibling waits are distinct wait_ids, so no duplicate was created.

## Context

### 2026-11-26T17:26:16.742Z — build-wait-reviewer

related_build: game-dll-20261126T165624137995Z-32c560
related_run: 20261126_165201
