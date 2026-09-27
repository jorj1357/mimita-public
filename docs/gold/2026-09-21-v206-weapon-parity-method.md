# 2026-09-21 — v2.0.6 weapon parity: method and reference

Status: `REFERENCE` — how to capture a v2.0.6 weapon reference and reuse it for
every weapon. Companion to the movement oracle
(`src/physics/movement/reference/movement-v206-reference.*`) and its harness.

## Why

Weapons must feel like v2.0.6 while their logic stays hot-reloadable and
selectable (`config/weapons.json` / hot tool definitions, `behaviorSource`
`json` vs `cpp`). Judging "v2.0.6 feel" by eye is unreliable, so capture a frozen
numeric reference first, then port the hot behavior and compare tick by tick.

## What actually existed at v2.0.6

`git ls-tree -r v2.0.6 -- src/combat` shows only these weapon factories in
`weapon-data.cpp`:

| id | behavior | damage | fireDelay | reload | mag | pellets | spread | recoil |
|---|---|---|---|---|---|---|---|---|
| revolver | Hitscan | 50 | 0.08 | 1.0 | 6 | 1 | 0.0 | 99 |
| shotgun | Hitscan | 12 | 0.25 | 1.5 | 2 | 15 | 3.0 | 130 |
| godball | Godball | 10 | 0.0 | 0.0 | 0 | 1 | 0.0 | 0 |
| swordsword | Swordsword | 35 | 0.3 | 0.0 | 0 | 1 | 0.0 | 10 |
| op_revolver | (revolver copy) | 50 | 0.01 | 1.0 | 999 | 1 | 0.0 | 99 |
| aa12 | (shotgun copy) | 12 | 0.1 | 1.5 | 999 | 15 | 3.0 | 130 |

Extraction command used:

```powershell
git show v2.0.6:src/combat/weapon-data.cpp | Select-String `
  -Pattern "\.damage|\.fireDelay|\.pelletCount|\.spread|\.recoil|\.magazineSize|\.reloadTime|def\.id"
```

**Important:** v2.0.6 had no rocket launcher and no spy knife, and no victim /
shooter knockback fields (those are later additions). So:

- **revolver, shotgun**: exact v2.0.6 parity is defined by the table above plus
  the v2.0.6 `weapon-fire.cpp` hitscan pipeline.
- **rocket launcher, spy knife**: no v2.0.6 source exists. Their parity target is
  the **current cold authoritative path** (`weapon-system.cpp`,
  `weapon-fire-*.cpp`, `server-attack.cpp`, `weapon-rocket-launcher.cpp`,
  `weapon-spyknife.cpp`), captured the same way before either is ported hot.

## Method (reusable for every weapon)

1. **Freeze the reference numbers.** Extract the weapon's `WeaponDefinition`
   fields from the chosen source (v2.0.6 tag for revolver/shotgun, current cold
   path for later weapons).
2. **Freeze the reference behavior.** Reconstruct the fire pipeline from the
   same source (`weapon-fire.cpp` at the tag; `weapon-fire-*.cpp` today) into a
   small frozen reference function that is NOT a gameplay owner, following the
   movement oracle pattern.
3. **Define identical fixed-tick scenarios.** For each weapon: one shot at a
   wall at a fixed distance, one shot at an actor at a fixed distance, point
   blank, max range, a moving target, and a reload cycle. Same origin, direction,
   tick rate (60 Hz), and world for both paths.
4. **Run both paths** (frozen reference and the current hot/cold path) on the
   same scenario.
5. **Compare and log the first differing tick** for: damage applied, victim
   knockback impulse, shooter recoil impulse, ammo, cooldown, reload timing, and
   projectile position/velocity (for projectile weapons).
6. **Port the hot behavior** until every field matches, then set
   `TOOL_FLAG_OWNS_EXECUTION` on the recipe and verify the flag flip keeps parity.
7. **Reuse** the same scenario set for the remaining weapons.

## Server authority: what the flip actually replaces

`tool.primary-use` is dispatched from the **server** attack path
(`server-attack.cpp`, `server-npcs.cpp`), not the client fire. Flipping
`TOOL_FLAG_OWNS_EXECUTION` therefore makes the server resolve hitscan through the
hot behavior instead of the cold `traceHitscan`. The hot behavior must
replicate, tick-for-tick:

- `buildPelletDirections` / `generatePelletDirections` (pellet count + spread +
  deterministic seed) — `weapon-execution.cpp:146`, `pellet-pattern.*`
- target ray test per pellet (`rayPlayerTarget`, AABB/capsule) —
  `weapon-execution.cpp:164`
- closest-pellet + world-block selection (`traceHitscan`, `:224-298`)
- damage model: `computeHitscanDamage` = damage × part multiplier × falloff ×
  angle, with `hitscanPartMultiplier` (head = headshotMultiplier, legs =
  limbDamageMultiplier, else 1) and `hitscanFalloffFactor` (params
  `distanceFalloffStart`, `minDamageFraction`, `falloffExponent`) —
  `weapon-execution.cpp:110-139`
- knockback aggregation `direction * damage * knockbackPerDamage` and the
  per-target spawn-generation identity.

Until those are ported and matched against the harness, the flag stays off. This
is why "flip hitscan/melee now" is not a one-line change: it is a server-authority
port, not a presentation change. The hot side already has the capability surface
needed (`findEntities`, `readComponent`, `queryWorldRay`, `damage.apply` with a
`knockback[3]`, `physics.impulse` for shooter recoil).

## Hot ownership flip precondition

The router seam is already honest (`combat-policy.cpp::onToolUse` lets the
behavior decide `handled`). As of 2026-09-22 the hot `hitscanUse` is no longer
simplified: it builds the shared fixed pellet grid, scans targets, validates
against the same rewound per-part body boxes the cold trace uses (published by
the cold server through the `HitscanTargetBoxes` dynamic component;
`src/hot-reload/hot-hitscan-target.h`), and applies the shared v2.0.6 damage
model (`src/combat/hitscan-model.h`) with per-part multipliers, falloff, and
per-target knockback. Ammo/cooldown/reload are owned by the hot
`ToolInstanceStateV1`.

The remaining precondition was the **authoritative consequences**, not the trace.
It is now closed: `GAME_CAP_HITSCAN_RESOLVE` (`hitscan.resolve`) hands the hot
trace aggregates to the shared cold consequence pipeline
(`src/network/server-hitscan-outcome.cpp`, extracted verbatim from the cold
branch) — damage policy, `DamageConfirmedEventPacket`,
`broadcastNpcDamageEvent`, kill recording, and shot-visual broadcasts. Hot owns
the trace/behavior; the kernel owns the consequences, so a hot weapon cannot
silently drop them. The temporary `ControlSource` gate is removed, so player
hitscan is hot-owned with full parity.

## Reuse checklist for the next weapon

- [ ] copy the reference numbers for the weapon
- [ ] add its scenarios to the parity harness
- [ ] port the hot behavior (`modules/tools/<behavior>.cpp`)
- [ ] match every compared field at the first differing tick
- [ ] set `TOOL_FLAG_OWNS_EXECUTION`, re-run, confirm parity and no regressions
- [ ] record evidence in the changelog and the feature record
