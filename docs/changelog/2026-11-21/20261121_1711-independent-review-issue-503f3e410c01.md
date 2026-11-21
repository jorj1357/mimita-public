// 2026-11-21 17:11 UTC
/* purpose
* record one bounded independent issue review activation
* preserve observed facts, inference, contrary evidence, and next checks separately
* no source, config, compiler input, build, or runtime behavior was changed by this session
*/

# Task

- Task ID: work-ee488db94f7a (session-d0828c09236c)
- Wake: wake-b3253e1df142
- Owner: issue-review-sweeper
- Focus: issue-503f3e410c01 (melee damage, Arcade collision, Mace active pose)
- Summary: Completed one bounded read-only review of issue-503. Filled the two missing readiness fields, reviewed proof status and the latest pinned journals, appended review id 52 with observed facts, inference/uncertainty, contrary evidence, and ordered next checks, and appended one process proposal to issue-b396205aaecf. No source, build, close, or fix claim was made.
- Status: REVIEW_COMPLETE_WITH_OPEN_PROOF
- Date, time, timezone: 2026-11-21T17:11Z, ISO 8601
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: none

# Pre-existing changes

- The shared checkout was already heavily modified before this activation. `git status --short` showed modified config, devscripts, docs, specs, source, and untracked issue/changelog files. They are not attributed to this session.
- This session changed only the shared issue records for issue-503 and issue-b396205aaecf, coordination activity, and this changelog. It edited no source, compiler input, build script, or leased file.
- No build or test was requested or run.

# Requested behavior

- Wake prompt: complete one bounded independent review of issue-503f3e410c01, starting with issue brief/readiness, proof status, linked artifacts, and the latest relevant journals. Append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and the next discriminating check. Do not edit source, build, close, or claim a fix. Then append at most one evidence-backed process proposal with a measurable target and smallest experiment to the best existing improvement issue.

# Specification alignment

- `AGENTS.md` gates 0b/0c: a woken agent must inspect issue brief, readiness, and proof state; may repair missing context; cannot claim behavior completion without verified runtime proof or a reasoned exemption.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: build success is separate from runtime proof; proof must name the exact run, build/generation, journal, and predicate.
- `docs/specs/dev-loop/agent-wake.md` and `agent-coordination.md`: a wake is bounded; reviews and evidence are append-only; the owner remains responsible for the behavior task.
- `docs/workflows/issue-intake-and-triage.md`: issue context fields are required for readiness.

# Exact implementation changes

- None in source, config, compiler inputs, or build scripts.
- Issue-503 readiness: added `proof_expectation` and `risk` with `issue context`; the issue moved from 6/8 to 8/8 ready. The three behavior tasks remain `open` and `unproven`.
- Issue-503 review: appended review id 52 with verdict `inconclusive`; issue state is now `in_review`.
- Process proposal: appended one proposal to issue-b396205aaecf (runtime registry and build-to-proof links).

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md` | Appended a Context block (proof expectation, risk) and review id 52 by the coordination tool. | Append-only issue history; do not remove. |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` | Appended one process proposal by the coordination tool. | Append-only issue history; do not remove. |
| `.dev/agent-coord` issue records and activity | Stored the review, readiness fields, proposal, and activity. | Coordination state is append-only audit history; do not delete. |
| `docs/changelog/2026-11-21/20261121_1711-independent-review-issue-503f3e410c01.md` | Added this session changelog. | Remove only if superseded. |

# Contract / ABI changes

- None.

# State / schema changes

- None in game/build state. Coordination state changed only append-only issue text, readiness fields, and activity.

# Diagnostics

- Dev-loop status at ~17:03Z: lifecycle=game_build_queued, build=published_stale, run=20261121_144350, server-01 pid 7556 alive, client pids=[], game-build status=failed (revision 2 failed).
- Published build: generation 420, `build/game/generation-420/game-g420.dll`, sha256 `d70d49fd185ff12108e1f95c11c42f66e52c201f296d222d9cbba819c9e9a2f6`; its change report lists only `src/network/multiplayer-projectiles.cpp`.
- Gen420 proof-bearing run: `logs/2026-11-21/20261121_140624` (EXE artifact `2026-11-21T02:26:50.306117Z`, sha `8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871`, profile tdm-melee-validation, map dust2cyberiav4, AI-controlled client).
  - `collision.capsule-world.solve` reports `arcade_mode=true`.
  - 285 Mace + 36 Chainsaw `melee.contact.claim.accepted`; Mace 232 inactive at multiplier 1.0 (damage 1-5), 53 active at multiplier 10.0 (damage 250-2430; 27 exactly 250), 54 Mace kills.
  - 325 `server.damage.applied` attacker_type=npc -> target_actor_type=player, source=physical_contact.
  - `weapon.contact.result` attacker_type 2 -> target_type 1: 34 swordsword, 21 chainsaw, 1 mace.
  - 1940 `collision.arcade_pose_skipped`, skip_reason=arcade_capsule_world_owner; all sampled events have body_part_count=0 and extra_part_count=0.
- Mace parity join: 283/285 exact predicted==accepted; 51/51 joined active-pose claims exact. Two mismatches: contact_id 174 (client inactive predicted 4; server active elapsed 35, accepted 1756) and contact_id 344 (client inactive predicted 1; server active elapsed 30, accepted 250). Contact IDs are unique in the run, so the mismatches are not ID reuse.
- Human/FCS run `logs/2026-11-21/20261121_144350`: 254 `melee.contact.claim.rejected`, all `future_contact`, 0 accepted. contact_tick was constant 43683 while client local tick advanced ~112910-115571 and the validating server tick was 6545.
- Run 144350 journal identity: `client-01` contains writes from pid 11492 (process=client) and pid 29352 (process=server), while `run.json` pins server-01 pid 7556. The 254 future_contact events come from pid 29352.
- White active pose: only `melee.contact.fx` ("spawned client melee white sphere...") was found; that is impact FX, not the Mace viewmodel tint.
- Source cross-check: `src/physics/movement/actor-final-pose.cpp:466` skips final-pose root translation when arcadeCollision; `src/network/server-physical-contact-actors.cpp:495` skips player attackers in Arcade, `:558` applies world contact only when `!arcade`, `:614` skips non-damaging contacts in Arcade, `:714` gates weapon clash on `!arcade`.

# Validation

- Ran `agent-coord.py begin`, `sync`, `observe`, `issue show`, `issue readiness`, `proof status`, `proof list`, `dev-loop.py --status`, and read-only journal scans with `rg` and Python JSONL parsing.
- Joined client and server Mace claims by `contact_id` over the gen420 run journals.
- Confirmed the appended review and proposal in the issue documents.
- No compiler, build worker, or second dev-loop was invoked.

# Regression review

- No regression introduced; no source changed. This review found existing timing/identity defects already owned elsewhere (issue-adfeaafa1136 for future_contact tick epoch; issue-89efbbc86535 for screenshot proof).

# Human acceptance

- Still required: the Mace bright-white active pose and the human-input Arcade behavior. Journal evidence alone does not prove the viewmodel tint.
- The user-visible acceptance of "not stuck on weapon geometry" and "NPC melee damages me" still needs a clean human/scripted Arcade run.

# Agent handoff context

- Owner of issue-503 behavior: melee-arcade-fix.
- Do not redo: the gen420 journal counts, client/server parity join, source guard read, and readiness fields.
- First next step: run one pinned Arcade client/server scenario with human or scripted human-like input; require 0 `future_contact` rejects before judging Mace damage. If `future_contact` persists, route to issue-adfeaafa1136 (owner codex-melee-gold-fix) as the first failing stage.
- Second next step: for contact_ids 174/344, compare client `melee.client.action`/`melee.client.phase` with server action state at the contact tick.
- Third next step: capture the white pose with `screenshot_at_tick` (issue-89efbbc86535) or record it as explicit human visual acceptance.
- Process owner: issue-b396205aaecf now carries the proof auto-correlation proposal (target: pinned run end to proof record within 5 minutes).

## Captured identity

```json
{
  "reviewed_issue": "issue-503f3e410c01",
  "review_id": 52,
  "verdict": "inconclusive",
  "readiness_after": {"ready": true, "score": 8, "missing": []},
  "game_current": {"generation": 420, "dll_sha256": "d70d49fd185ff12108e1f95c11c42f66e52c201f296d222d9cbba819c9e9a2f6"},
  "best_runtime_run": {"run_id": "20261121_140624", "game_generation": 420, "arcade_mode": true},
  "human_input_run": {"run_id": "20261121_144350", "accepted_mace_contacts": 0, "future_contact_rejects": 254}
}
```
