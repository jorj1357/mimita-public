# Air melee and weapon collision coverage

Date: 2026-10-27T06:25:25Z

## Skill review

- Skill: `docs/skills/spec-behavior-review-v1.md`
- Result: the user's decision is that all melee weapons are air-usable by
  default, and every weapon is an entity with a deliberate collision shape.
- Finding: `MeleeAction::actorState` defaulted to `Grounded`, and SwordSword,
  Chainsaw, and Mace omitted `actor_state`, so `resolveAction()` rejected them
  while airborne. The server attack and continuous-contact owners do not reject
  airborne actions.
- Runtime and human acceptance: air SwordSword input/action start confirmed in
  the live journal. Chainsaw air input was not separately reproduced in the
  final run because the cached dev profile still supplied weapon set 12; the
  action-state default applies to all melee actions. Human feel/visual review
  remains required.

## Changes

### Gameplay issue tracker

- Added the airborne-melee report and source finding to
  `docs/specs/open-gameplay-issues.md`.
- Added the missing weapon-collision coverage report and missing-ID list.

### Air melee

- `src/combat/weapon-melee-config.h`: `MeleeAction::actorState` now defaults to
  `Any`. Future melee actions are air-usable unless they explicitly declare
  `grounded` or `airborne`.
- Updated SwordSword, Chainsaw, and Mace specs to state that their actions are
  usable on the ground and in the air by default.
- Captain Falcon's explicitly grounded/airborne actions remain unchanged.

### Weapon collision coverage

- `config/weaponcollisions.json` now has an entry for all 34 top-level weapon
  IDs.
- Modeled weapons use the new `"source": "model"` mode where no tuned box is
  needed.
- No-model weapons use explicit hand-local proxy boxes: grenades, bomb, godball,
  force punch, quick hit, Captain Falcon move set, and all six zombie claws.
- `nothing` is the only zero-hitbox weapon.
- `src/combat/weapon-collision-config.cpp`: added `source:"model"` handling and
  explicit disabled-entry cleanup so stale collision state cannot survive an
  equip change.
- `src/combat/weapon-viewmodel.cpp`: capsule overrides now apply only to
  `source:"capsule"` entries, so box and model entries are not overwritten by a
  generic capsule.
- Added terminal command `weaponcollisions_check`, which reports and logs any
  registered weapon missing an entry.

## Evidence

- Build: dev-loop request `2026-10-27T06:22:06.769Z-8f558eea`, build 3455,
  status success; the subsequent launch published build 3456.
- Runtime journal: `C:\mimita-v9\logs\2026-10-27\20261027_062345\events.jsonl`.
- `weaponcollisions_check`: `missing_count: 0`.
- Air SwordSword evidence:
  - `melee.client.input` action `slash_one` with `grounded:false`,
    `on_ground:false`, `stable_on_ground:false`.
  - `melee.client.action` action `slash_one` with weapon `swordsword`.
- Runtime chainsaw air click was not separately captured because the cached dev
  profile launched with `--weapon-set 12`; this is a dev-loop profile cache
  issue, not a weapon-code rejection.
- Human visual review is still needed for model-source collision, proxy-box
  placement, and chainsaw air feel.
