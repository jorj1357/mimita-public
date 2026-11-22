# issue-a7464f307b2f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise; root-cause=compile-error)

### 2026-11-22T08:10:10.727Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent read-only review by build-wait-reviewer (wake-77c534826390). No source edits.

WAIT OCCURRENCE: wait_id game-dll-build:2026-11-22T07:52:17.567Z, kind game-dll-build, stage game_build, elapsed 60.883s vs budget 60.0s. Pinned evidence: .dev/build-timings/game-dll-20261122T075217617830Z-a15284.jsonl (build_attempt_id game-dll-20261122T075217617830Z-a15284, source_generation 2, revision cfae2085...).

NOT A WEDGED/STALLED BUILD. The attempt was actively progressing when the 60s budget tripped: manifest_snapshot completed 07:52:24.991 (7.4s), source_snapshot 07:52:36.005 (18.4s), snapshot_prune 07:52:43.742 (26.1s), object_cache dependency prepass 07:52:53.367 (35.7s), then ~29 translation units compiled from 07:52:55 to 07:54:09. So elapsed time is genuine build latency, not a hang.

ACTUAL OUTCOME / ROOT CAUSE: the same attempt then FAILED at 07:54:09.824Z, exit_code 1, elapsed 112.196s, failed_phase compile, error: src/network/server-npcs.cpp:904 'class Npc' has no member named 'hasLoggedDash'. This is the second long failing build on the SAME revision cfae2085: game-dll-20261122T074440480025Z-9387dd failed with the identical error after 456.908s. The recurring over-budget wait was therefore a downstream symptom of a real compile error that forced full long rebuilds. That error is already resolved by the next revision 5677872 (hasLoggedDash absent from source); build game-dll-20261122T075410208033Z-f3ff3f completed exit_code 0, generation 476, 190.312s, published 07:57:20.

ACCEPTANCE CHECK: wait_id present, stage present, elapsed present, evidence present, reviewer wake queued exactly once (wake-77c534826390 queued 07:53:20.242Z in .dev/agent-wake/events.jsonl; later claimed/retried but not re-queued). Data-quality gaps: evidence.generation is null though the attempt ran generation 475 (build.failed labels 473); evidence.reason is a garbled worker-output fragment, not a clean reason; evidence.build_id points at the last successful build (2026-11-22T02:37:06Z) rather than the waited attempt, which appears only embedded in the malformed reason.

CONCLUSION: The wait detection/wake path worked as specified for this occurrence; no wait-policy code fix is required for this issue. The friction is the standard 60s budget being below normal game-DLL compile latency (112-457s here), compounded by a source compile error now fixed. Recommend not treating this issue as a lockdown blocker; track the compile-error regressions under their own issue if they recur.

## Context

### 2026-11-22T08:10:37.365Z — build-wait-reviewer

related_build: game-dll-20261122T075217617830Z-a15284
