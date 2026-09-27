# Hot tools: generic behavior-id registry, action events, instance state, all weapons migrated

- EST timestamp: 2026-09-17 22:45:00 -04:00
- UTC timestamp: 2026-09-18T02:45:00Z
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Why

The weapon system was still selected by a per-tool key and cold
`WeaponBehaviorType` branches, definitions had no shared behavior identity, tool
actions had no generic event contract, and per-instance ammo/reload still lived
in the cold `WeaponRuntime` map. This workstream (phases 1-5 of the Tool/Weapon
plan) makes every current weapon a recipe on one hot registry that names a
shared behavior, emits animation-neutral action facts, and owns per-instance
state on the tool entity. It is hot-only: no EXE rebuild is required and every
future tool is a new recipe.

## Changes (all hot / DLL-side)

- `src/hot-reload/hot-tool-visual.h`
  - `ToolDefinitionV1` gained `behaviorId`, `animationSetId`, `effectSetId`,
    `networkPolicy`, `collisionPolicy`, `toolFlags` (hot-only; does not cross the
    ABI).
  - Added shared behavior-family constants (`TOOL_BEHAVIOR_HITSCAN/PELLET/MELEE/
    CONTACT/ROCKET/GRENADE/THROWN`), network/collision policy constants, and
    `findToolDefinition(toolKey)`.
- `src/hot-reload/hot-tool-action.h` (new)
  - `ToolActionEventV1` and the 13 generic action ids (Equipped..MeleeContact).
  - `emitToolAction` dispatches a `tool.action` fact and writes a
    `WEAPONS`-category record through the same `log.event` capability the
    collision package uses; `toolLogEvent`/`toolLog` are the shared helpers.
- `src/hot-reload/hot-tool-state.h` (new)
  - `ToolInstanceStateV1` dynamic component (ammo/reserve/cooldown/reload) lives
    on the tool entity; `toolStateEnsure`, `toolStateAdvance`, `toolStateConsume`,
    and the one canonical `toolStateTryStartReload`/`toolStateCompleteReload`
    ammo transition.
- `src/hot-reload/hot-package.h`
  - `addBehavior`/`findBehavior` + `BehaviorIdRegistrar` register one function
    for many tools.
- `src/hot-reload/modules/tools/combat-policy.cpp`
  - Router resolves by `behaviorId` (direct key, then recipe definition, with
    `findProjectileVisual` for numeric 5/7 keys, then legacy per-tool). Logs the
    route decision (`tool.route`) and the cold fallback.
- `src/hot-reload/modules/tools/tool-action-bridge.cpp` (new)
  - Subscribes to `tool.action` and updates `HotActorActionState` (shoot/reload/
    melee/equip flags). This is the only animation boundary; behaviors never
    touch animation.
- `src/hot-reload/modules/tools/tool-state-system.cpp` (new)
  - Fixed-tick system (`gameplay.60`) that counts cooldowns down and completes
    reloads through `toolStateCompleteReload`, emitting ReloadFinished. Registers
    the `ToolInstanceState` schema (runtime-only). Makes phase-4 reload actually
    finish for hot-owned tools.
- `src/hot-reload/modules/tools/hitscan-tool.cpp`
  - Registers `TOOL_BEHAVIOR_HITSCAN` and `TOOL_BEHAVIOR_PELLET`, reads live
    `hotRange`/`hotDamage` params, emits PrimaryAccepted/DryFire/Hit/Fired, and
    owns ammo/cooldown/reload via `ToolInstanceStateV1`.
- `src/hot-reload/modules/tools/melee-tool.cpp`
  - Registers `TOOL_BEHAVIOR_MELEE`, emits PrimaryAccepted + MeleeContact, reads
    live range/damage, logs hit/miss.
- `src/hot-reload/modules/tools/rocket-tool.cpp`, `grenade-tool.cpp`
  - Register `TOOL_BEHAVIOR_ROCKET`/`TOOL_BEHAVIOR_GRENADE`, read live projectile
    params (speed/gravity/lifetime/radius/damage/splash), emit action facts,
    log `tool.rocket`/`tool.grenade`, set per-instance cooldown.
- `src/hot-reload/modules/tools/thrown-grenade-tool.cpp` (new)
  - `TOOL_BEHAVIOR_THROWN` for smoke/frag/fire; projectile type key is the
    tool's own hash.
- `src/hot-reload/modules/tools/physical-contact-tool.cpp` (new)
  - `TOOL_BEHAVIOR_CONTACT` for Godball-like always-active contact tools.
- `src/hot-reload/modules/presentation/tool-visuals.cpp`
  - `setBehavior` helper wires behavior/animation/effect/policy ids.
  - Added recipes for every remaining built-in and JSON weapon: `godball`,
    `op_revolver`, `aa12`, `admin_revolver`, `hafs`, `quick_hit`,
    `grenade_smoke`, `grenade_frag`, `grenade_fire`, `ak`, `sniper`. Values were
    copied verbatim from `config/weapons.json` (no tuning). `ak` and `sniper`
    are JSON-only, so they now register as brand-new hot tools via
    `registerHotTools` with no cold edit.
- `src/hot-reload/hot-modules.json`
  - Added `hot-tool-action.h` and `hot-tool-state.h` to tracked headers.

## Phase 0 — generic hot-first execution gate (one cold build)

- `src/network/server-attack.cpp`
  - The early `dispatchToolUse` gate now runs for EVERY execution family and is
    populated correctly: `userEntity` (player entity), `toolEntity` (equipped
    tool entity), ownerId, `toolId = gameHash(def->id)` (recipe/behavior key),
    `toolNetworkId`, origin, direction, predictionKey, tick, and ammoCost.
  - When hot owns the use it now reports `accepted = true` (the behavior already
    applied the authoritative consequence), refreshes the legacy state view, and
    rate-limits through the shared cooldown.
  - Removed the duplicate, later projectile hot-dispatch block (one dispatch
    site). Non-migrated projectile tools are rejected explicitly rather than
    reviving the retired kernel-container spawn.
- `src/hot-reload/hot-tool-visual.h`
  - `TOOL_FLAG_OWNS_EXECUTION` (toolFlags bit0): a definition must opt in before
    hot code owns its execution. Safe default for a new recipe is off.
  - `findToolVisualByNetworkId()` maps a numeric NETWORK_WEAPON_* family id to a
    recipe (hot data), so the router can resolve behavior/flag for numeric keys.
- `src/hot-reload/modules/presentation/tool-visuals.cpp`
  - `setOwnsExecution()` marks migrated definitions; every migrated recipe opts
    in.
- `src/hot-reload/modules/tools/combat-policy.cpp`
  - Router declines (leaves `handled = 0`, so the cold path runs) when a
    definition has a behavior but has not opted into hot execution.
- `src/hot-reload/modules/tools/hitscan-tool.cpp`, `melee-tool.cpp`,
  `physical-contact-tool.cpp`
  - These now CLAIM the use only when they will actually act. No relationship
    target, out of range, or a wall-blocked shot leaves the use unclaimed, so
    the cold attack path stays authoritative for players. NPCs (which have the
    relationship target) go hot. One behavior serves both without a weapon
    switch.

## Evidence

- Hot build: `python devscripts/live-build.py` -> `Status success`,
  `build/hotreload/mimita-live-g000010.dll` (never writes `mimita.exe`). The
  generation manifest lists all new tool sources and both new headers.
- Cold build: `python build_agent.py` -> `BUILD SUCCESS`,
  `mimita-20260917T224644.exe`. The new strings `[ATTACK HOT ACCEPT]` and
  `cold-projectile-unsupported` are present in the EXE, proving
  `server-attack.cpp` was recompiled.
- `mimita-20260917T224644.exe --hot-combat-selftest`: 195 ok / 24 FAIL, stable
  across repeated runs. The 24 FAILs are identical to the pre-change `g000005`
  DLL (pre-existing animation policy/phase2 failures). No tool check regressed;
  all tool checks pass (`tool.definition provider resolves`, revolver/swordsword
  definitions, rocket and grenade canonical paths, behavior binding, tool
  instance state).
- `--gameplay-boundary-selftest`: PASS, 0 FAILs, 3/3 repeated runs. This covers
  the phase-0 contract directly: NPC rocket spawns the canonical hot projectile,
  hot hitscan NPC tool deals damage, the generic handled gate is set, and hot
  melee NPC tool deals damage.
- `--tool-entity-continuity-selftest`: PASS, 0 FAILs.
- `--hot-authoritative-selftest`: PASS. `--live-code-selftest`: PASS.

## Human verification still required

- Run the game with this DLL and confirm `events.jsonl` shows the new records:
  `"event":"tool.action"` (action name in `result=action=...`) and
  `"event":"tool.rocket"` / `tool.grenade` / `tool.thrown`. They use category
  `WEAPONS` (already `important` in `config/debuglogger.json`).
- Confirm live editing of `src/hot-reload/modules/presentation/tool-visuals.cpp`
  (or a `hot*` param via params) changes behavior without an EXE rebuild.
- Play a match and confirm player hitscan/pellet revive the COLD path (hot
  hitscan has no `relationship.targets`, so it declines and the cold trace
  runs). Migrated player tools (rocket/grenade/thrown) go hot.

## Notes

- Runtime `events.jsonl` evidence was not captured: the headless selftests tear
  the structured logger down before the behavior sections run. The logging path
  is the same `log.event` capability proven by the collision changelog.
- `ak` and `sniper` are now registered as hot tools rather than JSON-only. If
  they were intentionally unavailable, restrict them via the existing loadout
  policy rather than removing the recipes.
- The phase-0 gate is the one cold change in this session; unchanged ABI. It
  makes future tools hot-only: a new recipe + behavior + `setOwnsExecution`
  needs no EXE edit.
- Phase 6 (delete the cold `WeaponBehaviorType` branches and retire
  `weapons.json` as a value source) remains outstanding.
- Residual risk: player-fired hit/pellet/melee tools currently decline the hot
  behavior (no relationship target) and use the cold path; that is intentional
  for this step but should be replaced by parity hot behaviors before phase 6.
- Pre-existing: a `--hot-combat-selftest` access violation was seen once with an
  intermediate `g000009` candidate; it did not reproduce on `g000010` (2/2 clean)
  and the same 24 pre-existing animation failures remain the only failures.
