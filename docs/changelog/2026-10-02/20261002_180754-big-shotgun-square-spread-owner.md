# Big Shotgun Square Spread Owner

Time UTC: 2026-10-02T18:07:54Z
Display timezone: America/New_York
Branch: afad20a-rebuild
Commit: working tree; no commit created

## Result

Status: PASS_WITH_HUMAN_REVIEW

Gave the Big Shotgun a true square pellet pattern owned by a first-class
`WeaponDefinition.squareSpread` field instead of the previous
`custom_params.squareSpread` lookup path, which the user reported did not work.
The pattern is always a square for any `pellet_count`; `spread` remains the full
angular side length around the aim direction.

The previous implementation produced a `cols x rows` grid whose height collapsed
when the pellet count was not a perfect square (e.g. 15 pellets -> 4x4 grid with
only 3 occupied rows). The grid now uses a single square side and distributes
pellets across every row, so 15 pellets occupy a 4x4 square extent and 100
pellets a 10x10 square extent.

## Root cause of "that path doesn't work"

The square flag was read only from `custom_params.squareSpread`
(`weapon-execution.cpp:162` and `weapon-fire-hit.cpp:347`). `custom_params`
values are applied during weapon registration, but the `ActorPresetWeapons`
overlay rebuilds active definitions from `WeaponRegistry::all()` and only copies
`custom_params` by reference; the flag also depended on a numeric-coercion
lookup. Moving the flag to a real `WeaponDefinition` field parsed directly from
JSON removes that fragility and makes it hot-reloadable through the normal
weapon-config path.

## Code changes

- `src/combat/weapon-types.h`: added `bool WeaponDefinition::squareSpread`.
- `src/combat/weapon-data.cpp`: `createBigShotgunDefinition()` sets
  `squareSpread = true` (JSON-free fallback).
- `src/combat/weapon-json-config.cpp`: parses top-level `square_spread`.
- `src/combat/weapon-execution.cpp`: shared server hitscan generator now reads
  `def.squareSpread`.
- `src/combat/weapon-fire-hit.cpp`: local prediction reads `def.squareSpread`.
- `src/combat/pellet-pattern.cpp`: square grid uses one square side and spreads
  pellets across all rows so the shape is always square.
- `src/gamemode/match-roles.h` / `.cpp` and
  `src/combat/actor-preset-weapons.cpp`: actor presets may override
  `hitscan.square_spread`.
- `config/weapons.json`: `big_shotgun` gains `"square_spread": true`; the
  working-tree `spread` stays `10.0` and the redundant
  `custom_params.squareSpread` was removed.

## Validation

- PASS: `config/weapons.json` parses; `big_shotgun.square_spread=true`,
  `spread=10`, `pellet_count=100`.
- PASS: Python replica of `generatePelletDirections` confirmed symmetric square
  extents for pellet counts 1..100 (e.g. 15 -> 4x4, 100 -> 10x10, full +/- side).
- PASS: dev-loop `build_status=success`, `latest_successful_build=877`,
  `latest_successful_source_generation=125` (matches the edited source
  generation). The running EXE was not killed or replaced.

## Runtime and human review

Build success is separate from runtime proof. In-game confirmation is still
needed for: selecting Big Shotgun, observing the 100-pellet square pattern
(now spanning `spread: 10.0`), and confirming the normal shotgun's random spread
is unchanged. The user must run their rebuilt EXE.

## Pre-existing work

All unrelated worktree edits (ragdoll, simulate-tick, analytics, docs, prior
changelogs, and the pre-existing `big_shotgun` `spread` change to `10.0`) were
preserved and not attributed to this session.
