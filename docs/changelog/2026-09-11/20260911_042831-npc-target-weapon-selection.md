// 2026-09-11T04:28:31Z
/* purpose
* record behavior-profile influence on NPC target selection and weapon selection
* preserve exact source, config, build, and headless runtime evidence
* this file does NOT claim emotion/panic, memory, stealth, or HUD work
* this file does NOT replace the append-only regression record
*/

# NPC behavior-driven target and weapon selection

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-11T04:28:31Z`
- Display timezone: America/New_York
- Display time: `2026-09-11 00:28:31 EDT`
- Pre-existing changes: the behavior-profile slice (aim/reaction/cadence/
  aggression/range) and all prior actor/role/movement/navigation slices were
  preserved. This session only extended the same profile into target and weapon
  selection.
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and other
  `docs/changelog/` files were left untouched.

## Files changed

- `src/npc/npc-behavior.h` / `npc-behavior.cpp`: nine new profile fields and
  their parsing/resolution defaults.
- `config/behavior-profiles.json`: target/weapon weights for balanced,
  aggressive, nervous.
- `src/npc/npc.h`: `Npc::serverTargetId` (current target, persisted per life).
- `src/network/server-gamemode.cpp`, `src/network/server-npcs.cpp`: reset the
  target id on gamemode spawn/respawn.
- `src/network/server-npcs.cpp`: profile-scored target selection with
  stickiness/threshold; legacy nearest path preserved when no profile.
- `src/npc/npc.cpp`: profile-scored weapon selection (metadata + weights) with a
  switch threshold; legacy distance switching preserved when no profile.
- `src/npc/npc-internal.h`, `src/npc/npc-spawn.cpp`: `weaponSelectionRangeOf`
  (explicit range, else projectile travel, else hitscan falloff, else melee).

## New behavior fields

Target selection (JSON keys): `target_stickiness`, `target_switch_threshold`,
`low_health_target_bias`, `threat_bias`, `distance_target_bias`.
Weapon selection: `weapon_range_bias`, `weapon_damage_bias`,
`weapon_safety_bias`, `weapon_switch_threshold`.
Missing/negative values keep neutral defaults (`distance_target_bias` and
`weapon_range_bias` default 1.0, stickiness/thresholds/biases default 0.0), so
an unconfigured profile reproduces nearest-target and range-fit selection.

## Target scoring

Server-side, per NPC, over live hostile players and NPCs:
```text
score = distanceScore * distanceTargetBias
      + vulnerabilityScore * lowHealthTargetBias
      + threatScore * threatBias
      + (candidate == currentTarget ? targetStickiness : 0)
```
- `distanceScore = 1/(1+planarDistance)`.
- `vulnerabilityScore = 1 - hp/maxHp` (players use `ServerPlayer::maxHealth`).
- `threatScore = clamp(equippedWeaponDamage/50, 0, 1)` for NPCs (0.5 for
  players, who expose no cheap weapon-damage value).
Switch rule: keep the current target unless a new candidate exceeds it by
`targetSwitchThreshold`. Current target is refound by id; dead targets fall
through. `Npc::serverTargetId` is the persisted identity.

## Weapon scoring

In the existing weapon-switch owner, when a profile is active:
```text
weaponScore = rangeFit * weaponRangeBias
            + damageUtility * weaponDamageBias
            + safetyUtility * weaponSafetyBias
```
- `rangeFit = 1 - |selectionRange - targetDistance| / max(selectionRange, 1)`.
- `damageUtility = clamp(damage * pelletCount / 80, 0, 1)` (burst damage).
- `safetyUtility = clamp(selectionRange/80, 0, 1)`, halved for
  projectile/explosive weapons, 0.05 for melee.
Switch only when the best candidate beats the current weapon by
`weaponSwitchThreshold`. Ammo/runtime availability, loadout legality, the
`forceWeapon` override, and `weaponSwitchCooldown` stay authoritative.

## Fallback behavior

No behavior profile (`behavior.active == false`): the original nearest-hostile
target scan and the original distance-bucket weapon switching run unchanged.
Unknown/missing fields use neutral defaults. No profile-name branches exist.

## Measurable differences

Controlled run: three roles on opposing teams, identical movement
(`retrograd_fast`) and loadout (`standard`), differing only by behavior profile
(TDM, funworld3, 12 NPCs). From the always-on NPC log:

| profile | target switches | mean target life | mean target dist | mean engage dist | weapon switches |
|---|---|---|---|---|---|
| aggressive | 37 | 2.66 s | 17.87 m | 23.91 m | shotgun 18, revolver 0 |
| balanced | 76 | 1.60 s | 7.86 m | 11.72 m | revolver 8, shotgun 35 |
| nervous | 233 | 0.51 s | 11.60 m | 13.17 m | revolver 12, shotgun 15 |
| default (no role) | 109 | 1.69 s | 45.64 m | 45.17 m | revolver 10, rocket 17, shotgun 16 |

Aggressive commits ~7x longer than nervous and never picks the revolver;
nervous changes targets most and favors the longer-range revolver; the default
row shows the unchanged legacy nearest/bucket behavior. All differences come
from numeric weights.

## Regressions

- FFA (10 NPCs): `Match ACTIVE mode=ffa`, 33 kills.
- Elimination (10 NPCs): match ended
  (`[PERSISTENCE] Match result emitted: mode=elimination winner=blue`).
- TDM (30 NPCs): 196 kills; completed 40 s in 42.2 s wall time (no stall).
- Role health/loadout/movement logs present (`ROLE SPAWN`, `ROLE MOVEMENT`).
- Behavior aim/reaction/cadence paths were not modified this slice.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`, `docs/specs/weapons/weapons.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS; selection is numeric and
  data-driven, existing owners reused, no profile-name branches.
- `docs/skills/efficiency-checker-v1.md`: PASS; O(candidates) per NPC per tick,
  no per-tick JSON or allocations; weapon scoring only runs when a profile is
  active and the switch cooldown allows.
- `docs/skills/logging-checker-v1.md`: PASS; `npc-target`/`npc-weapon` log only
  on change in the always-on NPC log.

## Smallest logical next phase

Add a per-profile `target_switch_chance`-style stochastic commitment (or a
threat-based engagement priority) only if playtesting shows deterministic
scoring is too predictable — otherwise move to reading these profiles into the
HUD role readout so the behavior is observable in-game.
