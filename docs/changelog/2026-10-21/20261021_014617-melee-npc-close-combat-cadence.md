# Melee NPC close-combat behavior and cadence

- Status: `PASS_WITH_HUMAN_REVIEW`. The Melee NPC profile now favors close approach and quicker moves; the state scorer disallows orbit and retreat states for profiles with a preferred range at or below 2 m. Attack-gap input is clamped to 1–30 ticks. Build 2717 succeeded. No Melee runtime scenario was run, so visible chasing and confirmed hits still need in-game review.
- Time: 2026-10-21T01:46:17Z (2026-10-20 21:46:17 EDT, America/New_York).
- Branch: `20261011zt`; HEAD at start: `094e6e5a`.
- Pre-existing edits: `config/analytics.json` was modified and left untouched. `config/behavior-profiles.json` already contained user edits to the Melee gap, attack range, and equal move weights; the current gap value of 2 was preserved while relevant range and move-weight tuning was changed.

## Findings and changes

- `src/npc/npc-state-machine.cpp`, `scoreState`: the Melee profile previously had no preferred range, so it inherited ranged combat scoring. Circle, Strafe, and Retreat could all be selected. A preferred range of 1.5 m now marks it as close-combat, and these three states receive no score for that profile, leaving direct approach and attack states available.
- `config/behavior-profiles.json`, `melee_aggressive`: the prior user value `"melee_attack_range_meters": 0.1` was clamped by the profile reader to 0.5 m, making attacks start only at point-blank distance. It is now 4.5 m. Added `"preferred_range": 1.5`. The existing gap of 2 is retained. Move weights now favor `a_ground_punch` (10) and `a_uptilt` / `a_air_up_kick` (6), while `a_air_kick` and specials remain 1.
- `src/npc/npc-behavior.cpp`, `readProfile`: `melee_attack_gap_ticks` previously accepted 0–3600. It now clamps to 1–30.
- `docs/specs/gamemodes/supersmashbrosmelee.md`, section 34: records close-combat profile behavior and explicitly defines gap ticks as the pause after an action ends. The action's authored duration is additional, so 1–30 is not a start-to-start maximum. This avoids promising an attack start every 30 ticks while moves still play without interruption.

## Exact source/config edits

- `src/npc/npc-behavior.cpp`, `readProfile` (line 77): before: `j["melee_attack_gap_ticks"].get<int>(), 0, 3600);`; after: `j["melee_attack_gap_ticks"].get<int>(), 1, 30);`.
- `src/npc/npc-state-machine.cpp`, `scoreState` (lines 40–48): there was no close-combat state filter. Added: `const bool closeCombatProfile = npc.behavior.active && npc.behavior.preferredRange > 0.0f && npc.behavior.preferredRange <= 2.0f;` followed by a zero-score rule for Circle, Strafe, and Retreat.
- `config/behavior-profiles.json`, `melee_aggressive` (lines 668–677): before the user-edited block had no `preferred_range`, attack range `0.1`, and equal weights of `1.0`; after it sets preferred range `1.5`, attack range `4.5`, and weights 10/1/6/6/1/1 in the action-ID order recorded above. The prior gap `2` was preserved.
- `docs/specs/gamemodes/supersmashbrosmelee.md`, section 34 (line 237): replaced the previous one-tick/unbounded-gap wording with the validated 1–30 clamp, post-action wait semantics, 1.5 m preferred range, and 4.5 m attack-start range.
- This changelog is new; there was no prior text.

## Documents and review

- Read the repository instructions and `docs/ROUTER.md`; reviewed the Melee and NPC behavior specifications, NPC movement architecture, JSON configuration architecture, runtime scenario workflow, `docs/skills/spec-behavior-review-v1.md`, build and task-completion operations, and related Melee regressions. Router references `docs/regressions/regressions-v1.md`, but only an archived copy exists in this checkout; it was not treated as current guidance.
- Behavior review: `PASS_WITH_HUMAN_REVIEW`. The live report is consistent with current code: retreat/circle/strafe selection and point-blank attack range can explain an NPC approaching then moving away or failing to start an attack. The exact failure was not observed in a fresh Melee runtime run.
- No tests were added or run.

## Validation and remaining evidence

- `git diff --check`: passed; Git emitted only a line-ending conversion warning for `src/npc/npc-state-machine.cpp`.
- Canonical dev-loop build request `2026-10-21T01:45:53.138Z-bad1a6fc`: success, build 2717, 2 translation units compiled, 562 skipped, artifact published at `.dev/builds/2717`.
- The dev-loop's already-running visible session remains on build 2716 in Juggernaut on `dust2cyberiav4`; auto-restart is off. It was left untouched. No new `--versioninfo` identity or runtime journal was created for build 2717, and no claim is made that the new behavior is live or human-verified.
- Next acceptance: launch a Melee session on build 2717, confirm the role resolves `melee_aggressive`, and inspect `melee.action.start` records for range, gap, and chosen move alongside movement-state and confirmed-hit records. Confirm the NPC consistently closes distance and lands attacks on the player.
