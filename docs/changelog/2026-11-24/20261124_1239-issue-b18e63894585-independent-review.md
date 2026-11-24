# Independent bounded review of issue-b18e63894585

- Task ID: wake-b82da7cc7f7e
- Agent: issue-review-sweeper
- Focus: one read-only review of elite NPC objective/navigation regression on recent `jugger_horde` journals (gen587 run `20261124_081518`, gen590 control `20261124_110111`)
- Summary: Appended one independent review (review id 357) to `issue-b18e63894585` and one measured diagnostic-volume proposal to `issue-3f776ce60511`. No source edit, no build request, no test run, no close, no fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-24T12:39Z, ISO 8601

# Pre-existing changes

- `git status` retains 72 dirty/untracked entries from other agents/sessions. This session modified only the two reviewed issue docs and added this changelog; it created no source, config, or spec change. No compiler, build.py, module worker, or dev-loop was invoked.

# Requested behavior

Run one bounded independent review of `issue-b18e63894585`: inspect issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then a short development-process review with one specific proposal to the best existing improvement issue (or none if unsupported). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/sync/observe; observation separated from inference; no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: no runtime proof claimed from a build or from another agent's journal.
- `docs/specs/gamemodes/juggernaut-horde.md` (Fighter/Juggernaut hostile targeting) used as the behavior reference.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` | scheduling note only, 0 reviews | + one review (id 357) | required independent review record | remove only the appended review |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` | prior agent suggestion | + 1 process proposal | measured journal-volume reduction target | remove only the appended suggestion |
| `.dev/agent-coord/brain.db` | issue state `new` | review, state `in_review`, +1 suggestion | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-24/20261124_1239-issue-b18e63894585-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

`issue_review` added review id 357 to `issue-b18e63894585` (state `new` -> `in_review`, reviewer `issue-review-sweeper`). One suggestion appended to `issue-3f776ce60511` (state unchanged `in_review`).

# Diagnostics

- Readiness `ready=true, missing=[], score=8`; tasks `elite-npc-aggression-implementation` (owner `fcs-npc-live-fix`, open) and `elite-npc-regression-review` (unowned, open); both `latest_proof=null`.
- The issue's cited journal `logs/2026-11-20/20261120_232200/client-01/events.jsonl` does not exist; `logs/2026-11-20` is empty.
- `20261124_110111` (gen590, dust2cyberiav4, `--npcs 5`): 20 `npc.target-sense`, 90 `npc.attack-gate` (28 `target_visible=true`), 87 `npc.target-changed`, 9 `npc.shot` `fired=true`, 17 `server.damage.applied`.
- `20261124_081518` server (gen587, 08:15-10:17Z): 14 segments, 1,292.1 MiB; whole run has 228 `server.damage.applied` `victim_is_npc=true` and 5,988 `npc.shot` `hit_entity=true`.
- Final window 10:10:37.751Z-10:17:06.209Z: 8,128 `npc.attack-gate` all `target_id=0` and `no_visible_target`; 0 `npc.shot`; 0 `server.damage.applied`; 0 `npc.target-changed`; 1,799 `npc.stuck-episode`; 342 `npc.movement-progress-failed`; 2,171 `npc.target-sense` `target_visible=false`.
- Phase telemetry same window: `npcs=67`, `npc_over=true`, `max_npc_ms=61.89` vs budget 10; `living_npcs=63`; `target_npc_candidates=31,691`, `target_player_candidates=473`, `target_scans=473`, `target_scan_skipped=1,417`.
- `npc.trace` 10:17:05.776Z: 200059-200062 fighters team 0 PATROL; 200064-200066 juggernauts team 1 RETREAT; only 200063 reported dead (`health=0`).

# Validation

- `agent-coord.py begin/sync/observe`, `issue readiness`, `issue show`, `issue brief`, and `proof status` reviewed.
- `issue review` appended review id 357; `issue suggest` appended the process proposal to `issue-3f776ce60511`.
- No compiler, build request, module worker, second dev-loop, or runtime test run was invoked.

# Findings

1. The fix is unimplemented and unproven; both tasks lack a proof record.
2. A recent gen587 full-roster Horde run ends with >=3 live enemy NPCs present while all sampled fighters hold `target_id=0` and all 8,128 attack gates suppress `no_visible_target`, with zero shots/damage for 6.5 min. This reproduces the prolonged-match symptom, but the match/round phase is not logged, so a non-combat phase cannot be ruled out.
3. Contrary evidence: the same run had NPC-vs-NPC damage (228 NPC victims) and hits earlier, and the gen590 5-NPC control fires; this is state-dependent, not a constant "NPCs never shoot" defect.
4. `npc.target-sense` is human-player-only (`src/npc/npc.cpp:548`), so it does not describe NPC-vs-NPC perception; the investigation must use `target_id`/`target_npc_candidates`.
5. Candidate first-failure areas: team comparison in `npcTeamOf()` (`src/network/server-npcs.cpp:2212-2215`), module-vs-gamemode targeting-policy divergence, or target-scan starvation under NPC CPU over-budget. Not distinguished by available journals.

# Confidence

High on the artifact counts, paths, times, readiness, and proof absence (direct reads). Moderate on the symptom being the reported regression. Unknown: whether the final window was an active combat phase, and which target-acquisition step fails.

# Proof

No proof claim. Read-only review; evidence is journal-tier, source-tier, and issue-store-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; the observed symptom is recorded on the issue back to gen587 journals.

# Human acceptance

- Visual, gameplay, multiplayer review: none (not applicable)
- Still unverified: whether elite fighters actually acquire/engage NPC hostiles under a current-generation, active-phase run.

# Agent handoff context

- Owner: `fcs-npc-live-fix`; the unowned `elite-npc-regression-review` task remains claimable.
- Next exact step: one pinned gen593 `jugger_horde` run (3-5 min, fresh process) with >=1 live hostile per team, or 1 fighter NPC + 1 live enemy juggernaut NPC and no human; correlate per fighter `myTeam`, resolved hostile NPC team, `target_npc_candidates`, `npc.target-changed` target, attack-gate `target_id`/`suppression_reason`, `npc.shot` `fired`, `server.damage.applied` `victim_is_npc`.
- What another agent should not redo: do not re-review `20261124_081518` expecting a clean controlled proof (late-run window, phase unknown); do not treat the 5-NPC gen590 control as NPC-vs-NPC proof.

# Automatic evidence

- Git status: shared dirty tree preserved (72 entries); this session added one issue review, one suggestion, one coordination DB change, and this changelog
- Build identity: reviewed runs `20261124_081518` (game generation 587) and `20261124_110111` (game generation 590); no build requested
- Journal paths: `logs/2026-11-24/20261124_081518/server-01/events*.jsonl` (14 files, 1,292.1 MiB), `logs/2026-11-24/20261124_110111/server-01/events.jsonl`

# How to reverse

- Remove only the appended review from the `issue-b18e63894585` doc; do not rewrite prior text.
- Remove only the appended suggestion from the `issue-3f776ce60511` doc.
- Restore `.dev/agent-coord/brain.db` from backup if withdrawn.
- Delete this changelog. No commit to revert.
