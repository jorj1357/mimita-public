# Independent review: melee damage, arcade collision, and mace active pose (issue-503f3e410c01)

**Date:** 2026-11-23 UTC
**Agent:** issue-review-sweeper
**Wake:** `wake-c4a674630604` (scheduled issue review)
**Issue:** `issue-503f3e410c01` — "Melee damage, arcade collision, and mace active pose"

## Result

Read-only bounded review: no source edits, no build, no run, no lease, no close,
no owner claim. Appended review id 3 (verdict `inconclusive`); prior review text
preserved. Issue remains `in_review` with all three tasks open and unproven.

## Findings

- New source-current player-melee runtime exists in unrelated agents' isolated
  runs (`a065471f` is still the last commit touching the melee/arcade paths and
  those paths are clean).
- `logs/2026-11-22/20261122_205852` (juggernaut horde, gen519, DLL sha256
  `d7fc116cd0ea84168340bcf374bebca28be0d66bfa0ba66e3a3ecd6243bb4d14`): 2037
  server-accepted/joined claim ids, 49 joined, 39 exact predicted==accepted;
  27 both-active of which 23 exact; accepted active damage 250..2438 at
  multiplier 10.0; arcade_mode=true in all 2824 samples; zero future_contact
  rejects.
- `logs/2026-11-22/20261122_210337` (fun counter strike, gen523, DLL sha256
  `d65291be7a55612379a91fb9defe21ff2216e1c52e1fc76f7b64b9c008bb46c2`): 106
  accepted Mace player->NPC claims, 44 active at 10x, 104/106 exact; 42/44
  server-active exact. The two mismatches are client action_active=false
  elapsed 0 vs server active elapsed 30/35 (contact_ids 163, 702).
- `arcade_pose_skipped` now reports nonzero held-weapon parts (1506/2993 in
  205852, 1248/2497 in 210337, 2505/9554 in `20261123_144429`), closing the
  prior "zero-part counter" gap in kind.
- `logs/2026-11-23/20261123_144429` (juggernaut horde) has 53 NPC->player
  weapon.contact.result (chainsaw 34, mace 7, swordsword 12) plus 422 NPC->player
  physical_contact damage events under arcade_mode=true.
- White active pose still has no journal predicate: the tint is computed at
  `src/combat/weapon-viewmodel.cpp:233-245,618-627` but only printf'd under
  `DebugConfig::DEBUG_WEAPON_VIEWMODEL`. No tint event in any recent journal.
- Proof rows still show `status=runtime_proven` with `result=unproven` and the
  failed run `20261120_072010`; issue `related_run` is still stale. This is the
  existing proof-integrity defect on `issue-b396205aaecf`.
- `issue-adfeaafa1136` (future_contact blocker) is still assigned to an inactive
  owner but its symptom does not reproduce on gen519/gen523.

## Process review

Repeated delay confirmed: three proof-bearing journals from other agents' runs
sat uncorrelated for ~24 h; the runtime-registry correlation proposal already
exists on `issue-b396205aaecf`, so none was duplicated. One new proposal
(machine-provable white active pose via a transition-gated
`weapon.viewmodel.tint` event) was appended to `issue-89efbbc86535`.

## References

- `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md`
- `logs/2026-11-22/20261122_205852/`
- `logs/2026-11-22/20261122_210337/`
- `logs/2026-11-23/20261123_144429/`
- `src/combat/weapon-viewmodel.cpp:233-245,618-627`
