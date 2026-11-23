# issue-991c632f1ca0: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-23T17:28:38.955Z — build-wait-reviewer

related_build: game-dll-20261123T162701884743Z-d98804

## Review (confirmed-legitimate-heavy-wait-success; root-cause=npc.h-fanout-rebuild; evidence; wait-journal-events-missing(issue-failed-timeout); identity-mismatch; wake-queued-once; wake-dispatch-late; defect-F-new; no-build-regression)

### 2026-11-23T17:28:38.977Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-56368e82bb5b), read-only on 2026-11-23. No source edit, build, or close performed.

OBSERVED (direct journal)
1. Wait: game-dll-build at stage game_build, elapsed 60.762s, wait_id game-dll-build:2026-11-23T16:27:01.355Z (budget 60s; dev-loop.py:114-115 WAIT_ISSUE_SECONDS). Exactly one issue (this one, created 16:28:02.149Z) and exactly one reviewer wake (wake-56368e82bb5b queued 16:28:04.703Z; .dev/agent-wake/events.jsonl:2234). Per-occurrence acceptance (one issue, one wake) is met.
2. This wait was NOT a stalled or failed build, and NOT merely policy noise: the in-flight attempt game-dll-20261123T162701884743Z-d98804 (generation 564, source_generation 9, request 2026-11-23T16:27:01.143Z-ed14d524) finished success=true and published generation-564 at 16:39:06.569Z after 724.396s (dev-loop.game-generation.ready, journal line 404; .dev/build-timings/game-dll-20261123T162701884743Z-d98804.json total_seconds 724.351). The 60.762s trigger was ~12x below the real cost, so the alert was justified.
3. Root cause = heavy fanout rebuild, not a defect. The sealed batch changed src/npc/npc.h, src/npc/npc.cpp, src/network/server-npcs.cpp, src/network/server-packet-chat.cpp (dev-loop.batch.sealed, journal line 251). It forced 137 TUs (skipped 506, compile_seconds 673.847); slowest server-npcs.cpp 201.698s, npc.cpp 198.381s, server-packet-chat.cpp 194.028s; 31 cache invalidations "dependency newer than object: ...npc.h". npc.h fanout is the cost driver (link to the header-fanout/build-speed work).
4. Required evidence is only PARTIALLY present. The issue record and wake record exist, but the required dev-loop wait events do NOT: there is no dev-loop.wait.over_budget and no dev-loop.wait.reviewer-wake for this wait_id. The journal instead has only dev-loop.wait.issue-failed at 16:28:34.966Z ("could not record over-budget wait"; error: agent-wake.py run --once timed out after 30 seconds). This is the occurrence-specific gap versus proof_expectation "Dev-loop wait event plus the issue record and reviewer wake record"; see DEFECT F.

REPRODUCED KNOWN DEFECTS (cross-linked; not new duplicates)
A. Wrong identity in wait evidence: evidence generation=563 and build_id=2026-11-23T13:26:39.822193Z (previous published build), while the in-flight attempt is generation 564. Root: check_wait_budget reads self.game_build_generation (dev-loop.py:3879), which is set only on completion. Tracked by issue-9a4503f6ed68.
B. Policy noise: a flat 60s budget plus per-wait_id issue minting keeps producing identical "Wait over budget: game-dll-build at game_build" issues (~136 game-dll-build wait issues now; this is one more). Tracked by issue-9a4503f6ed68.
C. Late reviewer dispatch: wake queued 16:28:04.703Z; codex claimed/launched 16:55:39.605/669Z (~27.6 min after queue); stale retry to opencode 17:00:35.575Z; opencode launched 17:12:02.903Z (~44 min after queue), far beyond the <=5 min bound. Tracked by issue-84c584957e65.
(The reason field for THIS occurrence is readable - the [GAME BUILD] revision line - so defect issue-6e08d89b55e3 did not recur here.)

DEFECT F (new; needs a focused bug, not another wait issue)
dev-loop.wait.issue-failed masks a successful record and suppresses the required wait events. In check_wait_budget (dev-loop.py:3887-3912), record_wait_incident() succeeds and wait_id is added to reported_wait_ids (3891), then wake_reviewer() (3895) raises because its inline subprocess.run([... agent-wake.py run --once], timeout=30) in build_friction.py:244-246 exceeds 30s. The except at 3908 then logs issue-failed and skips both dev-loop.wait.reviewer-wake (3897-3902) and dev-loop.wait.over_budget (3903-3907), even though the issue and the queued wake both exist. Effects: (i) the journal falsely reports that the wait record failed; (ii) the required over_budget/reviewer-wake events are missing for this occurrence; (iii) the next occurrence mints yet another issue. Suggested safe fix (needs human approval, not applied): emit dev-loop.wait.over_budget immediately after a successful record_wait_incident and before the best-effort inline launch; make the inline "run --once" fire-and-forget (short timeout or swallowed) so launch latency cannot retract a successful record; reserve issue-failed for genuine record failures.

RECOMMENDATION
- Treat this as one legitimate over-budget wait on a correct, successful, intrinsic heavy (npc.h-fanout) build; no build regression and no source fix to the build itself.
- Route policy/identity/noise/dedupe to issue-9a4503f6ed68, wake latency to issue-84c584957e65, and DEFECT F to a new focused bug (owner: dev-loop/build-friction) once approved.
- related_build set to the exact attempt game-dll-20261123T162701884743Z-d98804.

PROOF NOTE: no proof record exists for this task. The build timing JSON, dev-loop journal, issue record, and wake queue above are direct evidence; they prove detection/dispatch and build outcome, not that the flat 60s policy is correct.
CONFIDENCE: high for build outcome, duration, and root cause (direct build-timings JSON + journal); high for missing wait events and the issue-failed cause (journal + code); high for wake timings (agent-wake journal).
