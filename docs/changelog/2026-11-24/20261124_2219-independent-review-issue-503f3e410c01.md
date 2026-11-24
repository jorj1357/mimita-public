# Independent review: melee damage, arcade collision, and mace active pose (issue-503f3e410c01)

**Date:** 2026-11-24 UTC
**Agent:** issue-review-sweeper
**Wake:** `wake-e826520e910c` (scheduled issue review)
**Issue:** `issue-503f3e410c01` — "Melee damage, arcade collision, and mace active pose"

## Result

Read-only bounded review: no source edits, no build, no run, no lease, no close,
no owner claim. Appended review id 4 (verdict `inconclusive`); prior review text
preserved. Issue remains `in_review` with all three tasks open and unproven.

## Findings

- Readiness 8/8. `proof status` still shows all three tasks with
  `status=runtime_proven`, `result=unproven`, empty `verified_utc`/`journal_path`,
  citing the failed run `20261120_072010`. Unchanged proof-integrity defect on
  `issue-b396205aaecf`.
- **Source drift since the prior review.** `src/network/server-players.cpp`
  changed at `186eebd2` (2026-11-23 19:07 EST) and `51306c08` (2026-11-24 00:22
  EST); current sha256 `69e5b9d2…` vs recorded `182b68c3…`.
  `src/physics/movement/physics-collision-glb-main.cpp` current sha256
  `0abedb32…` vs recorded `5b77a843…` (changed `d651e1332`, 2026-11-20 18:25).
  `task-proof-899d76b7464a` therefore mismatches 2/2 recorded paths but reports
  no stale reason. The melee-logic files still match their recorded hashes.
- **Zero new player Mace evidence** under `logs/2026-11-24`: no
  `melee.client.contact_claim`, no `melee.contact.claim.accepted`, no claim
  rejects. Only 16 `melee.client.action`, all `weapon=captain_falcon_moveset`
  (run `20261124_205956`). `mace_active_contact` has no new parity or white-pose
  evidence.
- **New source-current NPC-to-player melee in Arcade:** run `20261124_121332`
  (Bloodtest/Slaughterhouse; game DLL generation 594, revision `73a9b963…`)
  has 118 `weapon.contact.result` npc→player, all `weapon=bloodtest_bat`, plus
  2077 NPC→player `physical_contact` damage, with `arcade_mode=true` in
  3543/3543 sampled capsule solves. Run `20261124_170535` adds 58 / 1018 with
  `arcade_mode=true` in 776/776 samples. Candidate for `arcade_npc_melee_hit`,
  but the weapon differs from Mace/Chainsaw/SwordSword and needs owner field
  confirmation.
- 2026-11-24 `collision.arcade_pose_skipped` samples are all zero-part
  (`body_part_count=0`, `extra_part_count=0`); the Nov 22/23 nonzero-part
  evidence was not reproduced this cycle.
- `issue-adfeaafa1136` (human melee blocker) remains assigned to an inactive
  owner; no player melee claim was attempted this cycle.

## Process review

Repeated delay confirmed for the fourth consecutive sweep: the proof-integrity
defect (runtime_proven on an unproven failed run) persists and the existing
`issue-b396205aaecf` proposal is unimplemented. One new, non-duplicate facet
was appended to `issue-b396205aaecf`: recorded proof `source_hashes` can drift
without `proof status` reporting stale (task-proof-899d76b7464a mismatches 2/2
paths). The three `brain_check.build_resource_gaps` are already recorded on
`issue-136d4553ff92` / `issue-6692376723be` / `issue-98a885a0bb81`, so none
were duplicated. One issue-linked question was sent to `melee-arcade-fix`
(`msg-1e7e23650b85`).

## References

- `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md`
- `logs/2026-11-24/20261124_121332/server-01/events-0005.jsonl`
- `logs/2026-11-24/20261124_170535/server-01/`
- `logs/2026-11-24/20261124_205956/client-01/events.jsonl`
- `src/network/server-players.cpp`, `src/physics/movement/physics-collision-glb-main.cpp`
- `src/physics/movement/actor-final-pose.cpp:466`, `src/physics/movement/physics-collision-glb-main.cpp:217-225`
