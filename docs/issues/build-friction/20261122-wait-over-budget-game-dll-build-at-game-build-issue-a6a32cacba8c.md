# issue-a6a32cacba8c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_legitimate_wait_succeeded; duplicate_issue_occurrence=true; code_fix_needed=no; wake_queued_once=true)

### 2026-11-22T06:50:35.126Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review 2026-11-22T06:34Z by build-wait-reviewer (wake-ee339c68b652). Read-only: no source edit, no build requested by this agent, no fix or close claimed.

VERDICT: confirmed_legitimate_wait_succeeded; not_stalled; duplicate_issue_occurrence=true; code_fix_needed=no.

- wait_id game-dll-build:2026-11-22T06:08:02.690Z fired at 06:09:03.004Z (60.275s) while build attempt game-dll-20261122T060802960932Z-cbaf8e (generation 464, source_generation 6, request auto-6, changed file src/npc/npc-navigator.cpp, note f1db1c02f256) was genuinely in flight.
- That build SUCCEEDED: success=true, completed 2026-11-22T06:10:05.908Z, total_seconds=122.937 (worker_total 87.274). Phase durations: changed_source_preflight=61.469s, link=16.619s, object_cache_scan=4.381s, compile=1.346s, publish=0.545s. Slowest compile: src/npc/npc-navigator.cpp=59.465s.
- Conclusion: the 60s wait is a TRUE POSITIVE, not a false alarm. A single npc-navigator.cpp change alone exceeds 60s both in changed_source_preflight (~61.5s) and in the single translation-unit compile (~59.5s), plus ~16.6s link. No stall and no build-infrastructure fault.
- Evidence: .dev/build-timings/game-dll-20261122T060802960932Z-cbaf8e.json (dll build\game\generation-464\game-g464.dll) and its .jsonl sibling.

EVIDENCE-FIDELITY NOTE (already tracked elsewhere)
- The wait record stores generation=463 and build_id=2026-11-22T02:37:06.972018Z, while the actual attempt is generation 464. At wait-check time dev-loop.py:3533 reports self.game_build_generation, which only advances on completion (dev-loop.py:2967), so it lags by one; build_id is the last EXE artifact, not the game-DLL attempt id. Traceability required correlating started_utc to the build-timings file. The same off-by-one appears in issue-c481f05923e4 (gen 466 vs attempt 467) and issue-86b3b2ce1e73 (gen 467 vs attempt 468); this is the same policy/observability gap already reviewed on issue-6b55a3773999.

POLICY / CONSOLIDATION
- check_wait_budget keys wait_id on game_build_started_utc (dev-loop.py:3520-3536), so every >60s build mints a separate issue; the prior review recorded 75 open duplicate-title wait issues. Retain this record as occurrence evidence and triage as duplicate-of-signature noise under issue-136d4553ff92 (build-coordinator) and program issue-e044a931c54b (build-runtime-subagents) rather than opening a further issue.
- Suggested follow-up (not implemented here): make the 60s budget progress-aware using per-stage elapsed/heartbeat so it does not fire while compile/link is making measured progress, and suppress a new wait issue while a previous wait for the same source revision is still open.

WAKE / ACCEPTANCE
- Wait record carries wait_id, stage, reason, elapsed, request_ids, source_generation, and started_utc; one reviewer wake (wake-ee339c68b652) was queued for this occurrence. The structured generation/build_id fields are stale (noted above), so wake-once acceptance holds but exact build identity is only recoverable by timestamp correlation.

No code fix claimed. Build result is separate from behavior proof; no runtime claim is made.
