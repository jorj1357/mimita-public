# issue-bccd816a3abd: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_legitimate_wait; diagnostic_reason_corrupt=true; duplicate_issue_occurrence=true; code_fix_needed=no; wake_queued_once=true)

### 2026-11-22T06:20:07.748Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review 2026-11-22T06:14Z by build-wait-reviewer (wake-07e6bf519cd0). Read-only: no source edit, no build/test requested by this agent, no fix or close claimed. Examined issue brief/readiness (ready, score 8, no missing fields), the exact wait_id, .dev/build-timings for the wait window, sibling wait issues, the dev-loop wait-budget source, and the wake journal.

VERDICT: confirmed_legitimate_wait (real in-flight build, not a stalled or duplicate infrastructure fault), plus one new diagnostic-quality defect: the recorded wait reason is corrupted/truncated.

WAIT + BUILD TIMELINE
- wait_id game-dll-build:2026-11-22T05:51:04.003Z fired at 60.619s at the 60s budget while the game-DLL build was genuinely in flight.
- Waited build: attempt game-dll-20261122T055104141967Z-435000, generation 459, request auto-2, revision 6abf1cd3..., changed source src/npc/npc-navigator.cpp. Started 05:51:04.142Z; ended 05:52:23.664Z; total 79.503s; success=false; failed_phase=compile. Evidence: .dev/build-timings/game-dll-20261122T055104141967Z-435000.json (worker tail: "compile failed: 1 file(s) in 39.785s") and its .jsonl.
- First divergence (real cause): src/npc/npc-navigator.cpp did not compile - error: 'const class Npc' has no member named 'team' at line 760 (npc.team), plus a follow-on nlohmann::json brace-init error at line 766. The wait was a failing compile, not an infrastructure hang.
- Recovery: note 408af55ca1dc ("Remove invalid team field from area-escape event...", created 05:52:02.552Z) fixed the source; retry game-dll-20261122T055224056464Z-80d87f (auto-3) compiled and published generation 462 at 05:54:05.911Z after 101.696s (success=true). Current src/npc/npc-navigator.cpp:758-770 no longer references npc.team.

NEW FINDING - recorded wait reason is corrupt
- Issue evidence "reason" is ":40535.598,\"build_attempt_id\":\"game-dll-20261122T055104141967Z-435000\",...}". This is not a human-readable reason; it is a mid-JSON tail of the last [GAME_FILE] event. It exactly matches the translation_unit.completed row for generation-461/module-identity-provider (elapsed_ms=40535.598, progress_write_ms=9.773, record_overhead_ms=11.2) at .dev/build-timings/game-dll-20261122T055104141967Z-435000.jsonl line 755.
- Owner/localization: _record_game_build_output sets detail = line.rstrip()[-240:] for every stdout line and assigns it to self.game_build_detail (devscripts/dev-loop.py:2829-2864); check_wait_budget copies game_build_detail into wait["reason"] (devscripts/dev-loop.py:3528). For structured [GAME_FILE]/[GAME_STAGE]/etc lines this yields a truncated JSON fragment instead of a readable reason.
- Impact: the issue carries stage/elapsed/build id (acceptance technically met) but "reason" is unusable - exactly the evidence a reviewer needs. Suggested minimal fix for the owner: for structured prefixes, derive a short human summary (phase + counts + last file status) instead of the raw 240-char tail; keep raw JSON in a separate field. Not implemented here.

WAKE / ISSUE POLICY EVIDENCE
- Wake queued exactly once for this occurrence: .dev/agent-wake/events.jsonl has one wake.queued for wake-07e6bf519cd0 at 2026-11-22T05:52:06.781Z targeting issue-bccd816a3abd. Acceptance holds.
- Duplicate signature: same wait as issue-4fe1a7a2a4bf (05:49:32Z, 60.315s, attempt 9f7d98) and issue-6d795125c36f (05:52:23Z, attempt 80d87f, which actually succeeded). check_wait_budget keys wait_id on game_build_started_utc (devscripts/dev-loop.py:3520-3536), so each retry >60s mints a new issue; three issues were created in ~3 minutes for one broken edit plus its retries.
- Root policy gap: WAIT_ISSUE_SECONDS defaults to 60s (devscripts/dev-loop.py:107-110), but measured one-source incremental game-DLL wall time here was 79.5s (failed) and 101.7s (success). Any ordinary one-file build trips the budget. Owners: issue-136d4553ff92 (latency/observability) and issue-e044a931c54b (avoidable waits). Suggested: progress-aware wait using build-worker heartbeat, or align the budget to measured per-stage norms, and suppress a new wait issue while an earlier wait for the same revision is open.

No code fix claimed. Build evidence is separate from behavior proof; no runtime claim is made. Recommend triage as duplicate-of-signature noise consolidated under issue-136d4553ff92, retaining this record as occurrence evidence for wait_id game-dll-build:2026-11-22T05:51:04.003Z.
