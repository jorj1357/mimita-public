# Automatic NPC surface navigation

Date: 2026-10-04
EST timestamp: 2026-10-04 17:09:05 EST
Branch: `afad20a-rebuild`
HEAD at start: `74ccd6da bettter npc behavior now`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, pure-test, and in-binary runtime evidence are proven. No live
Counter-Strike map test (NPC below a ramp, human above the wall) was performed;
that human acceptance remains required.

## Scope

Implement the "Automatic NPC Surface Navigation" package on top of the existing
rolling local A* planner (`NpcNavigator`), without adding a second global
navigation system:

1. Actor-preset `navigation` settings (shared collision defaults).
2. Pure triangle classification (floor/ramp/step/too-steep/ceiling).
3. Multi-surface grid cells so stacked floors/bridges path correctly.
4. Edge-validated connections (a wall never becomes a straight route).
5. Policy-gated legal jump links and no wall jump.
6. Removal of the direct wall-pushing fallback; blocked/turn/retry instead.
7. Capability tags on path segments for future movement types.

## Pre-existing / external edits (NOT mine, NOT touched or claimed)

The working tree already contained large uncommitted edits from earlier
sessions (e.g. `src/npc/npc-behavior.*`, `src/npc/npc-perception.*`,
`src/npc/npc-state-machine.*`, `src/npc/npc-states.cpp`, `src/npc/npc.h`,
`src/npc/npc.cpp`, `config/npc-difficulty.json`, `config/weapons.json`,
`config/behavior-profiles.json`, `devscripts/dev-loop.py`). I did not revert or
claim them. `src/npc/npc.cpp` was already modified before this session; my only
edits there are `activeNavigationSettings` and the navigator call site (noted
below). `--actor-preset-selftest` still FAILs on the pre-existing revolver
damage assertion (working tree 35 vs the assertion's 100), independent of this
change.

A background dev-loop recompiled changed objects and relinked `mimita.exe`
after my edits. I did not force a cold build.

## Files changed

### New: `src/npc/npc-surface.h` / `.cpp`

- `NavSurfaceKind` (Floor/Ramp/Step/TooSteep/Ceiling) and
  `NavCapability` (Walk/Jump/Drop/Crawl/Fly/Roll/Teleport; only Walk/Jump/Drop
  produced today).
- `NavigationSurface` (position, normal, height, walkable, kind).
- `classifySurface(normal, walkableDot)` and `isWalkableNormal`.
- `navSurfaceKindName` / `navCapabilityName`.
- `npcSurfaceSelfTest` (pure, world-independent).

### New: `src/npc/npc-navigation-settings.h` / `.cpp`

- `NpcNavigationSettings` (mode, maxWalkableSlopeDot, automaticFromCollision,
  repathWhenBlocked, searchRadius, allowNavigationJumps, allowWallJump,
  blockedBehavior, maxStepHeight, wallProbeDistance).
- `parseNpcNavigationSettings` (strict enums/bools, clamped numbers,
  `max_walkable_slope_degrees` -> dot; absent = shared rule).
- `npcNavigationSettingsSelfTest`.

### `src/gamemode/match-roles.h` / `.cpp`

- `MatchRoleDefinition.navigationSettings`.
- `readActorPreset` parses the optional `navigation` block.
- `logNavigationSettings` emits one `[NPC NAV CFG]` line per configured preset.

### `src/npc/npc-internal.h`, `src/npc/npc.cpp`

- `activeNavigationSettings(npc)`: live-resolves the preset `navigation` block
  by `actorPresetId` (hot-reloadable), or nullptr for shared defaults.
- Navigator call site now passes `activeNavigationSettings(npc)` and `policy`.
- Traversal is only applied when `!nav.blocked`; a blocked route owns steering
  (`moveDir = nav.dir`) so traversal cannot launch a jump/dash into the wall.

### `src/npc/npc-navigator.h` / `.cpp` (core)

- `NpcNavResult` gains `blocked` and `capability`; `NpcNavigator` gains
  `pathCapability` and the `update(..., settings, policy)` overload.
- `gatherColumnSurfaces`: collects **all** walkable surfaces in a column
  (stacked floors), rejects a surface whose headroom is below the actor height
  (low ceiling or floor stacked too close).
- `segmentClear`: horizontal clearance with 3 offset rays; only steep faces
  block, so walkable ramps are not walls.
- `planLocalPath`: flattened per-(cell,surface) A*. Connections require an edge
  clearance test, drop/step/slope rules, and jump gating
  (`allowNavigationJumps && npcPolicyAllowsJump(policy, Navigation)`), with a
  landing-height clearance test unless `allow_wall_jump`. Continuous walkable
  ramps are walked even when the per-cell rise exceeds the step height (so
  `jump_style:"never"` can still use a ramp).
- `update`: no-route retry delay (`kBlockedRetryInterval`); the old
  unconditional direct-steer-to-destination fallback is replaced by a
  blocked test. If the direct line is blocked and no route exists, the actor
  turns toward the clearer side (`blocked_behavior` turn/turn_then_repath) or
  holds (`repath`) and never continuously pushes into the wall.

### `config/actor-presets/counter_strike.json`

- Added the `navigation` block (mode `automatic_surface_path`,
  `automatic_from_collision: true`, `repath_when_blocked: true`,
  `search_radius: 20.0`, `allow_navigation_jumps: true`, `allow_wall_jump:
  false`, `blocked_behavior: turn_then_repath`). `max_walkable_slope_degrees`
  is intentionally omitted so the shared collision rule (dot 0.80) is used and
  the planner can never route onto ground the body slides off.

### `src/game/game-cli.cpp`

- New `--npc-navigation-selftest` (real `NpcNavigator` on minimal collision
  worlds).

### New: `tests/npc-navigation-test.cpp`

- Standalone `check()` harness for classification and settings parsing,
  linking only `npc-surface.cpp` + `npc-navigation-settings.cpp`.

## Reasoning

The spec explicitly keeps the rolling local A* planner as the owner and asks to
improve it. The change therefore stays inside `NpcNavigator` plus a small,
pure settings/classification surface. Multi-surface cells and edge validation
fix the "fake route through a wall" and stacked-floor gaps; policy gating fixes
jump links a `jump_style:"never"` actor must not use; removing the direct
fallback fixes the reported repeated wall pushing.

## Spec review (`docs/skills/spec-behavior-review-v1.md`)

Result: no blocker findings.

- Severity: medium — spec-code disagreement on the slope default.
  - Specification: the example sets `max_walkable_slope_degrees: 45`; the shared
    physics rule is `MAX_WALKABLE_SLOPE_DOT = 0.80` (~36.9 degrees).
  - Resolution (human decision this session): default to the shared rule; the
    CS preset omits the override. A 45-degree planner limit would let the
    planner route onto ground the body treats as non-walkable.
- Severity: low — `search_radius` spec default 20.0 vs legacy window 12.5. The
  CS preset sets 20.0; non-preset actors keep 12.5. Clamped to [6, 20].
- No unresolved `NEEDS_SPEC_DECISION` items remain.

## Documents and skills

- Spec: user-provided "Automatic NPC Surface Navigation" (this session).
- `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md`.
- Skill: `docs/skills/spec-behavior-review-v1.md`.
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`.

## Validation

### Source / config

- New and edited files as listed. `counter_strike.json` parses (actor-preset
  selftest loads it); the `navigation` block is confirmed by the settings test.

### Build

- A background dev-loop compiled the changed objects and linked
  `mimita.exe` (17:05:43), after the last source edit (17:05:35). My
  `python build.py build-only` runs reported `Nothing changed` (objects already
  newer than sources), so no forced cold build was performed.

### Pure test

```text
build/npc-navigation-test.exe
[npc-navigation-test] PASS (34 checks)
```

### In-binary selftests (`mimita.exe`)

```text
[NPC NAVIGATION SELFTEST] PASS
  ok  surface classification (floor/ramp/steep/wall/ceiling)
  ok  navigation settings parse/default/clamp
  ok  routes around a wall
  ok  wall route detours sideways instead of through the wall
  ok  no route across a sealed wall
  ok  blocked route is reported
  ok  does not steer directly into the blocking wall
  ok  reaches a higher platform
  ok  the route uses a jump connection
  ok  jump_style never rejects the jump route
  ok  a wall above the jump cap is not a jump route
  ok  stacked floor reached via a ramp
  ok  route ends on the upper floor surface

[NPC POLICY SELFTEST] ... PASS
[NPC SEARCH SELFTEST] PASS
[NPC RADAR SELFTEST] PASS
[NPC NAV REQUEST SELFTEST] PASS
[ACCEPTANCE] all groups PASS (incl. npc-nav-request)
```

- `--actor-preset-selftest`: policy line correct; overall FAIL only on the
  pre-existing revolver-damage assertion (not caused by this change).

### Runtime / human

Not performed. The live Counter-Strike map test is still required.

## Human review still needed

1. Place an NPC below a ramp and stand above a wall; confirm the NPC detects
   the target above, stops pushing into the wall, turns toward the ramp, walks
   up the ramp, sees the target, and resumes pursuit/combat.
2. Confirm NPCs still navigate, pursue, and fight on `dust2cyberiav4` without
   regressions, and that they no longer repeatedly push confirmed walls.
3. Optionally edit the `navigation` block in `counter_strike.json`, save, and
   confirm living NPCs change without a restart (`[NPC NAV CFG]` logs a new
   revision).

## Known limitations / follow-ups

- Performance: multi-surface columns and edge rays are heavier than the old
  single-probe grid; the global plan token (16/s) bounds it. Profile if many
  NPCs path simultaneously.
- Future movement types (crawl/fly/roll/teleport) have capability tags but no
  behavior yet.
- Dynamic/destructible geometry is not re-planned specially.
