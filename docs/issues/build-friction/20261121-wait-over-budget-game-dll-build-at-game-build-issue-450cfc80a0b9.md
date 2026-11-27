# issue-450cfc80a0b9: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-needs-policy-fix)

### 2026-11-21T15:41:09.947Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence: wait_id game-dll-build:2026-11-21T03:46:54.696Z (build_attempt game-dll-20261121T034654937752Z-3c82d2, generation 417).

Evidence
- Issue evidence record: budget_seconds=60.0, elapsed_seconds=61.776, stage=game_build, source_generation=5, request_ids=[auto-5].
- .dev/build-timings/game-dll-20261121T034654937752Z-3c82d2.json: started 03:46:54.938Z, completed 04:25:24.660Z, total_seconds=2309.492, success=false, failed_phase=changed_source_preflight, worker_error="changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 1488.004s". Phase timings: object_scan 337.838s, changed_source_preflight 342.853s, compile 414.893s, link 100.34s, publish 69.91s.
- devscripts/dev-loop.py:103-106 sets one global WAIT_ISSUE_SECONDS=60 for every stage. devscripts/dev-loop.py:3433-3449 flags game-dll-build once total elapsed in stage game_build >= that budget; no progress/stall test.
- At issuance (03:47:56Z, 61.776s) the build reason was [GAME_STAGE] codegen:packet_registry_codegen.py started (elapsed_ms 6535), i.e. the build was actively progressing.

Finding (first divergence)
The wait policy, not the build, is the fault. A 60s total-stage budget is applied to game_build, a stage that legitimately runs 10-40 minutes (this attempt: 2309.492s). Every real game-DLL build therefore trips it, producing 36+ open "Wait over budget: game-dll-build at game_build" issues (issue-450cfc80a0b9, issue-ad873eff97f3, issue-6415397775a2, issue-f9ea4f30ad18, issue-dad513d47108, issue-121f867b430a, issue-9215d710021d, issue-e96486c8cc3e, issue-f995213a15b0, plus older in_review duplicates). This occurrence is a false positive: at 61.776s the build was 6.5s into packet registry codegen, not stalled. The issue also duplicates the intent of parent issue-e044a931c54b (assigned build-runtime-subagents) and measuring issue-7d3dcfd7433b (triaged); creating another identical wait issue is noise.

Real friction in the same attempt (separate owner, already re-resolved)
- The attempt failed compile at src/network/multiplayer-projectiles.cpp:2419 (std::nullptr_t to float at spawnExplosionFx arg 7), then blocked the queue for ~38 min. That source is now fixed at line 2419 (1.0f arg) and generation 420 builds successfully (current.json total_seconds=600.49, status=ok). No open action for this location.
- Remaining, separate, still-relevant slow phases for the wait parent: object_scan 337.8s, changed_source_preflight 342.9s (worker reports 1488s), compile 414.9s, link 100.3s.

Missing fields
None. issue readiness is ready=true, missing=[], score=8/8; reproduction, expected behavior, acceptance, paths, proof expectation, and evidence are all present.

Recommendation for the owner (report only; reviewer made no code edit)
1. Replace the single 60s total-stage budget in dev-loop.py:check_wait_budget() with stage-aware budgets AND progress detection (flag only when the stage detail/timeline has not advanced for N seconds), so an actively progressing long build does not page a reviewer.
2. Deduplicate by (kind, stage, active build attempt/generation) instead of one issue per wait start timestamp, and append occurrences under the existing parent rather than spawning new issues.
3. Route the phase-timing findings to issue-e044a931c54b / issue-7d3dcfd7433b; do not leave 36 parallel identical wait issues open.

Verdict: reviewed / root cause is wait-policy false positive; requires a dev-loop wait-budget fix owned separately (not this reviewer). No source or leased file edited.

## Scheduled independent review

### 2026-11-26T17:42:01.534Z — build-wait-reviewer

A bounded independent review was queued as wake wake-428daa5797aa after 786 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-recurrence-confirmed)

### 2026-11-26T17:45:41.479Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review (issue-review-sweeper, 2026-11-26) of issue-450cfc80a0b9. Read-only; no source edited, built, closed, or fixed.

Observed facts
- Issue readiness: ready=true, missing=[], score=8/8 (brief). State in_review, owner unset, one prior review (build-wait-reviewer, 2026-11-21T15:41:09Z, verdict reviewed-needs-policy-fix). Acceptance is met by the record itself: wait_id, stage, reason, elapsed, build/source-generation and request_ids are all present, and a reviewer wake was queued once.
- The prior finding is unchanged in code: devscripts/dev-loop.py:114-115 still defines one global WAIT_ISSUE_SECONDS=max(10, MIMITA_WAIT_ISSUE_SECONDS or 60). check_wait_budget (dev-loop.py:3884-3900) still flags game-dll-build when utc-age since game_build_started_utc >= 60 with no progress/stall test. Dedup is still per wait_id only (dev-loop.py:3901-3904, reported_wait_ids keyed on game_build_started_utc at :3889), so each new start timestamp creates a new issue; there is no cross-attempt dedup by (kind,stage,generation).
- Recurrence is ongoing, not historical: 37 open issues titled "Wait over budget: game-dll-build at game_build" (5 new, 32 in_review) as of ~17:43Z, including newly created issue-b73ec4904f0c (17:43:33Z), issue-a02e5a6588db (17:21Z), issue-45733b82bbbc (17:28Z), issue-24863e1f595e (17:40Z). Several were auto-opened within the last hour.
- Current sibling evidence (issue-a02e5a6588db): elapsed=61.28s, stage=game_build, generation=703, reason still the initial "[GAME BUILD] revision=... sources=644 jobs=10 priority=high ... seeded=0" line; started_utc=17:20:46.773Z while build_id=16:59:26.767Z (about 21 min earlier). So the 60s clock is measuring stage-entry age, not demonstrated build progress.

Inference / uncertainty
- Inference: the wait policy fires on total stage age, so it cannot distinguish an actively-progressing long build from a stalled one and will keep emitting one issue + one wake per start timestamp. That is the noise mechanism; the issue is a working-as-designed detector with an overly coarse signal, not a unique defect.
- Uncertain: "false positive" for this 2026-11-21 occurrence is plausible (reason showed packet-registry codegen 6.5s in) but not proven solely from the reason string; reason text is a stage marker, not a liveness beacon.

Contrary evidence
- Recent actual game-DLL build timings are well under 60s: .dev/build-timings/game-dll-20261126T173819997450Z-5cdb02.json total_seconds=38.88 (gen 709), game-dll-20261126T173539751579Z-9182c6.json total_seconds=26.05 (gen 708), game-dll-20261126T174154804725Z-a751bd.json total_seconds=10.67. So >=60s is not a universal cost of game_build; some flagged occurrences may reflect real queue/contention slowness rather than a pure false positive. This weakens a blanket "every occurrence is false" claim while leaving the noise/dedup finding intact.

Next discriminating check
- For one current occurrence (e.g., issue-a02e5a6588db, generation 703, started 17:20:46.773Z), correlate check_wait_budget's stage-entry age and reason string against that attempt's .dev/build-timings phase timeline and the dev-loop game-build stage events: at the 61.28s mark, was the attempt (a) still queued/pre-start, (b) actively compiling/relinking, or (c) stalled? That single correlation decides true-positive vs false-positive per occurrence and tells the build owner whether to add queue-time separation or progress-based stall detection first.

Verdict: reviewed-recurrence-confirmed. Issue is not independently resolvable as a one-off; it is one of a growing duplicate family and should be tracked under the wait-policy umbrella (issue-e044a931c54b / issue-7d3dcfd7433b), not closed by this reviewer. No source, build, or lease touched.
