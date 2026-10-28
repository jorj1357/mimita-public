# Melee scenario follow-up intake

- Time: 2026-10-28 14:14:34 EDT (2026-10-28T18:14:34Z)
- Branch: `20261011zt`
- HEAD: `933cfb48`
- Result: `PASS_WITH_HUMAN_REVIEW` — supplied scenario and additional design feedback recorded; behavior remains unverified.

## Scenario inspection

Read `devscripts/scenarios/user/scenario-20261028_181131-meleeissue.jsonl`.
It contains 3,452 tick records plus a header and footer. The header identifies
Sandbox on `assets/maps/tdmtest3.glb`, starts with mace in slot 26, and records
the initial tick and pose. Across the input recording, mace and SwordSword are
equipped; chainsaw is not. This is local input/movement recording, not a world
replay: it contains no target health, authoritative contact outcomes, or
weapon-versus-weapon damage/order records. The user's observed corner snagging
and SwordSword/mace pass-through report is therefore recorded as a reproduction
lead, without claiming that the file alone proves the failure.

## Repository change

Updated the existing “SwordSword and mace physical hits fail inconsistently in
Sandbox” entry in `docs/specs/open-gameplay-issues.md` (near line 1655) to add:

- scenario path, map/mode, starting mace slot, tested weapons, and the fact that
  chainsaw was not used;
- corner snagging as a separate collision acceptance case;
- the user's observation that weapon-versus-weapon contacts appear to work,
  alongside the concern that they resolve before damage;
- proposed more client-responsive melee hit presentation/damage prediction,
  with visible weapon pass-through/contact, and a requirement to define
  authority, confirmation/reconciliation, and ordering before implementation.

No code, weapon configuration, or regression record was changed. Existing
uncommitted edits were preserved.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`,
  `docs/specs/weapons/melee/melee-weapons.md`, and
  `docs/workflows/runtime-scenario-validation.md`.
- `git diff --check` passed for the issue tracker update.
- No build or runtime replay was run. The recording depends on the same map,
  mode, weapon set, and dev server, and does not include target state. Human
  gameplay review and a live journal correlating weapon shape, contact,
  damage, and weapon-versus-weapon ordering are still needed.

## Pre-existing workspace changes

Preserved the pre-existing edits to `config/accounts/default.json`,
`config/collision.json`, `config/weaponcollisions.json`, and
`config/weapons/mace/config.json`; untracked assets under
`assets/objects/things/effects-objects/` and
`assets/objects/weapons/weapon-objects/`, plus
`assets/textureshq/explosionmarkv1.png` and
`assets/textureshq/rocketsmokev1.png`; deletion of
`docs/specs/open-gameplay-issues-human.md`; and the earlier session's issue
tracker update and changelog.
