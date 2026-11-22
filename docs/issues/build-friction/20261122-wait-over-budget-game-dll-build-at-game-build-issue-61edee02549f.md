# issue-61edee02549f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-22T07:17:30.140Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed over-budget game-dll-build wait occurrence from pinned build-timings journal, issue record, and agent-wake journal (no source edited, no build/test started, no files claimed).

VERDICT: confirmed genuine over-budget wait; detector and wake path behaved as designed; issue fields complete; build was healthy but slow, not wedged.

Identity
- wait_id game-dll-build:2026-11-22T06:46:09.130Z; kind game-dll-build; stage game_build (hardcoded in check_wait_budget, devscripts/dev-loop.py:3527); budget 60.0s; elapsed 60.847s; recorded 2026-11-22T06:47:10.020Z (60.847s after started_utc 06:46:09.130Z �� exactly as the attempt crossed 60s).
- source_generation 5; request_ids [auto-5, 2026-11-22T06:46:40.920Z-bb2073a8]; build note 45e247ec84a0 (agent fun-cs-npc-objective, src/npc/npc-navigator.cpp).

In-flight attempt (the wait points at real build work)
- game-dll-20261122T064609195741Z-152751 (via .dev/build-timings/game-dll-20261122T064609195741Z-152751.json): generation 469, revision cac9473c, started 06:46:09.196Z, completed 06:48:07.896Z, total 118.687s, success=true.
- At the 60.847s deadline (~06:47:09.977Z) it was mid-compile: translation_unit src/npc/npc-navigator.cpp started 06:46:24.874Z, completed 06:47:32.627Z, 67.48s. Then changed_source_preflight completed 06:47:32.800Z (68.002s), object_cache_scan 4.425s, link.gnu_linker 27.07s, coherence_validation 1.1s, publish 0.78s.
- So the over-budget time is genuine CPU/link work (one heavy changed TU plus link), not a stuck poll or hang.

Wake (exactly once)
- .dev/agent-wake/events.jsonl: wake-b049205384a9 wake.queued 2026-11-22T06:47:11.237Z (2.2s after issue creation) for issue-61edee02549f; claimed/launched 06:47:18.380/06:47:18.439 (codex); wake.retry fallback to opencode 06:53:27.463; claimed/launched 07:09:34.275/07:09:34.322. All entries are the same single wake request, so exactly one reviewer wake was queued for this occurrence.

First divergence / root cause
- Flat WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:109) sits below normal whole-game DLL cost whenever src/npc/npc-navigator.cpp recompiles (67.48s TU here) plus link (~27s). check_wait_budget (devscripts/dev-loop.py:3520-3536) ages from game_build_started_utc and hardcodes stage game_build, so any healthy build over 60s files an issue. Same conclusion as issue-6d795125c36f and issue-86b3b2ce1e73; no per-build regression here.

Attribution defect (persists, not caused by this occurrence)
- evidence.build_id is 2026-11-22T02:37:06.972018Z (previously published EXE artifact), not the in-flight game-DLL attempt game-dll-20261122T064609195741Z-152751. evidence.generation is 468 (previously published) while the in-flight attempt is generation 469. related_build is empty (sibling issue-86b3b2ce1e73 had it set). evidence.reason is a clean [GAME BUILD] revision=... line here (not the truncated progress-tail seen in siblings), but the active phase at detection was the npc-navigator compile, not conveyed by the reason. Suggested: record the game-DLL attempt id/generation and the active phase explicitly.

Recurrence
- Non-closed siblings with the identical title were filed minutes apart: issue-c481f05923e4 (06:34:11), issue-86b3b2ce1e73 (06:38:00), issue-61edee02549f (06:46:09), issue-9c77739954f9 (06:52:24). record_wait_incident dedupes only by exact wait_id (devscripts/build_friction.py:131-169) and wait_id embeds the start timestamp, so every >60s build creates a new issue. docs/specs/dev-loop/change-to-evidence-latency.md mandates one issue plus one reviewer wake per exact wait occurrence and names wait_id as the dedupe key, so per-occurrence issues are spec-conforming; the noise follows from the uncalibrated 60s budget.

Evidence limits
- The dev-loop runtime journal reported by --status (logs/2026-11-22/20261122_011031/dev-loop/events.jsonl) is absent on disk, so dev-loop.wait.over_budget could not be re-read directly. Proof rests on the build-timings summary+timeline, the coordinator issue record, and the agent-wake journal above.

Fields
- reproduction, expected behavior, acceptance, paths, and proof expectation present; readiness ready=true, missing=[]. No missing-field fill required.

Follow-up already tracked: issue-9a4503f6ed68 (owner wake-proof-wait) for the wait hook, plus prior reviewer recommendation for measured per-stage budgets, signature-based accumulation of repeated same-stage waits, and the game_build generation/build_id attribution + reason fixes. No code fix claimed here.
