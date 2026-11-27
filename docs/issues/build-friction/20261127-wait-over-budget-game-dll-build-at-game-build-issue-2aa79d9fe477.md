# issue-2aa79d9fe477: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-recurrence-not-a-stall)

### 2026-11-27T00:57:46.530Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-f93906ee6177) at 2026-11-27T00:56Z. Read-only: no source edit, no compiler, no build/test started by this agent, no fix or closure claimed.

ACCEPTANCE / PROOF-EXPECTATION CHECK (observability-friction record, not a behavior claim)
- Wait id, stage, reason, elapsed, build/run IDs and evidence are present: wait_id game-dll-build:2026-11-27T00:23:02.531Z, stage game_build, elapsed 60.044s, budget 60.0s, build_id 2026-11-26T22:52:33.481041Z, source_generation 11, request_ids [2026-11-27T00:23:02.292Z-91838ee2:mode3:game]. Gaps: recorded generation=null and no run_id (see CANONICAL ISSUES).
- events.jsonl (run 20261127_001718) line 97 dev-loop.wait.over_budget created=true (00:24:22.236Z); line 96 exactly ONE dev-loop.wait.reviewer-wake for this wait_id (queued=true, adapter=codex, wake-f93906ee6177). One wake per occurrence confirmed. Proof expectation (wait event + issue record + reviewer wake) satisfied.

FIRST DIVERGENCE / WAIT SEMANTICS
- This wait was NOT a stall. The same attempt emitted healthy dev-loop.game-build.file 'compiled' rows DURING the 60s window: player-render-mesh 00:23:29.604 (12.25s), debug-diag-commands 00:23:32.499 (15.151s), vehicle-system 00:23:45.000 (27.641s), module-identity-provider 00:23:49.983 (0.861s), weapon-attachment 00:24:16.524 (26.281s). The alarm fired because the flat WAIT_ISSUE_SECONDS=60 budget (dev-loop.py:3839-3900) is far below a normal full-DLL wall time, not because work stopped.
- Per-TU queue_wait_ms was ~856,000-917,000ms (~14-15 min) before many TUs started (e.g. src/terminal/npc-commands.cpp 856250ms; src/terminal/network-commands.cpp 856156ms), so most of this attempt's 1015.86s elapsed before this batch's compiles actually began.
- Attempt game-dll-20261127T002302708617Z-988794 (source_generation 11, real generation 744, revision 9b86e55e7afa804e4eee4e7ce3a6308b91d01713b13126c646bfbe7301f498c5) ran 1015.86s and FAILED at link in 25.703s. That link break (undefined PlayerAiController::active/meleeWeapon/buildFrame/setMeleeWeapon/setActive and ClientCollisionWorldView ctor/vtable) is separately owned by opencode-deepseek-v4-1 (build note 14515e15a4b7, 00:49:45Z) and is NOT part of this wait-budget issue; do not conflate them.

SECOND DEFECT (evidence quality)
- reason is a mid-JSON tail starting `:47093.0,"build_attempt_id":...` rather than a readable stage detail. Owner: dev-loop.py:3112 (detail = line.rstrip()[-240:]) assigned to game_build_detail at :3142. A >240-char progress line truncates to a numeric tail.

DUPLICATE-FLOOD MEASUREMENT (supports the issue's own risk field)
- As of ~00:51Z there are 43 open build-friction issues titled 'Wait over budget: game-dll-build at game_build' (41 in_review, 2 new), 0 owned, 0 reviewed, spanning 2026-11-21..2026-11-27. build_friction.record_wait_incident keys uniqueness on per-occurrence wait_id, so every fresh >60s full build creates another issue even when kind/stage/reason are identical. The wake prompt's 'add the occurrence instead of creating a duplicate issue' is therefore contradicted by the code path the dev-loop actually uses.

CANONICAL ISSUES (evidence appended, no duplicates created)
- issue-9a4503f6ed68 (owner wake-proof-wait): identity + reason-tail occurrence appended (build/run-ID gap, reason JSON tail).
- issue-136d4553ff92 (owner build-coordinator): budget-vs-wall-time + active-progress + queue-wait occurrence appended.

CONFIDENCE
- High: exactly-once issue+wake for this wait; the wait was healthy progress not a stall; 60s budget below natural wall time; reason is a JSON tail; 43 open duplicates; attempt identity and link failure facts.
- No runtime/behavior proof applies to this record. Other live game pairs were left untouched.
