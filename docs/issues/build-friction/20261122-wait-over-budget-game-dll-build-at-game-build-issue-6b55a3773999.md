# issue-6b55a3773999: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_legitimate_wait_succeeded; duplicate_issue_occurrence=true; code_fix_needed=no; wake_queued_once=true)

### 2026-11-22T06:29:35.720Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review 2026-11-22T06:2xZ by build-wait-reviewer (wake-88130a84900d). Read-only: no source edit, no build requested by this agent, no fix or close claimed. Examined issue brief/readiness (ready, missing none, score 8), the exact wait_id, .dev/build-timings/game-dll-20261122T060035669341Z-4e8b8d.json and .jsonl, sibling wait issues, and the wake journal.

VERDICT: confirmed_legitimate_wait_succeeded (not a stalled/duplicate infrastructure fault).
- wait_id game-dll-build:2026-11-22T06:00:35.562Z fired at 06:01:37.378Z (61.77s) while build attempt game-dll-20261122T060035669341Z-4e8b8d (generation 463, request auto-4, changed file src/npc/npc-navigator.cpp, note 905e8f929e5e) was genuinely in flight.
- That build SUCCEEDED: success=true, completed 2026-11-22T06:04:40.753Z, total_seconds=245.06. Phase durations: changed_source_preflight=126.35s (npc-navigator.cpp compile=125.742s), link=36.974s, object_cache_scan=7.994s, compile=2.027s, publish=0.997s. Evidence: .dev/build-timings/game-dll-20261122T060035669341Z-4e8b8d.json (generation=463, dll=build\game\generation-463\game-g463.dll).
- FIRST DIVERGENCE (real cause) is NOT in the build data path; it is the wait policy. WAIT_ISSUE_SECONDS defaults to 60s (devscripts/dev-loop.py:107-110), but the observed normal one-source incremental game-DLL wall time in this window is ~90-245s even on success. Any ordinary one-file build therefore trips the budget and mints an issue.

EVIDENCE-FIDELITY NOTES
- The issue evidence records generation 462, but the attempt published generation 463. At wait-check time dev-loop.py:3533 reported self.game_build_generation (462), which lagged the attempt actual generation (463). Minor off-by-one/identity-lag in the recorded wait evidence; not a behavior fault.
- The independent root dev-loop wait event (dev-loop.wait.over_budget) for this wait_id is not present in any retained run journal under logs/2026-11-22; the daemon that emitted it was replaced before the current daemon started at 06:23:47Z, and the current state events_path logs/2026-11-22/20261122_011031/dev-loop/events.jsonl does not exist. Retained trace is the issue captured wait record plus the wake journal. Flag this evidence-retention gap for issue-136d4553ff92 / root-connection-reliability.

WAKE/ISSUE POLICY EVIDENCE
- Wake queued exactly once for this occurrence: .dev/agent-wake/events.jsonl has one wake.queued for wake-88130a84900d at 06:01:40.527Z. The codex adapter exited and an opencode fallback launched at 06:09:06; those are transport retries, not duplicate issues/wakes. Acceptance holds for this occurrence.
- DUPLICATE-SIGNATURE NOISE (different wait_ids): each genuine retry/build over 60s mints its own issue because check_wait_budget keys wait_id on game_build_started_utc (devscripts/dev-loop.py:3520-3536). There are currently 75 open issues titled Wait over budget: game-dll-build at game_build out of 101 open build-friction issues.
- ROOT POLICY GAP: same latency/observability problem owned by issue-136d4553ff92 (build-coordinator) and the avoidable-wait program issue-e044a931c54b (build-runtime-subagents). Suggested follow-up (not implemented here): make the wait budget progress-aware using the build worker per-stage elapsed/heartbeat (for example do not fire while compile/link stages are making measured progress), and/or align the budget to measured per-stage norms; suppress a new wait issue while a previous wait for the same source revision is still open.

No code fix claimed. Build result is separate from behavior proof; no runtime claim is made. Recommend triage as duplicate-of-signature noise consolidated under issue-136d4553ff92, retaining this record as the occurrence evidence.
