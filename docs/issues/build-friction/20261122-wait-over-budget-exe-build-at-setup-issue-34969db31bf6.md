# issue-34969db31bf6: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T07:41:12.782Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer, wake-21757ebfb6a7, 2026-11-22. Read-only: no source edit, no build/test requested, no fix or closure claimed.

CONCLUSION
Acceptance for the wait record is met, but this is not a new fault: it is the 6th open occurrence of the same exe-build at setup / preparing build 60s-budget overshoot and the same root cause already tracked in issue-8aae0621645f and issue-ae248907f1f1. Not a stuck build.

VERIFIED OCCURRENCE
- wait_id exe-build:10:2026-11-22T07:08:26.104Z; kind exe-build; stage setup; reason preparing build; elapsed 60.675s vs 60.0s budget; generation 10; source_generation 10; started_utc 07:08:26.104Z; build_id 2026-11-22T02:37:06.972018Z (previous published artifact, not the in-flight attempt - attribution defect persists); request_ids [].
- Issue record issue-34969db31bf6 carries title, quote, reproduction, expected behavior, acceptance, paths, risk, proof_expectation, and the full wait evidence JSON; readiness ready=true, missing=[].

PINNED ATTEMPT EVIDENCE (no-op; 77.76s failed presentation-DLL inside setup)
- build/build-metrics.jsonl 2026-11-22T07:09:58Z: result noop, compiled 0, skipped 87, elapsed_s 80.833, phase_seconds.presentation_dll_s 77.76, phase_details.presentation_dll = {outcome: failed, exit_code 1, error: build_game_dll.py exited with code 1}. The 60.675s wait fired while this child was still running.
- Next attempt 07:10:56 shows phase_details.presentation_dll = {outcome: cached_failure_skip, previous_error: build_game_dll.py exited with code 1, retry_after_s 1742.404}, i.e. a ~29 min failure cache now suppresses the repeat; the 07:08 attempt predates that skip.
- This matches issue-8aae0621645f (same signature, 60.019s) and issue-ae248907f1f1 (failing optional presentation-DLL build hidden inside EXE setup).

WAKE DELIVERY
- .dev/agent-wake/events.jsonl: wake-21757ebfb6a7 queued once at 07:09:27.464Z; first claimed/launched 07:09:34.018Z/077Z (codex, exited); retry 07:18:37.730Z; re-claimed/launched 07:18:55.745Z/784Z (opencode, this session). Request file attempts=2. One wake request per occurrence, so queued-exactly-once passes; ~9m28s delivery latency from adapter fallback.

JOURNAL EVIDENCE GAP
- The declared root dev-loop journal C:\mimita-v9\logs\2026-11-22\20261122_011031\dev-loop\events.jsonl does not exist (run dir has only diagnostics/, server-01/, run.json, host-resource CSVs). The exact dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake events for this wait_id are therefore not on disk; the wait is evidenced only by the issue record and the wake journal. This matches the open issue-0498cbef4ef3 (Root dev-loop drops runtime journal events: wait and watcher evidence never written).

POLICY NOISE / DUPLICATION
- Open build-friction issues: 110; 83 Wait over budget: game-dll-build at game_build; 6 Wait over budget: exe-build at setup (issue-8aae0621645f 60.019s, issue-17abc587c89a 61.375s, issue-723326f11472 60.253s, issue-7d013b4a6c77 60.363s, issue-dff7e8c0f758 60.833s, this 60.675s). All within ~1.4s of the flat 60s budget at the same stage/reason. build_friction.py:131-169 keys uniqueness on wait_id, so every occurrence mints a new issue and a new wake; the issue risk note names this. Recorded here as a recurrence, not a duplicate analysis; occurrence appended to issue-8aae0621645f.

RECOMMENDATION (not claimed)
- Do not tune the 60s budget here; a per-class budget needs human approval. Follow existing owners: issue-ae248907f1f1 (stop the failing optional presentation-DLL from consuming EXE setup and surface the child failure reason), issue-e044a931c54b / issue-7d3dcfd7433b (remove/measure avoidable waits and per-stage budgets), issue-136d4553ff92 (stage accounting), issue-0498cbef4ef3 (restore dev-loop journal events).
- Keep this issue in_review until the root-cause fix lands and a journal proves setup sub-stage timing.

Confidence: high on the pinned no-op attempt and the failed presentation-DLL phase (build-metrics direct), the acceptance check, and the single wake; high that this is a recurrence (identical signature and root cause). No source, build, or runtime behavior changed by this review.
