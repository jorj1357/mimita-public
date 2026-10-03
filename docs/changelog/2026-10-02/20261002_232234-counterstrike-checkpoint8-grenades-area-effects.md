# Counter-Strike Checkpoint 8 — grenades and area effects

Date: 2026-10-02
EST timestamp: 2026-10-02 23:22:34 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and pure area-effect/grenade-rule evidence are proven. Live
throw/rendering and in-game fire/smoke/dark-bang behavior are NOT verified and
remain required.

## Scope

Checkpoint 8 of `docs/specs/20261002plan.md`: grenade tool policies and generic
area effects (Stage 13).

## Pre-existing / external edits (not mine)

`config/weapons.json` shows a pre-existing `beam_thickness` 2.0 -> 0.01 change
from an external writer (flagged since Checkpoint 3). Not modified by this
session. Other runtime-written user settings unchanged.

## Files changed

### `src/combat/area-effect.h` / `.cpp` (new)

- `AreaEffectKind` (None/Fire/Smoke/DarkBang) and `AreaEffect` (id, kind,
  owner, team, position, radius, height, duration, damage cadence, team policy).
- `tickAreaEffects` (fixed-tick; appends `AreaEffectDamage`; removes expired),
  `areaEffectContains`, `areaEffectSelfTest`.

### `src/combat/grenade-registry.h` / `.cpp` (new)

- `GrenadeDefinition` (id, weapon id, area kind, radius/height/duration, damage
  cadence, team policy, spawnsAreaEffect).
- `GrenadeRegistry` loading `config/grenades.json` with hot reload;
  `grenadeRegistrySelfTest`.

### `config/grenades.json` (new)

frag (direct explosion, no area), smoke (15s volume), darkbang (2.5s), fire
(4 m x 3 m cylinder, 10 damage every 10 ticks to enemies, 7s).

### `src/network/server-gamemode.h` / `.cpp`

- `ServerGamemodeState` gained `areaEffects`, `nextAreaEffectId`,
  `areaEffectSpawnCounter`.
- Public API: `serverSpawnAreaEffect`, `serverSpawnGrenadeAreaEffect`.
- `serverAreaEffectTick`: fixed-tick; applies fire damage through the shared
  damage path (NPC health mirror + `applyServerDamage` for players); emits
  `area_effect.spawn`.
- Wired into `serverGamemodeTick` right after `updateActorStates`.
- Relocated the area-effect functions out of the anonymous namespace so their
  definitions match the header's `MimitaNet` declarations.

### `src/terminal/debug-commands.cpp`

Added `grenade_spawn <frag|smoke|darkbang|fire>`.

### `src/game/game-cli.cpp`

Added `--grenade-selftest` and `--area-effect-selftest` (plus includes).

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 8.

## Reasoning

Area effects are a generic server-authoritative primitive so all grenade types
(and future area tools) share one tested implementation and one damage path.
Grenade policy lives in `config/grenades.json`, leaving `weapons.json` base
values untouched. Frag reuses the existing projectile splash explosion.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stage 13; Checkpoint 8).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.
- Skill: `docs/skills/terminal-command-checker-v1.md` — `grenade_spawn`
  registered with usage/description/category and clear failure messages.

## Validation

Build: `BUILD SUCCESS`.

Runtime (`mimita.exe`):

```text
[GRENADE SELFTEST] loaded=yes defs=4 PASS
[AREA EFFECT SELFTEST] fire_cadence=ok expiry=ok PASS
[MAP CONFIG SELFTEST] PASS
[OBJECTIVE SELFTEST] PASS
[NPC UTILITY SELFTEST] PASS
[NPC NAV REQUEST SELFTEST] PASS
[NPC PERCEPTION SELFTEST] PASS
[GAMEMODE SELFTEST] PASS
[ACTOR PRESET SELFTEST] PASS
[CS ROUND SELFTEST] PASS
```

## Known limitations / follow-ups

- Throwing is not yet per-grenade: all four map to `grenade_launcher`, and the
  projectile explosion does not yet call `serverSpawnGrenadeAreaEffect` (no
  grenade id flows on the projectile). The types are spawnable via the API and
  `grenade_spawn`, and fully tested; throw-time selection needs per-grenade
  weapon entries or a grenade id on the projectile.
- Client rendering of the smoke volume / fire cylinder is deferred; the server
  owns the effect and outcomes.
- Grenade presentation (no damage numbers/hit markers) is inherited from the
  Counter-Strike actor preset applied in Checkpoint 3.

## Human review still needed

- In a live session confirm a frag damages authoritatively with no damage
  numbers/hit markers, smoke has a visible volume and timer, fire damages
  roughly every 10 ticks, and dark-bang produces the Counter-Strike screen
  effect.
- Confirm `grenade_spawn fire/smoke/darkbang` creates effects at the player
  position and that fire only damages enemies.

## Explicitly not done yet

Throw-time grenade selection, client area-effect rendering, TeamBrain and
advanced NPC objective play (Checkpoint 9), and debug tooling/acceptance
(Checkpoint 10).
