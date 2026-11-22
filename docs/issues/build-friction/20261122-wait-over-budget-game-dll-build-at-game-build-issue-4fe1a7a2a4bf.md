# issue-4fe1a7a2a4bf: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_legitimate_wait; duplicate_issue_occurrence=true; code_fix_needed=no; wake_queued_once=true)

### 2026-11-22T06:09:51.674Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review 2026-11-22T05:5xZ by build-wait-reviewer (wake-512cb083e2fb). Read-only: no source edit, no build requested by this agent, no fix or close claimed. Examined issue brief/readiness (ready, score 8), the exact wait_id, .dev/build-timings for the wait window, sibling wait issues, and the wake journal.

VERDICT: confirmed_legitimate_wait (not a stalled/duplicate infrastructure fault).
- wait_id game-dll-build:2026-11-22T05:49:32.594Z fired at 05:50:32.909Z (60.315s) while the build was genuinely in flight.
- The waited build is attempt game-dll-20261122T054932720222Z-9f7d98, generation 459, request auto-1, changed file src/npc/npc-navigator.cpp. It started 05:49:32.720Z, ended 05:51:03.540Z, total 90.811s, success=false, failed_phase=changed_source_preflight (47.3s). Evidence: .dev/build-timings/game-dll-20261122T054932720222Z-9f7d98.json.
- FIRST DIVERGENCE (real cause): the changed-source preflight compile failed on the note-9b42616af50c edit (agent fun-cs-npc-objective, 05:45:16.053Z) at src/npc/npc-navigator.cpp:760: error: 'const class Npc' has no member named 'team' (npc.team), plus a follow-on nlohmann::json brace-init error at line 766. The source did not compile, so the wait was not an infrastructure hang.
- Retry timeline: attempt game-dll-20261122T055104141967Z-435000 (auto-2, same revision 6abf1cd3...) failed at compile 05:52:23.664Z after 79.503s (.dev/build-timings/...-435000.json); attempt game-dll-20261122T055224056464Z-80d87f (auto-3, revision 16292a51...) succeeded, generation 462 published 05:54:05.911Z after 101.086s. So the owning agent's follow-up edit cleared the compile error; no lasting build block.

WAKE/ISSUE POLICY EVIDENCE
- Wake was queued exactly once for this wait occurrence: .dev/agent-wake/events.jsonl has one wake.queued for wake-512cb083e2fb (05:50:34.865Z); the later wake.claimed/wake.launched and one adapter retry (codex exited, opencode fallback 05:53:28) are transport retries, not duplicate issues/wakes. Acceptance holds for this occurrence.
- DUPLICATE ISSUE OCCURRENCE (same signature, different wait_ids): each subsequent retry >60s minted its own wait issue because check_wait_budget keys wait_id on game_build_started_utc (devscripts/dev-loop.py:3520-3536): issue-bccd816a3abd (wait 05:51:04.003Z, 60.619s, attempt 435000) and issue-6d795125c36f (wait 05:52:23.994Z, 60.848s, attempt 80d87f, which ultimately SUCCEEDED). Three build-friction issues were created in ~3 minutes for one broken edit plus its retries.
- ROOT POLICY GAP: WAIT_ISSUE_SECONDS defaults to 60s (devscripts/dev-loop.py:107-110), but the observed one-source incremental game-DLL wall time in this window was 79.5-101.1s even on success. Any ordinary one-file build therefore trips the budget, and every retry adds another issue. This is the same latency/observability problem owned by issue-136d4553ff92 and the avoidable-wait problem tracked by issue-e044a931c54b. Suggested follow-up for those owners (not implemented here): make the wait progress-aware using build worker heartbeat/progress rows, or raise/align the budget to measured per-stage norms, and suppress a new wait issue while a previous wait for the same source revision is still open.

No code fix claimed. The build result is separate from behavior proof; no runtime claim is made. Recommend triage as duplicate-of-signature noise consolidated under issue-136d4553ff92, with this record retained as the occurrence evidence.
