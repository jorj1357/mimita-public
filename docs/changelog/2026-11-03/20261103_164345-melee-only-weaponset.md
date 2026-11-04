# Melee Only Weapon Set

- Local time: 2026-11-03 16:43:45 EST
- UTC: 2026-11-03T21:43:45Z
- Branch: `20261011zt`
- Commit: not recorded in this checkout
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Added a host-selectable `Melee Only` weapon set for any managed gamemode. It
contains Chainsaw, Mace, and Swordsword. When explicitly selected by the host,
the shared loadout applies to human and NPC actors, including role and actor
preset assignments, disaster loadouts, first Zombie Tower join, normal spawn,
and respawn. The existing mode and role loadout precedence remains in effect
when there is no explicit host selection.

## Files changed

- `config/weaponsets.json`, new set ID 32/key `melee_only`: added the set with
  `chainsaw`, `mace`, `swordsword`, plus the existing `nothing` utility entry.
- `src/network/server-gamemode.cpp`, `serverResolveActorSpawnProfile()` and
  `serverCommunityWeaponAllowed()`: explicit host selection now overrides role
  and actor-preset sets for every managed gamemode and is enforced in modes
  outside the former community-mode allowlist. The first Zombie Tower join
  also retains the explicit set.
- `src/network/server-players.cpp`, `resetPlayerForSpawn()`: a random disaster
  weapon cannot replace an explicitly selected host set; updated the inventory
  ownership comment to match resolution.
- `src/network/server-npcs.cpp`, NPC adoption and respawn: a random disaster
  weapon cannot replace an explicitly selected host set.
- `docs/specs/gamemodes/gamemodes.md`: documented shared explicit-set
  precedence for managed modes and actors.
- `docs/specs/open-gameplay-issues.md`: added OGI-021 for cross-mode actor
  verification.

## Previous and new behavior

Previously, `serverResolveActorSpawnProfile()` applied a shared host set over
actor roles only for TDM. In other managed modes, the role or preset could
still supply a different spawn inventory; random disaster selections could
also replace the selected inventory. Explicit sets are now resolved for all
managed modes and keep their authority through those spawn paths.

## Documents and review

- Read `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`,
  `docs/specs/weapons/weapons.md`, `docs/specs/actor-presets/actor-presets.md`,
  `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/teamdeathmatch.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/regressions/README.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/dev-loop/quality-gates.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- `docs/skills/spec-behavior-review-v1.md`: current issue intake did not define
  cross-mode host-selection precedence. Clarified the expected rule in the
  gamemodes spec before changing the resolver; no product decision remained
  open.
- `docs/skills/logging-checker-v1.md`: not read; no new diagnostic path added.
- `docs/skills/efficiency-checker-v1.md`: not read; no hot-loop work added.

## Validation and evidence

- Parsed `config/weaponsets.json` and confirmed set 32 contains the requested
  three weapons plus the standard `nothing` utility entry.
- `git diff --check` reported pre-existing whitespace in the user's dirty
  `ROADMAP.md`, `docs/specs/open-gameplay-issues.md`, and
  `docs/specs/vehicles/vehicles.md`; no new whitespace finding came from the
  added row or code/config changes.
- Canonical build request `2026-11-03T21:42:42.792Z-7976e1e7` completed
  successfully. Dev-loop status reported artifact
  `2026-11-03T21:43:16.891370Z`, executable
  `.dev/builds/2026-11-03T21-43-16.891370Z/mimita.exe`, SHA-256
  `ed3113433814b8c0d36979e08b2bf7a39417b2f75a7d800438e7f4b21c20ef59`, and
  current source generation 51.
- No tests were run. No fresh executable runtime scenario or human review was
  performed. The existing dev-loop server/client were active on the prior
  artifact; they were left running. OGI-021 remains `Verify` until human/NPC
  inventory, authorization, and respawn behavior are observed across modes.

## Pre-existing work

The checkout contained unrelated user changes before this task, including
changes in `src/network/server-npcs.cpp`, `src/network/server-players.cpp`,
`docs/specs/open-gameplay-issues.md`, build tooling, and gameplay documentation.
Those edits were preserved. Only the weapon-set and disaster-precedence hunks
in the two network files and the new OGI row belong to this work.
